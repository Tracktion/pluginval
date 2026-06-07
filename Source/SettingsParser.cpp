/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

// CLI11 must be included before the JUCE headers: on Linux JUCE pulls in the X11
// headers (JUCE_GUI_BASICS_INCLUDE_XHEADERS), which #define Success/None/Bool/etc.
// and would clash with CLI11's CLI::ExitCodes::Success enumerator.
#include <CLI/CLI.hpp>

#include "SettingsParser.h"
#include "SettingsSerializer.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace settings_parser
{
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

        juce::String getFooterText()
        {
            return juce::SystemStats::getJUCEVersion() + "\n\n" + juce::String (
R"(Other commands:
  --run-tests                 Run the internal unit tests.
  --strictness-help [level]   List all tests that run at the given strictness level.

Exit code:
  0 if all tests complete successfully
  1 if there are any errors

You can also specify any option as an environment variable by removing the prefix
dashes, converting internal dashes to underscores and capitalising, e.g.
    "--skip-gui-tests" -> "SKIP_GUI_TESTS=1"
    "--timeout-ms 30000" -> "TIMEOUT_MS=30000"
Precedence: command-line options > environment variables > --config file.)");
        }
    }

    //==============================================================================
    juce::String getVersionString()
    {
        return juce::String ("pluginval") + " - " + VERSION;
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

    //==============================================================================
    ParseResult parseTokens (const juce::StringArray& tokens)
    {
        ParseResult result;
        auto& s = result.settings;

        // A base64 JSON handoff from the parent process is fully authoritative.
        if (const auto b64 = valueForOption (tokens, "--config-base64"); b64.isNotEmpty())
        {
            s = settings_serializer::fromJsonString (decodeBase64 (b64).toStdString());
            s.validatePath = resolvePluginPath (juce::String (s.validatePath)).toStdString();
            return result;
        }

        // Seed from --config first; CLI11 only overwrites members whose flag/env
        // was provided, giving precedence: defaults < config < env < CLI.
        if (const auto configPath = valueForOption (tokens, "--config"); configPath.isNotEmpty())
            s = settings_serializer::fromJsonFile (juce::File (configPath));

        CLI::App app { "Validate plugins to test compatibility with hosts and verify plugin API conformance" };
        app.set_version_flag ("--version", getVersionString().toStdString());
        app.footer (getFooterText().toStdString());

        std::string configSink; // accepted here; the file is loaded above
        app.add_option ("--config", configSink, "Path to a JSON settings file (overridden by env vars and CLI options).");

        app.add_option ("--validate", s.validatePath, "Validates the plugin at the given path (or AU id).");
        app.add_option ("--strictness-level", s.strictnessLevel, "Strictness level 1-10 (default 5).")->envname ("STRICTNESS_LEVEL");
        app.add_option ("--timeout-ms", s.timeoutMs, "Test timeout in ms (default 30000, -1 to never timeout).")->envname ("TIMEOUT_MS");
        app.add_option ("--repeat", s.numRepeats, "Number of times to repeat the tests.")->envname ("REPEAT");
        app.add_flag ("--randomise", s.randomiseTestOrder, "Run the tests in a random order per repeat.")->envname ("RANDOMISE");
        app.add_flag ("--verbose", s.verbose, "Output additional logging information.")->envname ("VERBOSE");
        app.add_flag ("--skip-gui-tests", s.skipGuiTests, "Avoid tests that create GUI windows (for headless CI).")->envname ("SKIP_GUI_TESTS");
        app.add_option ("--sample-rates", s.sampleRates, "Comma-separated sample rates (default 44100,48000,96000).")->delimiter (',')->envname ("SAMPLE_RATES");
        app.add_option ("--block-sizes", s.blockSizes, "Comma-separated block sizes (default 64,128,256,512,1024).")->delimiter (',')->envname ("BLOCK_SIZES");
        app.add_option ("--data-file", s.dataFile, "Path to a data file tests can use to configure themselves.")->envname ("DATA_FILE");
        app.add_option ("--output-dir", s.outputDir, "Directory in which to write the log files.")->envname ("OUTPUT_DIR");
        app.add_option ("--output-filename", s.outputFilename, "Filename to write logs into.")->envname ("OUTPUT_FILENAME");

        app.add_option_function<std::string> ("--disabled-tests",
            [&s] (const std::string& v) { s.disabledTests = settings_serializer::disabledTestsToList (juce::String (v)); },
            "Comma-separated test names, or a path to a file listing them.");

        app.add_option_function<std::string> ("--random-seed",
            [&s] (const std::string& v)
            {
                try { s.randomSeed = settings_serializer::parseRandomSeed (juce::String (v)); }
                catch (const std::exception& e) { throw CLI::ValidationError ("--random-seed", e.what()); }
            },
            "Random seed (hex 0x.. or int) for replicable test runs.")->envname ("RANDOM_SEED");

        app.add_option ("--rtcheck", s.realtimeCheck, "Real-time safety checks: disabled, enabled or relaxed.")
           ->transform (CLI::CheckedTransformer (std::map<std::string, RealtimeCheck> {
                { "disabled", RealtimeCheck::disabled },
                { "enabled",  RealtimeCheck::enabled  },
                { "relaxed",  RealtimeCheck::relaxed  } }, CLI::ignore_case))
           ->envname ("RTCHECK");

        // Build argv (CLI11 treats element 0 as the program name)
        std::vector<std::string> storage;
        storage.reserve ((size_t) tokens.size() + 1);
        storage.emplace_back ("pluginval");

        for (const auto& t : tokens)
            storage.push_back (t.toStdString());

        std::vector<const char*> argv;
        argv.reserve (storage.size());

        for (const auto& str : storage)
            argv.push_back (str.c_str());

        try
        {
            app.parse ((int) argv.size(), argv.data());
        }
        catch (const CLI::ParseError& e)
        {
            // Includes CallForHelp / CallForVersion (exit code 0) and real errors.
            result.exitCode = app.exit (e);
            result.handled = true;
            return result;
        }

        s.validatePath = resolvePluginPath (juce::String (s.validatePath)).toStdString();
        return result;
    }

    PluginvalSettings parse (const juce::String& commandLine)
    {
        return parseTokens (preprocess (commandLine)).settings;
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
}
