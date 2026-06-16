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

    Must be called on the message thread (it uses the VST3-safe lifecycle
    helpers and creates the plugin instance directly). Throws std::runtime_error
    on any setup / render failure.
*/
RenderedAudio renderPlugin (const TestConfig&);

//==============================================================================
/** Runs a single resolved config end-to-end: render, then record-or-compare,
    producing a TestResult. Never throws - setup failures become an error result. */
TestResult runTest (const TestConfig&);

/** Loads the config file (first entry only for v1), runs it, reports the result
    to stdout and returns the process exit code (0 success, 1 failure).

    Must be called on the message thread. */
int runTestFile (const juce::File& configFile);

} // namespace acceptance
