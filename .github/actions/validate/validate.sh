#!/usr/bin/env bash
# Validates each plugin in INPUT_PLUGINS (one per line: a path, a glob or an AU
# id), runs them all, and fails if any failed. See action.yml for the inputs.

set -u
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../common/common.sh"

resolve_pluginval

report_dir="${INPUT_REPORT_DIR:-$RUNNER_TEMP/pluginval-reports/validate}"
mkdir -p "$report_dir"

passed=0
failed=0
index=0
rows=""
details=""

# One plugin per line (not comma-separated: AU ids contain commas). Globs are
# left to expand; anything that matches nothing is passed through and fails.
IFS=$'\n'
plugins=( ${INPUT_PLUGINS:-} )
unset IFS

for plugin in ${plugins[@]+"${plugins[@]}"}; do
    plugin="$(printf '%s' "$plugin" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')"
    [ -z "$plugin" ] && continue

    index=$((index + 1))
    report="$report_dir/$index-$(sanitise "$(basename "$plugin")").xml"
    rm -f "$report"   # so an earlier run's report can't be read if this one writes none

    echo "::group::pluginval validate: $plugin"
    # INPUT_ARGS is deliberately unquoted so it splits into separate arguments.
    run_pluginval validate --strictness-level "${INPUT_STRICTNESS_LEVEL:-5}" --junit-xml "$report" ${INPUT_ARGS:-} "$plugin"
    code=$?
    echo "::endgroup::"
    failure="pluginval exited with code $code (see the log)"

    # The per-test results come from the JUnit report; pass / fail from the exit code.
    read_report "$report"

    if [ $code -eq 0 ]; then
        passed=$((passed + 1))
        rows="$rows| \`$plugin\` | :white_check_mark: passed | $REPORT_TESTS |"$'\n'
    else
        failed=$((failed + 1))
        rows="$rows| \`$plugin\` | :x: failed | $REPORT_TESTS |"$'\n'
        [ -z "$REPORT_DETAILS" ] && REPORT_DETAILS="  - $failure"$'\n'
        details="$details- \`$plugin\`"$'\n'"$REPORT_DETAILS"
        annotate_error "pluginval validation failed: $plugin" "${REPORT_NAMES:-$failure}"
    fi
done

if [ $index -eq 0 ]; then
    echo "::error::No plugins to validate (the 'plugins' input is empty)"
    exit 1
fi

{
    echo "### pluginval validate"
    echo
    echo "| Plugin | Result | Tests |"
    echo "| --- | --- | --- |"
    printf '%s' "$rows"

    if [ -n "$details" ]; then
        echo
        echo "#### Failed tests"
        echo
        printf '%s' "$details"
    fi
} >> "$GITHUB_STEP_SUMMARY"

{
    echo "passed=$passed"
    echo "failed=$failed"
    echo "report-dir=$report_dir"
} >> "$GITHUB_OUTPUT"

echo "pluginval validate: $passed passed, $failed failed"
[ $failed -eq 0 ]
