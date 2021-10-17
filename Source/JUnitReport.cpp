#include "JUnitReport.h"

//<?xml version="1.0" encoding="UTF-8" ?>
//   <testsuites id="20140612_170519" name="New_configuration (14/06/12 17:05:19)" tests="225" failures="1262" time="0.001">
//      <testsuite id="codereview.cobol.analysisProvider" name="COBOL Code Review" tests="45" failures="17" time="0.001">
//         <testcase id="codereview.cobol.rules.ProgramIdRule" name="Use a program name that matches the source file name" time="0.001">
//            <failure message="PROGRAM.cbl:2 Use a program name that matches the source file name" type="WARNING">
//WARNING: Use a program name that matches the source file name
//Category: COBOL Code Review – Naming Conventions
//File: /project/PROGRAM.cbl
//Line: 2
//      </failure>
//    </testcase>
//  </testsuite>
//</testsuites>

namespace
{

String stringToId(const String& s)
{
    if (s.isEmpty())
    {
        return "-";
    }

    auto firstChar = s[0];
    bool validFirstChar =
            (firstChar >= 'a' && firstChar <= 'z') ||
            (firstChar >= 'A' && firstChar <= 'Z');

    String id = validFirstChar ? "" : "-";
    for (auto it = s.begin(); it != s.end(); ++it)
    {
        auto c = *it;
        bool valid =
                (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == ':' || c == '.';
        id += valid ? c : '-';
    }
    return id.toLowerCase();
}


XmlElement* createTestCase(const UnitTestRunner::TestResult& r)
{
    auto testcase = new XmlElement("testcase");

    //testcase->setAttribute("id", stringToId(r.unitTestName + "." + r.subcategoryName));
    testcase->setAttribute("name", r.subcategoryName);

    auto duration = (r.endTime - r.startTime).inMilliseconds();
    testcase->setAttribute("time", duration / 1000.0);

    for (const auto& m: r.messages)
    {
        auto failure = new XmlElement("failure");
        failure->setAttribute("type", "ERROR");
        failure->setAttribute("message", m);

        testcase->prependChildElement(failure);
    }
    return testcase;
}

}

bool JUnitReport::write(const HashMap<String, Array<UnitTestRunner::TestResult>> &allResults, File &output)
{
    XmlElement testsuites("testsuites");

    int gfailures = 0;
    int64 gtime = 0;
    int gcount = 0;
    for (auto it = allResults.begin(); it != allResults.end(); ++it)
    {
        const auto& results = it.getValue();
        auto pluginName = File(it.getKey()).getFileName();

        auto testsuite = new XmlElement("testsuite");

        int failures = 0;
        int passes = 0;
        int64 time = 0;
        int count = 0;

        for (const auto& r: results)
        {
            auto testcase = createTestCase(r);
            // add test case to test suite
            testsuite->prependChildElement(testcase);

            // calculate totals for test suite
            failures += r.failures;
            passes += r.passes;
            time += (r.endTime - r.startTime).inMilliseconds();

            // increment count
            count++;
        }

        //testsuite->setAttribute("id", stringToId("pluginval." + SystemStats::getOperatingSystemName() + "." + pluginName));
        testsuite->setAttribute("name", "pluginval of " + pluginName + " on " + SystemStats::getOperatingSystemName());
        testsuite->setAttribute("tests", count);
        testsuite->setAttribute("failures", failures);
        testsuite->setAttribute("time", time / 1000.0);

        // accumulate totals for all test suites
        gfailures += failures;
        gtime += time;
        gcount += count;

        testsuites.prependChildElement(testsuite);
    }

    testsuites.setAttribute("id", "plugins");
    testsuites.setAttribute("name", "Plugin validations");
    testsuites.setAttribute("tests", gcount);
    testsuites.setAttribute("failures", gfailures);
    testsuites.setAttribute("time", gtime / 1000.0);

    return testsuites.writeTo(output);
}
