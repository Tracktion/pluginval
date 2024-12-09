/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "juce_core/juce_core.h"
#include "CrashHandler.h"

#if ! JUCE_WINDOWS
 #include "native/pluginval_native_posix.h"
 static const auto crashLogPath = "/tmp/pluginval_crash.txt";
#endif

namespace
{
    juce::String getCrashLogContents()
    {
        return "\n" + juce::SystemStats::getStackBacktrace();
    }

    static juce::File getCrashTraceFile()
    {
       #if JUCE_WINDOWS
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pluginval_crash.txt");
       #else
        return juce::File (crashLogPath);
       #endif
    }

   #if JUCE_WINDOWS
    static void handleCrash (void*)
    {
        const auto log = getCrashLogContents();
        std::cout << "\n*** FAILED: VALIDATION CRASHED\n" << log << std::endl;
        getCrashTraceFile().replaceWithText (log);
    }
   #else
    static void handleCrash (void*)
    {
        const char* header = "\n*** FAILED: VALIDATION CRASHED\n";
        write (STDERR_FILENO, header, strlen (header));

        writeStackTrace (crashLogPath, 2); // Skip handleCrash and juce::handleCrash)

        // Terminate normally to work around a bug in juce::ChildProcess::ActiveProcess::getExitStatus()
        // which returns 0 (a "pass" in the host process) if the child process terminates abnormally.
        // - https://github.com/Tracktion/pluginval/issues/125
        // - https://forum.juce.com/t/killed-childprocess-activeprocess-exit-code/61645/3
        // Use _Exit() instead of exit() so that static destructors don't run (they may not be async-signal-safe).
        // FIXME: exiting here prevents Apple's Crash Reporter from creating reports.
        std::_Exit (SIGKILL);
    }
  #endif
}

//==============================================================================
void initialiseCrashHandler()
{
    // Delete the crash file, this will be created if possible
    getCrashTraceFile().deleteFile();
    juce::SystemStats::setApplicationCrashHandler (handleCrash);
}

juce::String getCrashLog()
{
    const auto f = getCrashTraceFile();

    if (f.existsAsFile())
        return f.loadFileAsString();

    return getCrashLogContents();
}
