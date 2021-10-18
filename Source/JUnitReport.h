#pragma once

#include <JuceHeader.h>

namespace JUnitReport
{

bool write(const HashMap<String, Array<UnitTestRunner::TestResult> > &allResults, File &output);

} // namespace JUnitReport
