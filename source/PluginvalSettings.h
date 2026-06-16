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

#include "PluginTests.h"
#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

//==============================================================================
/** Maps the RealtimeCheck enum to/from its JSON string representation.
    An unrecognised/empty string deserialises to the first entry (disabled),
    preserving the historical "empty -> disabled" behaviour.
*/
NLOHMANN_JSON_SERIALIZE_ENUM (RealtimeCheck,
{
    { RealtimeCheck::disabled, "disabled" },
    { RealtimeCheck::enabled,  "enabled"  },
    { RealtimeCheck::relaxed,  "relaxed"  },
})

//==============================================================================
/**
    The single, parser-agnostic settings struct.

    Every field uses std types so it can be (de)serialised by nlohmann/json and
    is decoupled from JUCE. JSON keys mirror the field names below. Conversion to
    the JUCE-flavoured PluginTests::Options happens only at the boundary via
    toPluginTestOptions().
*/
struct PluginvalSettings
{
    int strictnessLevel = 5;
    std::int64_t randomSeed = 0;
    std::int64_t timeoutMs = 30000;
    bool verbose = false;
    int numRepeats = 1;
    bool randomiseTestOrder = false;
    bool skipGuiTests = false;
    std::string dataFile;
    std::string outputDir;
    std::string outputFilename;
    std::vector<std::string> disabledTests;
    std::vector<double> sampleRates;
    std::vector<int> blockSizes;
    RealtimeCheck realtimeCheck = RealtimeCheck::disabled;
    std::string validatePath;

    bool operator== (const PluginvalSettings&) const = default;

    //==============================================================================
    /** The default sample rates used when none are specified. */
    static std::vector<double> defaultSampleRates()  { return { 44100.0, 48000.0, 96000.0 }; }
    /** The default block sizes used when none are specified. */
    static std::vector<int>    defaultBlockSizes()   { return { 64, 128, 256, 512, 1024 }; }

    //==============================================================================
    /** Converts to the JUCE-flavoured options consumed by PluginTests. */
    PluginTests::Options toPluginTestOptions() const
    {
        PluginTests::Options o;
        o.strictnessLevel    = juce::jlimit (1, 10, strictnessLevel);
        o.randomSeed         = randomSeed;
        o.timeoutMs          = timeoutMs;
        o.verbose            = verbose;
        o.numRepeats         = juce::jmax (1, numRepeats);
        o.randomiseTestOrder = randomiseTestOrder;
        o.withGUI            = ! skipGuiTests;
        o.dataFile           = juce::String (dataFile);
        o.outputDir          = juce::String (outputDir);
        o.outputFilename     = juce::String (outputFilename);
        o.disabledTests      = toStringArray (disabledTests);
        o.sampleRates        = sampleRates.empty() ? defaultSampleRates() : sampleRates;
        o.blockSizes         = blockSizes.empty()  ? defaultBlockSizes()  : blockSizes;
        o.realtimeCheck      = realtimeCheck;
        return o;
    }

    /** Builds settings from a resolved PluginTests::Options + plugin path.
        Used to serialise options for the child validation process.
    */
    static PluginvalSettings fromPluginTestOptions (const PluginTests::Options& o, const juce::String& fileOrID)
    {
        PluginvalSettings s;
        s.strictnessLevel    = o.strictnessLevel;
        s.randomSeed         = o.randomSeed;
        s.timeoutMs          = o.timeoutMs;
        s.verbose            = o.verbose;
        s.numRepeats         = o.numRepeats;
        s.randomiseTestOrder = o.randomiseTestOrder;
        s.skipGuiTests       = ! o.withGUI;
        s.dataFile           = o.dataFile.getFullPathName().toStdString();
        s.outputDir          = o.outputDir.getFullPathName().toStdString();
        s.outputFilename     = o.outputFilename.toStdString();
        s.disabledTests      = fromStringArray (o.disabledTests);
        s.sampleRates        = o.sampleRates;
        s.blockSizes         = o.blockSizes;
        s.realtimeCheck      = o.realtimeCheck;
        s.validatePath       = fileOrID.toStdString();
        return s;
    }

private:
    static juce::StringArray toStringArray (const std::vector<std::string>& v)
    {
        juce::StringArray a;
        for (const auto& s : v)
            a.add (juce::String (s));
        return a;
    }

    static std::vector<std::string> fromStringArray (const juce::StringArray& a)
    {
        std::vector<std::string> v;
        for (const auto& s : a)
            v.push_back (s.toStdString());
        return v;
    }
};

//==============================================================================
// to_json/from_json with per-field defaults: keys missing from the JSON fall
// back to a default-constructed PluginvalSettings rather than value-zero. This
// makes "defaults" a free layer and lets sparse JSON layers merge cleanly.
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT (PluginvalSettings,
    strictnessLevel, randomSeed, timeoutMs, verbose, numRepeats,
    randomiseTestOrder, skipGuiTests, dataFile, outputDir, outputFilename,
    disabledTests, sampleRates, blockSizes, realtimeCheck, validatePath)
