#pragma once

#include <JuceHeader.h>
#include "PluginTestResult.h"

namespace JUnitReport
{

bool write(const HashMap<String, PluginTestResultArray> &allResults, File &output);

} // namespace JUnitReport
