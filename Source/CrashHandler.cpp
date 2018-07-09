/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "CrashHandler.h"

#if JUCE_MAC
 #include <dlfcn.h>
#endif

namespace
{
    String getExtraPlatformSpecificInfo()
    {
       #if JUCE_MAC
        StringArray imagesDone;
        StringArray output;

        for (auto& l : StringArray::fromLines (SystemStats::getStackBacktrace()))
        {
            const String imageName = l.upToFirstOccurrenceOf ("0x", false, false).fromFirstOccurrenceOf (" ", false, false).trim();
            const String addressString = l.fromFirstOccurrenceOf ("0x", true, false).upToFirstOccurrenceOf (" ", false, false).trim();

            if (imagesDone.contains (imageName))
                continue;

            imagesDone.add (imageName);

            Dl_info info;
            const size_t address = static_cast<size_t> (addressString.getHexValue64());

            if (dladdr (reinterpret_cast<void*> (address), &info) != 0)
                output.add (String ("0x") + String::toHexString (static_cast<pointer_sized_int> (reinterpret_cast<size_t> (info.dli_fbase))) + " " + imageName);
        }

        return "Binary Images:\n" + output.joinIntoString ("\n");
       #else
        return {};
       #endif
    }

    String getCrashLogContents()
    {
        return "\n" + SystemStats::getStackBacktrace() + "\n" + getExtraPlatformSpecificInfo();
    }

    static File getCrashTraceFile()
    {
        return File::getSpecialLocation (File::tempDirectory).getChildFile ("pluginval_crash.txt");
    }

    static void handleCrash (void*)
    {
        const auto log = getCrashLogContents();
        std::cout << "\n*** FAILED: VALIDATION CRASHED\n" << log << std::endl;
        getCrashTraceFile().replaceWithText (log);
    }
}

//==============================================================================
void initialiseCrashHandler()
{
    // Delete the crash file, this will be created if possible
    getCrashTraceFile().deleteFile();
    SystemStats::setApplicationCrashHandler (handleCrash);
}

String getCrashLog()
{
    const auto f = getCrashTraceFile();

    if (f.existsAsFile())
        return f.loadFileAsString();

    return getCrashLogContents();
}
