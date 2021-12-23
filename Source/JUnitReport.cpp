#include "JUnitReport.h"

namespace JUnitReport
{

XmlElement* createTestCaseElement(const String& pluginName, int testIndex, const PluginTestResult& r)
{
    auto testcase = new XmlElement("testcase");

    testcase->setAttribute("classname", r.result.unitTestName);

    // Adding test index here to have unique name for each test case execution
#if defined(USE_FILENAME_IN_JUNIT_REPORT)
    testcase->setAttribute("name", "Test " + String(testIndex + 1) + ": " + r.subcategoryName);
    testcase->setAttribute("file", pluginName);
#else
    testcase->setAttribute("name", "Test " + String(testIndex + 1) + ": " + r.result.subcategoryName + " of " + pluginName);
#endif

    auto duration = (r.result.endTime - r.result.startTime).inMilliseconds() / 1000.0;
    testcase->setAttribute("time", duration);

    String output = r.output.joinIntoString("\n");
    if (r.result.failures == 0)
    {
        auto system_out = new XmlElement("system-out");
        system_out->addTextElement(output);

        testcase->prependChildElement(system_out);
    }
    else
    {
        auto failure = new XmlElement("failure");
        failure->setAttribute("type", "ERROR");
        failure->setAttribute("message", r.result.messages.joinIntoString(" "));
        failure->addTextElement(output);

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

bool write(const HashMap<String, PluginTestResultArray> &allResults, File &output)
{
    XmlElement testsuites("testsuites");
    testsuites.setAttribute("name", "pluginval test suites");

    int total_failures = 0;
    int total_tests = 0;
    int64 total_duration = 0;
    int test_index = 0;
    for (auto it = allResults.begin(); it != allResults.end(); ++it)
    {
        const auto results = it.getValue();
        auto pluginName = it.getKey();

        int suite_failures = 0;
        int64 suite_duration = 0;
        int suite_tests = results.size();

        auto testsuite = createTestSuiteElement(pluginName);

        for (const auto& r: results)
        {
            auto testcase = createTestCaseElement(pluginName, test_index, r);
            testsuite->prependChildElement(testcase);

            // calculate totals for test suite
            suite_failures += r.result.failures;
            suite_duration += (r.result.endTime - r.result.startTime).inMilliseconds();

            test_index++;
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
