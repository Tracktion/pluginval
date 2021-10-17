#include "JUnitReport.h"

namespace
{

XmlElement* createTestCase(const String& pluginName, const UnitTestRunner::TestResult& r)
{
    auto testcase = new XmlElement("testcase");

    testcase->setAttribute("name", r.subcategoryName);
    testcase->setAttribute("classname", r.unitTestName);
    testcase->setAttribute("file", pluginName);

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

    int total_failures = 0;
    int64 total_time = 0;
    int total_count = 0;
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
            auto testcase = createTestCase(pluginName, r);
            // add test case to test suite
            testsuite->prependChildElement(testcase);

            // calculate totals for test suite
            failures += r.failures;
            passes += r.passes;
            time += (r.endTime - r.startTime).inMilliseconds();

            // increment count
            count++;
        }

        testsuite->setAttribute("package", "pluginval");
        testsuite->setAttribute("name", "pluginval of " + pluginName + " on " + SystemStats::getOperatingSystemName());
        testsuite->setAttribute("tests", count);
        testsuite->setAttribute("failures", failures);
        testsuite->setAttribute("time", time / 1000.0);

        testsuites.prependChildElement(testsuite);

        // accumulate totals for all test suites
        total_failures += failures;
        total_time += time;
        total_count += count;
    }

    testsuites.setAttribute("name", "pluginval test suites");
    testsuites.setAttribute("tests", total_count);
    testsuites.setAttribute("failures", total_failures);
    testsuites.setAttribute("time", total_time / 1000.0);

    return testsuites.writeTo(output);
}
