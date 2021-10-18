#include "JUnitReport.h"

namespace JUnitReport
{

XmlElement* createTestCaseElement(const String& pluginName, const UnitTestRunner::TestResult& r)
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

XmlElement* createTestSuiteElement(const String& pluginName)
{
    auto testsuite = new XmlElement("testsuite");
    testsuite->setAttribute("package", "pluginval");
    testsuite->setAttribute("name", "pluginval of " + pluginName + " on " + SystemStats::getOperatingSystemName());
    return testsuite;
}

void addTestsStats(XmlElement* element, int tests, int failures, int64 duration)
{
    element->setAttribute("tests", tests);
    element->setAttribute("failures", failures);
    element->setAttribute("time", duration / 1000.0);
}

bool write(const HashMap<String, Array<UnitTestRunner::TestResult>> &allResults, File &output)
{
    XmlElement testsuites("testsuites");
    testsuites.setAttribute("name", "pluginval test suites");

    int total_failures = 0;
    int total_tests = 0;
    int64 total_duration = 0;
    for (auto it = allResults.begin(); it != allResults.end(); ++it)
    {
        const auto results = it.getValue();
        auto pluginName = File(it.getKey()).getFileName();

        int suite_failures = 0;
        int64 suite_duration = 0;
        int suite_tests = results.size();

        auto testsuite = createTestSuiteElement(pluginName);

        for (const auto& r: results)
        {
            auto testcase = createTestCaseElement(pluginName, r);
            // add test case to test suite
            testsuite->prependChildElement(testcase);

            // calculate totals for test suite
            suite_failures += r.failures;
            suite_duration += (r.endTime - r.startTime).inMilliseconds();
        }

        addTestsStats(testsuite, suite_tests, suite_failures, suite_duration);

        testsuites.prependChildElement(testsuite);

        // accumulate totals for all test suites
        total_failures += suite_failures;
        total_duration += suite_duration;
        total_tests += suite_tests;
    }

    addTestsStats(&testsuites, total_tests, total_failures, total_duration);

    return testsuites.writeTo(output);
}

} // namespace JUnitReport
