/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "PluginvalSettings.h"

#include <magic_enum/magic_enum.hpp>

#include <charconv>
#include <sstream>

//==============================================================================
// ADL: CommaSeparatedDoubles
//==============================================================================
void from_string_argument (CommaSeparatedDoubles& out, std::string_view arg)
{
    out.values.clear();
    std::string token;
    std::istringstream stream { std::string (arg) };

    while (std::getline (stream, token, ','))
    {
        if (! token.empty())
            out.values.push_back (std::stod (token));
    }
}

std::string formattable_argument_value (const CommaSeparatedDoubles& v)
{
    std::string result;

    for (size_t i = 0; i < v.values.size(); ++i)
    {
        if (i > 0)
            result += ",";

        // Use integer format when there's no fractional part
        if (v.values[i] == static_cast<int> (v.values[i]))
            result += std::to_string (static_cast<int> (v.values[i]));
        else
            result += std::to_string (v.values[i]);
    }

    return result;
}

//==============================================================================
// ADL: CommaSeparatedInts
//==============================================================================
void from_string_argument (CommaSeparatedInts& out, std::string_view arg)
{
    out.values.clear();
    std::string token;
    std::istringstream stream { std::string (arg) };

    while (std::getline (stream, token, ','))
    {
        if (! token.empty())
            out.values.push_back (std::stoi (token));
    }
}

std::string formattable_argument_value (const CommaSeparatedInts& v)
{
    std::string result;

    for (size_t i = 0; i < v.values.size(); ++i)
    {
        if (i > 0)
            result += ",";

        result += std::to_string (v.values[i]);
    }

    return result;
}

//==============================================================================
// ADL: HexOrDecimalSeed
//==============================================================================
void from_string_argument (HexOrDecimalSeed& out, std::string_view arg)
{
    if (arg.starts_with ("0x") || arg.starts_with ("0X"))
    {
        auto hexPart = arg.substr (2);
        auto [ptr, ec] = std::from_chars (hexPart.data(), hexPart.data() + hexPart.size(), out.value, 16);

        if (ec != std::errc())
            out.value = 0;
    }
    else
    {
        auto [ptr, ec] = std::from_chars (arg.data(), arg.data() + arg.size(), out.value);

        if (ec != std::errc())
            out.value = 0;
    }
}

std::string formattable_argument_value (const HexOrDecimalSeed& v)
{
    return std::to_string (v.value);
}

//==============================================================================
// ADL: RealtimeCheck
//==============================================================================
void from_string_argument (RealtimeCheck& out, std::string_view arg)
{
    auto result = magic_enum::enum_cast<RealtimeCheck> (arg);
    out = result.value_or (RealtimeCheck::disabled);
}

std::string formattable_argument_value (const RealtimeCheck& v)
{
    return std::string (magic_enum::enum_name (v));
}

//==============================================================================
// PluginvalSettings -> PluginTests::Options
//==============================================================================
PluginTests::Options PluginvalSettings::toTestOptions() const
{
    PluginTests::Options opts;
    opts.strictnessLevel = strictnessLevel;
    opts.randomSeed = randomSeed;
    opts.timeoutMs = timeoutMs;
    opts.verbose = verbose;
    opts.numRepeats = numRepeats;
    opts.randomiseTestOrder = randomise;
    opts.withGUI = ! skipGuiTests;
    opts.sampleRates = sampleRates;
    opts.blockSizes = blockSizes;
    opts.realtimeCheck = realtimeCheck;

    if (! dataFile.empty())
        opts.dataFile = juce::File (juce::String (dataFile));

    if (! outputDir.empty())
        opts.outputDir = juce::File (juce::String (outputDir));

    if (! outputFilename.empty())
        opts.outputFilename = juce::String (outputFilename);

    if (! disabledTests.empty())
    {
        juce::String dt (disabledTests);

        if (juce::File::isAbsolutePath (dt))
        {
            juce::StringArray tests;
            juce::File (dt).readLines (tests);
            opts.disabledTests = tests;
        }
        else
        {
            opts.disabledTests = juce::StringArray::fromTokens (dt, ",", "");
        }
    }

    return opts;
}

//==============================================================================
// PluginTests::Options -> PluginvalSettings (reverse bridge)
//==============================================================================
PluginvalSettings PluginvalSettings::fromTestOptions (const juce::String& fileOrID, const PluginTests::Options& opts)
{
    PluginvalSettings s;
    s.pluginPath = fileOrID.toStdString();
    s.strictnessLevel = opts.strictnessLevel;
    s.randomSeed = opts.randomSeed;
    s.timeoutMs = opts.timeoutMs;
    s.verbose = opts.verbose;
    s.numRepeats = opts.numRepeats;
    s.randomise = opts.randomiseTestOrder;
    s.skipGuiTests = ! opts.withGUI;
    s.sampleRates = opts.sampleRates;
    s.blockSizes = opts.blockSizes;
    s.realtimeCheck = opts.realtimeCheck;

    if (opts.dataFile != juce::File())
        s.dataFile = opts.dataFile.getFullPathName().toStdString();

    if (opts.outputDir != juce::File())
        s.outputDir = opts.outputDir.getFullPathName().toStdString();

    if (opts.outputFilename.isNotEmpty())
        s.outputFilename = opts.outputFilename.toStdString();

    if (! opts.disabledTests.isEmpty())
        s.disabledTests = opts.disabledTests.joinIntoString (",").toStdString();

    return s;
}

//==============================================================================
// PluginvalSettings -> command line args
//==============================================================================
juce::StringArray PluginvalSettings::toCommandLineArgs() const
{
    juce::StringArray args (juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName());
    const PluginvalSettings defaults;

    if (strictnessLevel != defaults.strictnessLevel)
        args.addArray ({ "--strictness-level", juce::String (strictnessLevel) });

    if (randomSeed != defaults.randomSeed)
        args.addArray ({ "--random-seed", juce::String (randomSeed) });

    if (timeoutMs != defaults.timeoutMs)
        args.addArray ({ "--timeout-ms", juce::String (timeoutMs) });

    if (verbose)
        args.add ("--verbose");

    if (skipGuiTests)
        args.add ("--skip-gui-tests");

    if (numRepeats != defaults.numRepeats)
        args.addArray ({ "--repeat", juce::String (numRepeats) });

    if (randomise)
        args.add ("--randomise");

    if (! dataFile.empty())
        args.addArray ({ "--data-file", juce::String (dataFile) });

    if (! outputDir.empty())
        args.addArray ({ "--output-dir", juce::String (outputDir) });

    if (! outputFilename.empty())
        args.addArray ({ "--output-filename", juce::String (outputFilename) });

    if (! disabledTests.empty())
        args.addArray ({ "--disabled-tests", juce::String (disabledTests) });

    if (! sampleRates.empty())
    {
        juce::StringArray rates;

        for (auto rate : sampleRates)
            rates.add (juce::String (rate));

        args.addArray ({ "--sample-rates", rates.joinIntoString (",") });
    }

    if (! blockSizes.empty())
    {
        juce::StringArray sizes;

        for (auto size : blockSizes)
            sizes.add (juce::String (size));

        args.addArray ({ "--block-sizes", sizes.joinIntoString (",") });
    }

    if (realtimeCheck != RealtimeCheck::disabled)
        args.addArray ({ "--rtcheck", std::string (magic_enum::enum_name (realtimeCheck)) });

    args.addArray ({ "--validate", juce::String (pluginPath) });

    return args;
}

//==============================================================================
// JSON serialization
//==============================================================================
nlohmann::json PluginvalSettings::toJson() const
{
    nlohmann::json j;
    j["strictnessLevel"] = strictnessLevel;
    j["randomSeed"] = randomSeed;
    j["timeoutMs"] = timeoutMs;
    j["verbose"] = verbose;
    j["numRepeats"] = numRepeats;
    j["randomise"] = randomise;
    j["skipGuiTests"] = skipGuiTests;
    j["validateInProcess"] = validateInProcess;
    j["sampleRates"] = sampleRates;
    j["blockSizes"] = blockSizes;
    j["realtimeCheck"] = std::string (magic_enum::enum_name (realtimeCheck));

    if (! pluginPath.empty())
        j["validate"] = pluginPath;

    if (! dataFile.empty())
        j["dataFile"] = dataFile;

    if (! outputDir.empty())
        j["outputDir"] = outputDir;

    if (! outputFilename.empty())
        j["outputFilename"] = outputFilename;

    if (! disabledTests.empty())
        j["disabledTests"] = disabledTests;

    return j;
}

PluginvalSettings PluginvalSettings::fromJson (const nlohmann::json& j)
{
    PluginvalSettings s;

    if (j.contains ("strictnessLevel"))
        s.strictnessLevel = j["strictnessLevel"].get<int>();

    if (j.contains ("randomSeed"))
        s.randomSeed = j["randomSeed"].get<int64_t>();

    if (j.contains ("timeoutMs"))
        s.timeoutMs = j["timeoutMs"].get<int64_t>();

    if (j.contains ("verbose"))
        s.verbose = j["verbose"].get<bool>();

    if (j.contains ("numRepeats"))
        s.numRepeats = j["numRepeats"].get<int>();

    if (j.contains ("randomise"))
        s.randomise = j["randomise"].get<bool>();

    if (j.contains ("skipGuiTests"))
        s.skipGuiTests = j["skipGuiTests"].get<bool>();

    if (j.contains ("validateInProcess"))
        s.validateInProcess = j["validateInProcess"].get<bool>();

    if (j.contains ("sampleRates"))
        s.sampleRates = j["sampleRates"].get<std::vector<double>>();

    if (j.contains ("blockSizes"))
        s.blockSizes = j["blockSizes"].get<std::vector<int>>();

    if (j.contains ("realtimeCheck"))
    {
        auto str = j["realtimeCheck"].get<std::string>();
        s.realtimeCheck = magic_enum::enum_cast<RealtimeCheck> (str).value_or (RealtimeCheck::disabled);
    }

    if (j.contains ("validate"))
        s.pluginPath = j["validate"].get<std::string>();

    if (j.contains ("dataFile"))
        s.dataFile = j["dataFile"].get<std::string>();

    if (j.contains ("outputDir"))
        s.outputDir = j["outputDir"].get<std::string>();

    if (j.contains ("outputFilename"))
        s.outputFilename = j["outputFilename"].get<std::string>();

    if (j.contains ("disabledTests"))
        s.disabledTests = j["disabledTests"].get<std::string>();

    return s;
}

//==============================================================================
// PropertiesFile persistence
//==============================================================================
void PluginvalSettings::saveToProperties (juce::PropertiesFile& props) const
{
    props.setValue ("strictnessLevel", strictnessLevel);
    props.setValue ("randomSeed", static_cast<juce::int64> (randomSeed));
    props.setValue ("timeoutMs", static_cast<juce::int64> (timeoutMs));
    props.setValue ("verbose", verbose);
    props.setValue ("numRepeats", numRepeats);
    props.setValue ("randomiseTests", randomise);
    props.setValue ("validateInProcess", validateInProcess);
    props.setValue ("realtimeCheckMode", juce::String (std::string (magic_enum::enum_name (realtimeCheck))));

    if (! outputDir.empty())
        props.setValue ("outputDir", juce::String (outputDir));
}

PluginvalSettings PluginvalSettings::loadFromProperties (juce::PropertiesFile& props)
{
    PluginvalSettings s;
    s.strictnessLevel = juce::jlimit (1, 10, props.getIntValue ("strictnessLevel", 5));
    s.randomSeed = props.getIntValue ("randomSeed", 0);
    s.timeoutMs = props.getIntValue ("timeoutMs", 30000);
    s.verbose = props.getBoolValue ("verbose", false);
    s.numRepeats = juce::jmax (1, props.getIntValue ("numRepeats", 1));
    s.randomise = props.getBoolValue ("randomiseTests", false);
    s.validateInProcess = props.getBoolValue ("validateInProcess", false);

    auto modeString = props.getValue ("realtimeCheckMode", juce::String());
    s.realtimeCheck = magic_enum::enum_cast<RealtimeCheck> (modeString.toStdString())
                          .value_or (RealtimeCheck::disabled);

    auto outDir = props.getValue ("outputDir", juce::String());

    if (outDir.isNotEmpty())
        s.outputDir = outDir.toStdString();

    return s;
}

//==============================================================================
// PluginvalCliArgs -> PluginvalSettings
//==============================================================================
PluginvalSettings toSettings (const PluginvalCliArgs& args)
{
    PluginvalSettings s;

    s.strictnessLevel = juce::jlimit (1, 10, static_cast<int> (args.mStrictnessLevel));
    s.randomSeed = args.mRandomSeed.mValue.value;
    s.timeoutMs = static_cast<int64_t> (args.mTimeoutMs);
    s.verbose = args.mVerbose;
    s.numRepeats = juce::jmax (1, static_cast<int> (args.mRepeat));
    s.randomise = args.mRandomise;
    s.skipGuiTests = args.mSkipGuiTests;
    s.validateInProcess = args.mValidateInProcess;
    s.sampleRates = args.mSampleRates.mValue.values;
    s.blockSizes = args.mBlockSizes.mValue.values;
    s.realtimeCheck = args.mRtcheck.mValue;

    // Plugin path with path resolution
    std::string fileOrID = args.mValidate.mValue;

    if (! fileOrID.empty())
    {
        juce::String jFileOrID (fileOrID);

        if (jFileOrID.contains ("~") || jFileOrID.contains ("."))
            jFileOrID = juce::File::getCurrentWorkingDirectory().getChildFile (jFileOrID).getFullPathName();

        s.pluginPath = jFileOrID.toStdString();
    }

    if (! args.mDataFile.mValue.empty())
        s.dataFile = args.mDataFile.mValue;

    if (! args.mOutputDir.mValue.empty())
        s.outputDir = args.mOutputDir.mValue;

    if (! args.mOutputFilename.mValue.empty())
        s.outputFilename = args.mOutputFilename.mValue;

    if (! args.mDisabledTests.mValue.empty())
        s.disabledTests = args.mDisabledTests.mValue;

    return s;
}

//==============================================================================
// Pre-processing
//==============================================================================
namespace
{
    struct OptionSpec
    {
        const char* name;
        bool requiresValue;
    };

    static OptionSpec knownOptions[] =
    {
        { "--strictness-level",     true  },
        { "--random-seed",          true  },
        { "--timeout-ms",           true  },
        { "--verbose",              false },
        { "--skip-gui-tests",       false },
        { "--data-file",            true  },
        { "--output-dir",           true  },
        { "--output-filename",      true  },
        { "--repeat",               true  },
        { "--randomise",            false },
        { "--sample-rates",         true  },
        { "--block-sizes",          true  },
        { "--rtcheck",              true  },
    };

    juce::String getEnvVarName (const char* optName)
    {
        return juce::String (optName).trimCharactersAtStart ("-").replace ("-", "_").toUpperCase();
    }

    bool isPluginArgument (juce::String arg)
    {
        juce::AudioPluginFormatManager formatManager;
       #if JUCE_VERSION >= 0x08000B
        juce::addDefaultFormatsToManager (formatManager);
       #else
        formatManager.addDefaultFormats();
       #endif

        for (auto format : formatManager.getFormats())
            if (format->fileMightContainThisPluginType (arg))
                return true;

        if (auto f = juce::File::createFileWithoutCheckingPath (arg);
            f.hasFileExtension (".vst3")
           #if JUCE_PLUGINHOST_VST
            || f.hasFileExtension (".dll")
           #endif
            )
           return true;

        return false;
    }
}

std::vector<std::string> preProcessCommandLine (
    const juce::String& commandLine,
    std::function<juce::String (const juce::String& name, const juce::String& defaultValue)> envVarProvider)
{
    if (! envVarProvider)
    {
        envVarProvider = [] (const juce::String& name, const juce::String& defaultValue)
        {
            return juce::SystemStats::getEnvironmentVariable (name, defaultValue);
        };
    }

    // Step 1: Deprecated name conversion and noise stripping
    auto cleaned = commandLine.replace ("strictnessLevel", "strictness-level")
                              .replace ("-NSDocumentRevisionsDebugMode YES", "")
                              .trim();

    if (cleaned.contains ("strictness-level") && commandLine.contains ("strictnessLevel"))
    {
        std::cout << "!!! WARNING:\n\t\"strictnessLevel\" is deprecated and will be removed in a future version.\n"
                  << "\tPlease use --strictness-level instead\n\n";
    }

    // Step 2: Tokenize
    juce::StringArray tokens;
    tokens.addTokens (cleaned, true);
    tokens.trim();

    for (auto& s : tokens)
        s = s.unquoted();

    // Step 3: Merge environment variables
    for (auto& opt : knownOptions)
    {
        auto envVarName = getEnvVarName (opt.name);
        auto envVarValue = envVarProvider (envVarName, {});

        if (envVarValue.isNotEmpty())
        {
            if (tokens.indexOf (opt.name) != -1)
            {
                std::cout << "Skipping environment variable " << envVarName
                          << " due to " << opt.name << " set" << std::endl;
                continue;
            }

            if (opt.requiresValue)
                tokens.insert (0, envVarValue);

            tokens.insert (0, opt.name);
        }
    }

    // Step 4: Implicit --validate for bare plugin path as last arg
    bool hasCommand = tokens.indexOf ("--validate") != -1
                   || tokens.indexOf ("--help") != -1
                   || tokens.indexOf ("-?") != -1
                   || tokens.indexOf ("--version") != -1
                   || tokens.indexOf ("--run-tests") != -1
                   || tokens.indexOf ("--strictness-help") != -1;

    if (! hasCommand && tokens.size() > 0)
    {
        if (isPluginArgument (tokens[tokens.size() - 1]))
            tokens.insert (tokens.size() - 1, "--validate");
    }

    // Convert to std::vector<std::string>
    std::vector<std::string> result;
    result.reserve ((size_t) tokens.size());

    for (const auto& t : tokens)
        result.push_back (t.toStdString());

    return result;
}

//==============================================================================
// Full parse
//==============================================================================
std::optional<PluginvalSettings> parseCommandLineToSettings (const juce::String& commandLine)
{
    auto preprocessed = preProcessCommandLine (commandLine);

    // Build string_view span for magic_args
    std::vector<std::string_view> views;
    views.reserve (preprocessed.size() + 1);

    // magic_args expects argv[0] to be the program name
    auto exePath = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName().toStdString();
    views.push_back (exePath);

    for (const auto& s : preprocessed)
        views.push_back (s);

    const magic_args::program_info info {
        .mDescription = "Validate plugins to test compatibility with hosts and verify plugin API conformance",
        .mVersion = VERSION,
    };

    auto result = magic_args::parse<PluginvalCliArgs> (std::span<std::string_view> (views), info);

    if (! result.has_value())
        return std::nullopt;

    return toSettings (*result);
}
