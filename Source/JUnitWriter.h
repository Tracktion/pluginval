#pragma once

#include <JuceHeader.h>

class JUnitWriter
{
public:
    ~JUnitWriter() = default;

    static bool write(const HashMap<String, Array<UnitTestRunner::TestResult> > &allResults, File &output);

private:
    JUnitWriter() = default;
};
