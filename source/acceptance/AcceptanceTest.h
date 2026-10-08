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

#include "TestConfig.h"
#include "TestReporter.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>

namespace acceptance
{

//==============================================================================
/** A rendered buffer plus the metadata needed for the reference manifest. */
struct RenderedAudio
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 0.0;
    int blockSize = 0;
    juce::PluginDescription description;
};

//==============================================================================
/** Loads the plugin, applies state (file then parameters), feeds the input
    (audio / MIDI / silence) and renders a fixed duration into a single buffer.

    Intended to be called off the message thread (see TestRunner): VST3 lifecycle
    calls and the plugin's destruction are marshalled onto the message thread,
    which must be free to service them. Throws std::runtime_error on any setup /
    render failure.
*/
RenderedAudio renderPlugin (const TestConfig&);

//==============================================================================
/** Options from the "pluginval test" command line. */
struct RunOptions
{
    bool recordMissing = false;    /**< Record a missing reference and pass, rather than failing. */
    juce::int64 timeoutMs = 30000; /**< Abort the whole run after this long. <= 0 never times out. */
};

//==============================================================================
/** Runs a single resolved config end-to-end: render, then record-or-compare,
    producing a TestResult. Never throws - setup failures become an error result. */
TestResult runTest (const TestConfig&, const RunOptions&);

/** Loads the config file (first entry only for v1), runs it, reports the result
    to stdout and returns the process exit code (0 success, 1 failure). */
int runTestFile (const juce::File& configFile, const RunOptions&);

//==============================================================================
/** Runs runTestFile() on a background thread so the message thread stays free
    to service the plugin (VST3 lifecycle calls, async creation, posted work).

    onComplete is called on the message thread with the exit code. If the run
    exceeds the timeout, a watchdog reports the failure and terminates the
    process with exit code 1, since a hung plugin call can't be interrupted.
*/
class TestRunner  : private juce::Thread
{
public:
    TestRunner (const juce::File& configFile, const RunOptions&, std::function<void (int)> onComplete);
    ~TestRunner() override;

private:
    struct Watchdog;

    const juce::File configFile;
    const RunOptions options;
    std::function<void (int)> onComplete;
    std::unique_ptr<Watchdog> watchdog;

    void run() override;
};

} // namespace acceptance
