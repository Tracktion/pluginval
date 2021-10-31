#pragma once

#include <JuceHeader.h>
#include "CommonTypes.h"

namespace JUnitReport
{

bool write(const HashMap<String, UnitTestResultsWithOutput> &allResults, File &output);

} // namespace JUnitReport
