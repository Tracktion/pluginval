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

#include <magic_args/magic_args.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <optional>
#include <expected>

//==============================================================================
/** Custom type for comma-separated double values (e.g. sample rates). */
struct CommaSeparatedDoubles
{
    std::vector<double> values;
};

void from_string_argument (CommaSeparatedDoubles& out, std::string_view arg);
std::string formattable_argument_value (const CommaSeparatedDoubles& v);

//==============================================================================
/** Custom type for comma-separated int values (e.g. block sizes). */
struct CommaSeparatedInts
{
    std::vector<int> values;
};

void from_string_argument (CommaSeparatedInts& out, std::string_view arg);
std::string formattable_argument_value (const CommaSeparatedInts& v);

//==============================================================================
/** Custom type for hex-or-decimal seed parsing (e.g. "0x7f2da1" or "1234"). */
struct HexOrDecimalSeed
{
    int64_t value = 0;
};

void from_string_argument (HexOrDecimalSeed& out, std::string_view arg);
std::string formattable_argument_value (const HexOrDecimalSeed& v);

//==============================================================================
/** ADL hooks for RealtimeCheck enum so magic_args can parse it. */
void from_string_argument (RealtimeCheck& out, std::string_view arg);
std::string formattable_argument_value (const RealtimeCheck& v);

//==============================================================================
/** The magic_args parse target for CLI arguments.
    Member naming uses mCamelCase which magic_args converts to --kebab-case.
*/
struct PluginvalCliArgs
{
    magic_args::option<int> mStrictnessLevel {
        .mValue = 5,
        .mHelp = "Strictness level 1-10 (default: 5). Higher levels include longer, more thorough tests.",
    };

    magic_args::option<HexOrDecimalSeed> mRandomSeed {
        .mHelp = "Random seed (hex 0x... or decimal). 0 = auto.",
    };

    magic_args::option<int64_t> mTimeoutMs {
        .mValue = 30000,
        .mHelp = "Timeout in ms. -1 = no timeout.",
    };

    bool mVerbose { false };

    magic_args::option<int> mRepeat {
        .mValue = 1,
        .mHelp = "Number of times to repeat tests.",
    };

    bool mRandomise { false };
    bool mSkipGuiTests { false };
    bool mValidateInProcess { false };

    magic_args::option<std::string> mDataFile {
        .mHelp = "Path to data file for tests.",
    };

    magic_args::option<std::string> mOutputDir {
        .mHelp = "Directory for log files.",
    };

    magic_args::option<std::string> mOutputFilename {
        .mHelp = "Filename for log files.",
    };

    magic_args::option<std::string> mDisabledTests {
        .mHelp = "File path or comma-separated list of tests to disable.",
    };

    magic_args::option<CommaSeparatedDoubles> mSampleRates {
        .mValue = CommaSeparatedDoubles { { 44100, 48000, 96000 } },
        .mHelp = "Comma-separated sample rates (default: 44100,48000,96000).",
    };

    magic_args::option<CommaSeparatedInts> mBlockSizes {
        .mValue = CommaSeparatedInts { { 64, 128, 256, 512, 1024 } },
        .mHelp = "Comma-separated block sizes (default: 64,128,256,512,1024).",
    };

    magic_args::option<RealtimeCheck> mRtcheck {
        .mValue = RealtimeCheck::disabled,
        .mHelp = "Real-time safety checking: disabled, enabled, or relaxed.",
    };

    magic_args::option<std::string> mValidate {
        .mHelp = "Path to plugin file or AU identifier.",
    };
};

//==============================================================================
/** Unified settings struct used by both CLI and GUI code paths.
    Contains all validation options plus plugin path.
*/
struct PluginvalSettings
{
    std::string pluginPath;
    int strictnessLevel = 5;
    int64_t randomSeed = 0;
    int64_t timeoutMs = 30000;
    bool verbose = false;
    int numRepeats = 1;
    bool randomise = false;
    bool skipGuiTests = false;
    bool validateInProcess = false;
    std::string dataFile;
    std::string outputDir;
    std::string outputFilename;
    std::string disabledTests;
    std::vector<double> sampleRates = { 44100, 48000, 96000 };
    std::vector<int> blockSizes = { 64, 128, 256, 512, 1024 };
    RealtimeCheck realtimeCheck = RealtimeCheck::disabled;

    /** Convert to PluginTests::Options for backward compatibility with test framework. */
    PluginTests::Options toTestOptions() const;

    /** Build command line args for child-process spawning. */
    juce::StringArray toCommandLineArgs() const;

    /** Serialize to JSON. */
    nlohmann::json toJson() const;

    /** Deserialize from JSON. */
    static PluginvalSettings fromJson (const nlohmann::json& j);

    /** Create from a PluginTests::Options and plugin path (reverse bridge). */
    static PluginvalSettings fromTestOptions (const juce::String& fileOrID, const PluginTests::Options& opts);

    /** Save to a JUCE PropertiesFile (for GUI persistence). */
    void saveToProperties (juce::PropertiesFile& props) const;

    /** Load from a JUCE PropertiesFile (for GUI persistence). */
    static PluginvalSettings loadFromProperties (juce::PropertiesFile& props);
};

//==============================================================================
/** Convert parsed CLI args to a PluginvalSettings. */
PluginvalSettings toSettings (const PluginvalCliArgs& args);

//==============================================================================
/** Pre-process a raw command line string:
    - Deprecated name conversion (strictnessLevel -> strictness-level)
    - Strip macOS debug noise
    - Merge environment variables
    - Add implicit --validate for bare plugin paths

    Returns args ready for magic_args::parse.
*/
std::vector<std::string> preProcessCommandLine (
    const juce::String& commandLine,
    std::function<juce::String (const juce::String& name, const juce::String& defaultValue)> envVarProvider = nullptr);

//==============================================================================
/** Parse a command line string into PluginvalSettings.
    Returns nullopt if help/version was requested or parsing failed.
*/
std::optional<PluginvalSettings> parseCommandLineToSettings (const juce::String& commandLine);
