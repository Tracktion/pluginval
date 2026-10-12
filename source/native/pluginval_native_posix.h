/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#pragma once

#if __has_include(<unistd.h>)

#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#ifdef __printflike
 __printflike(2, 3)
#endif

/** Writes a string to a file descriptor in a async-signal-safe way. I.e. no allocations etc. */
inline void writeAsyncSignalSafe (int fd, const char* fmt, ...)
{
    char buf[1024];

    va_list args;
    va_start (args, fmt);
    // Warning: printf is not 100% async-signal-safe, but should be ok for locale-independent arguments like
    // integers, strings, hex... floating point is locale-dependent and not safe to use here.
    vsnprintf (buf, sizeof (buf), fmt, args);
    va_end (args);

    auto len = strlen (buf);
    [[ maybe_unused]] auto r = write (STDERR_FILENO, buf, len);

    if (fd != -1)
        [[ maybe_unused]] auto r2 = write (fd, buf, len);
}

/** Writes the current stack trace and images to a given filepath and stderr.
*/
inline void writeStackTrace (const char* filePath)
{
    // On Linux & Mac this is a signal handler, and therefore only "async-signal-safe" functions should be used.
    // This means nothing that uses malloc (juce::File, juce::String, std::string, std::vector etc.) or buffered I/O.
    int fd = open (filePath, O_RDWR | O_CREAT | O_TRUNC, 00644);

    // Write stack traces and images
    {
        // Recreate the output of backtrace_symbols(), which cannot be used in a
        // signal handler because it uses malloc
        constexpr int maxNumStacks = 128;
        void *stacktrace[maxNumStacks]{};
        int stackCount = backtrace(stacktrace, maxNumStacks);

        constexpr int maxNumImages = 64;
        const void *imageAddresses[maxNumImages]{};
        const char *imageNames[maxNumImages]{};
        int imageCount = 0;

        for (int i = 0; i < stackCount; ++i)
        {
            Dl_info info{};

            // Warning: dladdr can deadlock under rare conditions on macOS - if dyld
            // is adding an image to its list
            if (! dladdr (stacktrace[i], &info))
            {
                writeAsyncSignalSafe (fd, "%-3d %-35s %p\n", i, "", stacktrace[i]);
                continue;
            }

            const char* imageName = info.dli_fname ? strrchr (info.dli_fname, '/') : nullptr;

            if (imageName)
            {
                ++imageName;

                auto it = std::find (std::begin (imageAddresses), std::end (imageAddresses), info.dli_fbase);

                if (it == std::end (imageAddresses) && imageCount < maxNumImages)
                {
                    imageAddresses[imageCount] = info.dli_fbase;
                    imageNames[imageCount] = imageName;
                    ++imageCount;
                }
            }

            if (info.dli_saddr)
            {
                ptrdiff_t offset = static_cast<char*> (stacktrace[i]) - static_cast<char*> (info.dli_saddr);
                writeAsyncSignalSafe (fd, "%-3d %-35s %p %s + %ld\n", i, imageName, stacktrace[i], info.dli_sname,
                                      offset);
            }
            else
            {
                writeAsyncSignalSafe (fd, "%-3d %-35s %p\n", i, imageName, stacktrace[i]);
            }
        }

        if (imageCount > 0)
        {
            writeAsyncSignalSafe (fd, "\nBinary Images:");

            for (int i = 0; i < imageCount; i++)
                writeAsyncSignalSafe (fd, "\n%p %s", imageAddresses[i], imageNames[i]);

            writeAsyncSignalSafe (fd, "\n");
        }
    }

    if (fd != -1)
        close (fd);
}

 #endif //__has_include(<unistd.h>)