#pragma once

#include <JuceHeader.h>

struct PluginTestResult
{
    PluginTestResult(const UnitTestRunner::TestResult &result, const StringArray &output):
        result(result), output(output)
    {
    }

    UnitTestRunner::TestResult result;
    StringArray output;
};

using PluginTestResultArray = Array<PluginTestResult>;

