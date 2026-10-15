# Summarises a pluginval JUnit XML report (--junit-xml) as tab-separated lines:
#   T <tests> <failed>          the totals (failures + errors)
#   F <test name> <message>     one per failed test
# Written for POSIX awk (macOS ships BSD awk), so no gawk extensions.

function attr(s, key,    start, rest)
{
    start = index(s, " " key "=\"")
    if (start == 0) return ""
    rest = substr(s, start + length(key) + 3)
    return substr(rest, 1, index(rest, "\"") - 1)
}

function unescape(s)
{
    gsub(/&#1[03];/, " ", s)
    gsub(/&lt;/, "<", s)
    gsub(/&gt;/, ">", s)
    gsub(/&quot;/, "\"", s)
    gsub(/&apos;/, "\047", s)
    gsub(/&amp;/, "\\&", s)
    gsub(/[\t ]+/, " ", s)
    return s
}

{ sub(/\r$/, ""); xml = xml " " $0 }

END {
    count = split(xml, cases, "<testcase ")
    printf "T\t%d\t%d\n", attr(cases[1], "tests"), attr(cases[1], "failures") + attr(cases[1], "errors")

    for (i = 2; i <= count; ++i)
    {
        problem = index(cases[i], "<failure")
        if (problem == 0) problem = index(cases[i], "<error")
        if (problem == 0) continue

        message = unescape(attr(substr(cases[i], problem), "message"))
        if (length(message) > 300) message = substr(message, 1, 300) "..."
        printf "F\t%s\t%s\n", unescape(attr(" " cases[i], "name")), message
    }
}
