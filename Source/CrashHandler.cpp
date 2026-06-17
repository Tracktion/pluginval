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

#if !JUCE_WINDOWS
 #include <dlfcn.h>
 #include <execinfo.h>
 #include <fcntl.h>
 #include <stdarg.h>
 #include <stdio.h>
 #include <unistd.h>
 #define CRASH_LOG "/tmp/pluginval_crash.txt"
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
        return juce::File(CRASH_LOG);
      #endif
    }

  #if JUCE_WINDOWS
    static void handleCrash(void*)
    {
        const auto log = getCrashLogContents();
        std::cout << "\n*** FAILED: VALIDATION CRASHED\n" << log << std::endl;
        getCrashTraceFile().replaceWithText (log);
    }
  #else
    #ifdef __printflike
      __printflike(2, 3)
    #endif
    static void writeToCrashLog(int fd, const char* fmt, ...)
    {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        // Warning: printf is not 100% async-signal-safe, but should be ok for locale-independent arguments like
        // integers, strings, hex... floating point is locale-dependent and not safe to use here.
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        auto len = strlen(buf);
        write(STDERR_FILENO, buf, len);
        if (fd != -1)
            write(fd, buf, len);
    }

    static void handleCrash(void*)
    {
        // On Linux & Mac this is a signal handler, and therefore only "async-signal-safe" functions should be used.
        // This means nothing that uses malloc (juce::File, juce::String, std::string, std::vector etc.) or buffered I/O.

        int fd = open(CRASH_LOG, O_RDWR | O_CREAT | O_TRUNC);

        const char *message = "\n*** FAILED: VALIDATION CRASHED\n";
        write(STDERR_FILENO, message, strlen(message));

        // Recreate the output of backtrace_symbols(), which cannot be used in a signal handler because it uses malloc
        static const int kMaxStacks = 128;
        void *stacktrace[kMaxStacks] {};
        int stackCount = backtrace(stacktrace, kMaxStacks);

        static const int kMaxImages = 64;
        const void *imageAddresses[kMaxImages] {};
        const char *imageNames[kMaxImages] {};
        int imageCount = 0;

        int skip = 2; // Skip handleCrash and juce::handleCrash)
        for (int i = skip; i < stackCount; i++)
        {
            Dl_info info {};
            // Warning: dladdr can deadlock under rare conditions on macOS - if dyld is adding an image to its list
            if (!dladdr(stacktrace[i], &info))
            {
                writeToCrashLog(fd, "%-3d %-35s %p\n", i - skip, "", stacktrace[i]);
                continue;
            }

            const char *imageName = info.dli_fname ? strrchr(info.dli_fname, '/') : nullptr;
            if (imageName)
            {
                imageName++;

                auto it = std::find(std::begin(imageAddresses), std::end(imageAddresses), info.dli_fbase);
                if (it == std::end(imageAddresses) && imageCount < kMaxImages)
                {
                    imageAddresses[imageCount] = info.dli_fbase;
                    imageNames[imageCount] = imageName;
                    imageCount++;
                }
            }

            if (info.dli_saddr)
            {
                ptrdiff_t offset = (char *)stacktrace[i] - (char *)info.dli_saddr;
                writeToCrashLog(fd, "%-3d %-35s %p %s + %ld\n", i - skip, imageName, stacktrace[i], info.dli_sname, offset);
            }
            else
            {
                writeToCrashLog(fd, "%-3d %-35s %p\n", i - skip, imageName, stacktrace[i]);
            }
        }

        if (imageCount)
        {
            writeToCrashLog(fd, "\nBinary Images:");
            for (int i = 0; i < imageCount; i++)
                writeToCrashLog(fd, "\n%p %s", imageAddresses[i], imageNames[i]);
            writeToCrashLog(fd, "\n");
        }

        if (fd != -1)
            close(fd);

        // Terminate normally to work around a bug in juce::ChildProcess::ActiveProcess::getExitStatus()
        // which returns 0 (a "pass" in the host process) if the child process terminates abnormally.
        // - https://github.com/Tracktion/pluginval/issues/125
        // - https://forum.juce.com/t/killed-childprocess-activeprocess-exit-code/61645/3
        // Use _Exit() instead of exit() so that static destructors don't run (they may not be async-signal-safe).
        // FIXME: exiting here prevents Apple's Crash Reporter from creating reports.
        std::_Exit(SIGKILL);
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
