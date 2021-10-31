#include "JUnitReport.h"

namespace JUnitReport
{

XmlElement* createTestCaseElement(const String& pluginName, const std::pair<UnitTestRunner::TestResult, StringArray>& r)
{
    auto testcase = new XmlElement("testcase");

    testcase->setAttribute("classname", r.first.unitTestName);
#if defined(USE_FILENAME_IN_JUNIT_REPORT)
    testcase->setAttribute("name", r.subcategoryName);
    testcase->setAttribute("file", pluginName);
#else
    testcase->setAttribute("name", r.first.subcategoryName + " of " + pluginName);
#endif

    auto duration = (r.first.endTime - r.first.startTime).inMilliseconds();
    testcase->setAttribute("time", duration / 1000.0);

    if (r.first.messages.isEmpty())
    {
        // Passed test case
        auto output = new XmlElement("system-out");
        output->addTextElement(r.second.joinIntoString("\n"));

        testcase->prependChildElement(output);
    }
    else
    {
        // Failing test case
        auto failure = new XmlElement("failure");
        failure->setAttribute("type", "ERROR");
        failure->setAttribute("message", r.first.messages.joinIntoString(" "));
        failure->addTextElement(r.second.joinIntoString("\n"));

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

bool write(const HashMap<String, UnitTestResultsWithOutput> &allResults, File &output)
{
    XmlElement testsuites("testsuites");
    testsuites.setAttribute("name", "pluginval test suites");

    int total_failures = 0;
    int total_tests = 0;
    int64 total_duration = 0;
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
            auto testcase = createTestCaseElement(pluginName, r);
            // add test case to test suite
            testsuite->prependChildElement(testcase);

            // calculate totals for test suite
            suite_failures += r.first.failures;
            suite_duration += (r.first.endTime - r.first.startTime).inMilliseconds();
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
