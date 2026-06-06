/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "SettingsParser.h"
#include "SettingsSerializer.h"

#include <magic_args/magic_args.hpp>

#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace settings_parser
{
    //==============================================================================
    // The magic_args view of the flat command-line options. Everything is parsed
    // as a raw optional/flag; typed coercion happens when building the JSON layer.
    struct PluginvalCliArgs
    {
        magic_args::option<std::optional<std::string>> validate
            { .mName = "validate", .mHelp = "Validates the plugin at the given path (or AU id). Optional if the path is the last argument." };
        magic_args::option<std::optional<int>> strictnessLevel
            { .mName = "strictness-level", .mHelp = "Strictness level 1-10 (default 5, minimum 5 recommended)." };
        magic_args::option<std::optional<std::string>> randomSeed
            { .mName = "random-seed", .mHelp = "Random seed (hex 0x.. or int) for replicable test runs." };
        magic_args::option<std::optional<std::int64_t>> timeoutMs
            { .mName = "timeout-ms", .mHelp = "Test timeout in ms (default 30000, -1 to never timeout)." };
        magic_args::option<std::optional<int>> repeat
            { .mName = "repeat", .mHelp = "Number of times to repeat the tests." };
        magic_args::flag randomise
            { .mName = "randomise", .mHelp = "Run the tests in a random order per repeat." };
        magic_args::flag verbose
            { .mName = "verbose", .mHelp = "Output additional logging information." };
        magic_args::flag skipGuiTests
            { .mName = "skip-gui-tests", .mHelp = "Avoid tests that create GUI windows (for headless CI)." };
        magic_args::option<std::optional<std::string>> sampleRates
            { .mName = "sample-rates", .mHelp = "Comma-separated sample rates (default 44100,48000,96000)." };
        magic_args::option<std::optional<std::string>> blockSizes
            { .mName = "block-sizes", .mHelp = "Comma-separated block sizes (default 64,128,256,512,1024)." };
        magic_args::option<std::optional<std::string>> dataFile
            { .mName = "data-file", .mHelp = "Path to a data file tests can use to configure themselves." };
        magic_args::option<std::optional<std::string>> outputDir
            { .mName = "output-dir", .mHelp = "Directory in which to write the log files." };
        magic_args::option<std::optional<std::string>> outputFilename
            { .mName = "output-filename", .mHelp = "Filename to write logs into." };
        magic_args::option<std::optional<std::string>> disabledTests
            { .mName = "disabled-tests", .mHelp = "Comma-separated test names, or a path to a file listing them." };
        magic_args::option<std::optional<std::string>> rtcheck
            { .mName = "rtcheck", .mHelp = "Real-time safety checks: disabled, enabled or relaxed." };
        magic_args::option<std::optional<std::string>> config
            { .mName = "config", .mHelp = "Path to a JSON file of settings (overridden by env vars and CLI options)." };
    };

    //==============================================================================
    static magic_args::program_info makeProgramInfo()
    {
        return {
            .mDescription = "Validate plugins to test compatibility with hosts and verify plugin API conformance",
            .mVersion = getVersionString().toStdString(),
            .mExamples = {
                "pluginval --strictness-level 5 --validate path/to/plugin",
                "pluginval path/to/plugin",
                "pluginval --config settings.json --validate path/to/plugin",
            },
        };
    }

    static juce::String getTrailerText()
    {
        return juce::String (
R"(Other commands:
  --version                   Print the pluginval version.
  --run-tests                 Run the internal unit tests.
  --strictness-help [level]   List all tests that run at the given strictness level.

Exit code:
  0 if all tests complete successfully
  1 if there are any errors

Additionally, you can specify any of the command line options as environment
variables by removing prefix dashes, converting internal dashes to underscores
and capitalising all letters, e.g.
    "--skip-gui-tests" > "SKIP_GUI_TESTS=1"
    "--timeout-ms 30000" > "TIMEOUT_MS=30000"
Precedence is command-line options > environment variables > --config file.
)");
    }

    //==============================================================================
    juce::String systemEnv (const juce::String& name)
    {
        return juce::SystemStats::getEnvironmentVariable (name, {});
    }

    juce::String getVersionString()
    {
        return juce::String ("pluginval") + " - " + VERSION;
    }

    //==============================================================================
    namespace
    {
        bool isPluginArgument (const juce::String& arg)
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

            // The above checks the file exists, which isn't what we want for CLI parsing
            if (auto f = juce::File::createFileWithoutCheckingPath (arg);
                f.hasFileExtension (".vst3")
               #if JUCE_PLUGINHOST_VST
                || f.hasFileExtension (".dll")
               #endif
                )
                return true;

            return false;
        }

        juce::String valueForOption (const juce::StringArray& tokens, juce::StringRef option)
        {
            const juce::String prefix = juce::String (option) + "=";

            for (int i = 0; i < tokens.size(); ++i)
            {
                if (tokens[i] == option)
                    return i + 1 < tokens.size() ? tokens[i + 1] : juce::String();

                if (tokens[i].startsWith (prefix))
                    return tokens[i].substring (prefix.length());
            }

            return {};
        }

        bool hasOption (const juce::StringArray& tokens, juce::StringRef option)
        {
            const juce::String prefix = juce::String (option) + "=";

            for (const auto& t : tokens)
                if (t == option || t.startsWith (prefix))
                    return true;

            return false;
        }

        juce::String decodeBase64 (const juce::String& b64)
        {
            juce::MemoryOutputStream mos;
            juce::Base64::convertFromBase64 (mos, b64);
            return mos.toString();
        }

        // Builds the sparse CLI JSON layer by parsing tokens with magic_args.
        nlohmann::json parseCliLayer (const juce::StringArray& tokens)
        {
            std::vector<std::string> storage;
            storage.reserve ((size_t) tokens.size() + 1);
            storage.emplace_back ("pluginval");

            for (const auto& t : tokens)
                storage.push_back (t.toStdString());

            std::vector<std::string_view> views (storage.begin(), storage.end());

            const auto parsed = magic_args::parse<PluginvalCliArgs> (std::span<std::string_view> (views),
                                                                     makeProgramInfo());

            auto j = nlohmann::json::object();

            if (! parsed) // HelpRequested / VersionRequested / parse error: contribute nothing
                return j;

            const auto& a = *parsed;

            if (a.validate.has_value())         j["validatePath"]       = *a.validate;
            if (a.strictnessLevel.has_value())  j["strictnessLevel"]    = *a.strictnessLevel;
            if (a.randomSeed.has_value())       j["randomSeed"]         = settings_serializer::parseRandomSeed (juce::String (*a.randomSeed));
            if (a.timeoutMs.has_value())        j["timeoutMs"]          = *a.timeoutMs;
            if (a.repeat.has_value())           j["numRepeats"]         = *a.repeat;
            if (a.verbose)                      j["verbose"]            = true;
            if (a.randomise)                    j["randomiseTestOrder"] = true;
            if (a.skipGuiTests)                 j["skipGuiTests"]       = true;
            if (a.sampleRates.has_value())      j["sampleRates"]        = settings_serializer::commaToDoubleArray (juce::String (*a.sampleRates));
            if (a.blockSizes.has_value())       j["blockSizes"]         = settings_serializer::commaToIntArray (juce::String (*a.blockSizes));
            if (a.dataFile.has_value())         j["dataFile"]           = *a.dataFile;
            if (a.outputDir.has_value())        j["outputDir"]          = *a.outputDir;
            if (a.outputFilename.has_value())   j["outputFilename"]     = *a.outputFilename;
            if (a.disabledTests.has_value())    j["disabledTests"]      = settings_serializer::disabledTestsToArray (juce::String (*a.disabledTests));
            if (a.rtcheck.has_value())          j["realtimeCheck"]      = *a.rtcheck;

            return j;
        }

        nlohmann::json envLayer (const EnvProvider& env)
        {
            auto j = nlohmann::json::object();

            auto setString = [&] (const char* name, const char* key)
            {
                if (auto v = env (name); v.isNotEmpty())
                    j[key] = v.toStdString();
            };
            auto setFlag = [&] (const char* name, const char* key)
            {
                if (env (name).isNotEmpty())
                    j[key] = true;
            };

            if (auto v = env ("STRICTNESS_LEVEL"); v.isNotEmpty()) j["strictnessLevel"] = v.getIntValue();
            if (auto v = env ("RANDOM_SEED");      v.isNotEmpty()) j["randomSeed"]      = settings_serializer::parseRandomSeed (v);
            if (auto v = env ("TIMEOUT_MS");       v.isNotEmpty()) j["timeoutMs"]       = (std::int64_t) v.getLargeIntValue();
            if (auto v = env ("REPEAT");           v.isNotEmpty()) j["numRepeats"]      = v.getIntValue();
            setFlag ("VERBOSE",        "verbose");
            setFlag ("RANDOMISE",      "randomiseTestOrder");
            setFlag ("SKIP_GUI_TESTS", "skipGuiTests");
            setString ("DATA_FILE",       "dataFile");
            setString ("OUTPUT_DIR",      "outputDir");
            setString ("OUTPUT_FILENAME", "outputFilename");
            if (auto v = env ("SAMPLE_RATES"); v.isNotEmpty()) j["sampleRates"] = settings_serializer::commaToDoubleArray (v);
            if (auto v = env ("BLOCK_SIZES");  v.isNotEmpty()) j["blockSizes"]  = settings_serializer::commaToIntArray (v);
            setString ("RTCHECK", "realtimeCheck");

            return j;
        }
    }

    //==============================================================================
    juce::StringArray preprocess (const juce::String& commandLineIn)
    {
        if (commandLineIn.contains ("strictnessLevel"))
        {
            std::cout << "!!! WARNING:\n\t\"strictnessLevel\" is deprecated and will be removed in a future version.\n"
                      << "\tPlease use --strictness-level instead\n\n";
        }

        const auto commandLine = commandLineIn.replace ("strictnessLevel", "strictness-level")
                                              .replace ("-NSDocumentRevisionsDebugMode YES", "")
                                              .trim();

        juce::StringArray args;
        args.addTokens (commandLine, true);
        args.trim();

        for (auto& s : args)
            s = s.unquoted();

        // If only a plugin path is supplied as the last arg, add an implicit
        // --validate option for it so the rest of the CLI works.
        if (args.size() > 0)
        {
            const bool hasCommand = hasOption (args, "--validate")
                                 || args.contains ("--help") || args.contains ("-h")
                                 || args.contains ("--version")
                                 || args.contains ("--run-tests")
                                 || hasOption (args, "--config-base64");

            if (! hasCommand && isPluginArgument (args[args.size() - 1]))
                args.insert (args.size() - 1, "--validate");
        }

        return args;
    }

    bool isCommandLine (const juce::StringArray& tokens)
    {
        return tokens.contains ("--help") || tokens.contains ("-h")
            || tokens.contains ("--version")
            || hasOption (tokens, "--validate")
            || tokens.contains ("--run-tests")
            || tokens.contains ("--strictness-help")
            || hasOption (tokens, "--config-base64");
    }

    juce::String resolvePluginPath (const juce::String& raw)
    {
        // Resolve relative/home paths against the working directory. Absolute
        // paths and bare component IDs (no '.' or '~') are left untouched.
        if (raw.contains ("~") || raw.contains ("."))
            return juce::File::getCurrentWorkingDirectory().getChildFile (raw).getFullPathName();

        return raw;
    }

    PluginvalSettings resolveSettings (const juce::StringArray& tokens, const EnvProvider& env)
    {
        // A base64 JSON handoff from the parent process is fully authoritative.
        if (const auto b64 = valueForOption (tokens, "--config-base64"); b64.isNotEmpty())
        {
            auto s = settings_serializer::fromJsonString (decodeBase64 (b64).toStdString());
            s.validatePath = resolvePluginPath (juce::String (s.validatePath)).toStdString();
            return s;
        }

        auto configJson = nlohmann::json::object();

        if (const auto configPath = valueForOption (tokens, "--config"); configPath.isNotEmpty())
            configJson = nlohmann::json::parse (juce::File (configPath).loadFileAsString().toStdString());

        const auto envJson = envLayer (env);
        const auto cliJson = parseCliLayer (tokens);

        // Precedence (later wins): defaults < config < env < CLI.
        auto merged = nlohmann::json::object();
        merged.merge_patch (configJson);
        merged.merge_patch (envJson);
        merged.merge_patch (cliJson);

        auto s = merged.get<PluginvalSettings>();
        s.validatePath = resolvePluginPath (juce::String (s.validatePath)).toStdString();
        return s;
    }

    PluginvalSettings parse (const juce::String& commandLine, const EnvProvider& env)
    {
        return resolveSettings (preprocess (commandLine), env);
    }

    //==============================================================================
    juce::StringArray createChildProcessCommandLine (const juce::String& fileOrID, const PluginTests::Options& options)
    {
        const auto settings = PluginvalSettings::fromPluginTestOptions (options, fileOrID);
        const auto jsonString = settings_serializer::toJson (settings).dump();
        const auto b64 = juce::Base64::toBase64 (juce::String (jsonString));

        juce::StringArray args (juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName());
        args.addArray ({ "--config-base64", b64, "--validate", fileOrID });
        return args;
    }

    //==============================================================================
    void printHelp (const juce::String& exeName)
    {
        std::vector<std::string> storage { exeName.toStdString(), "--help" };
        std::vector<std::string_view> views (storage.begin(), storage.end());

        // magic_args prints the auto-generated usage to stdout and returns HelpRequested.
        (void) magic_args::parse<PluginvalCliArgs> (std::span<std::string_view> (views), makeProgramInfo());

        std::cout << "\n" << getTrailerText() << std::endl;
    }
}
