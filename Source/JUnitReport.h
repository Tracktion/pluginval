#pragma once

#include <JuceHeader.h>

class JUnitReport
{
public:
    ~JUnitReport() = default;

    static bool write(const HashMap<String, Array<UnitTestRunner::TestResult> > &allResults, File &output);

private:
    JUnitReport() = default;
};
