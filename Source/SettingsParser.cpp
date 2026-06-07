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
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace settings_parser
{
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

        /** Returns every value supplied for an option, in command-line order. */
        juce::StringArray allValuesForOption (const juce::StringArray& tokens, juce::StringRef option)
        {
            juce::StringArray out;
            const juce::String prefix = juce::String (option) + "=";

            for (int i = 0; i < tokens.size(); ++i)
            {
                if (tokens[i] == option)
                {
                    if (i + 1 < tokens.size())
                        out.add (tokens[i + 1]);
                }
                else if (tokens[i].startsWith (prefix))
                {
                    out.add (tokens[i].substring (prefix.length()));
                }
            }

            return out;
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
Precedence (lowest to highest): defaults, environment variables, --config, command-line options.
--config is repeatable; later files win per key.)");
        }

        std::string toEnvName (const std::string& longName)
        {
            return juce::String (longName).toUpperCase().replace ("-", "_").toStdString();
        }

        /** Loads one --config JSON file. */
        nlohmann::json loadConfigFile (const juce::String& path)
        {
            const juce::File file (path);

            if (! file.existsAsFile())
                throw std::runtime_error (("--config file not found: " + path).toStdString());

            return nlohmann::json::parse (file.loadFileAsString().toStdString());
        }

        /** Registers every option, bound to s. No ->envname(): the environment is a
            separate, lower-precedence layer (see parseTokens). */
        void configureApp (CLI::App& app, PluginvalSettings& s)
        {
            app.set_version_flag ("--version", getVersionString().toStdString());
            app.footer (getFooterText().toStdString());

            // Accepted so the CLI parse doesn't error on --config; the values are
            // handled manually (repeatable, inline-or-file) in parseTokens.
            app.add_option_function<std::vector<std::string>> ("--config",
                [] (const std::vector<std::string>&) {},
                "Path to a JSON settings file. Repeatable; later files win per key.")->take_all();

            app.add_option ("--validate", s.validatePath, "Validates the plugin at the given path (or AU id).");
            app.add_option ("--strictness-level", s.strictnessLevel, "Strictness level 1-10 (default 5).");
            app.add_option ("--timeout-ms", s.timeoutMs, "Test timeout in ms (default 30000, -1 to never timeout).");
            app.add_option ("--repeat", s.numRepeats, "Number of times to repeat the tests.");
            app.add_flag ("--randomise", s.randomiseTestOrder, "Run the tests in a random order per repeat.");
            app.add_flag ("--verbose", s.verbose, "Output additional logging information.");
            app.add_flag ("--skip-gui-tests", s.skipGuiTests, "Avoid tests that create GUI windows (for headless CI).");
            app.add_option ("--sample-rates", s.sampleRates, "Comma-separated sample rates (default 44100,48000,96000).")->delimiter (',');
            app.add_option ("--block-sizes", s.blockSizes, "Comma-separated block sizes (default 64,128,256,512,1024).")->delimiter (',');
            app.add_option ("--data-file", s.dataFile, "Path to a data file tests can use to configure themselves.");
            app.add_option ("--output-dir", s.outputDir, "Directory in which to write the log files.");
            app.add_option ("--output-filename", s.outputFilename, "Filename to write logs into.");

            app.add_option_function<std::string> ("--disabled-tests",
                [&s] (const std::string& v) { s.disabledTests = settings_serializer::disabledTestsToList (juce::String (v)); },
                "Comma-separated test names, or a path to a file listing them.");

            app.add_option_function<std::string> ("--random-seed",
                [&s] (const std::string& v)
                {
                    try { s.randomSeed = settings_serializer::parseRandomSeed (juce::String (v)); }
                    catch (const std::exception& e) { throw CLI::ValidationError ("--random-seed", e.what()); }
                },
                "Random seed (hex 0x.. or int) for replicable test runs.");

            app.add_option ("--rtcheck", s.realtimeCheck, "Real-time safety checks: disabled, enabled or relaxed.")
               ->transform (CLI::CheckedTransformer (std::map<std::string, RealtimeCheck> {
                    { "disabled", RealtimeCheck::disabled },
                    { "enabled",  RealtimeCheck::enabled  },
                    { "relaxed",  RealtimeCheck::relaxed  } }, CLI::ignore_case));
        }

        /** Builds a synthetic argv from the environment by deriving an env-var name
            from each registered option (e.g. --strictness-level -> STRICTNESS_LEVEL).
            CLI11 then parses and coerces it like any other argument. */
        std::vector<std::string> buildEnvArgv (const CLI::App& app, const EnvProvider& env)
        {
            std::vector<std::string> argv { "pluginval" };

            for (const auto* opt : app.get_options())
            {
                const auto& lnames = opt->get_lnames();

                if (lnames.empty())
                    continue;

                const auto& lname = lnames.front();

                // Skip the meta options that shouldn't be environment-driven.
                if (lname == "help" || lname == "version" || lname == "config" || lname == "validate")
                    continue;

                if (const auto value = env (juce::String (toEnvName (lname))); value.isNotEmpty())
                    argv.push_back ("--" + lname + "=" + value.toStdString()); // works for flags too (--flag=1/0)
            }

            return argv;
        }

        /** Runs one parse pass. Returns an exit code if the parse was "handled"
            (help/version/error), or nullopt on success. */
        std::optional<int> runParse (CLI::App& app, const std::vector<std::string>& argv)
        {
            std::vector<const char*> cargv;
            cargv.reserve (argv.size());

            for (const auto& a : argv)
                cargv.push_back (a.c_str());

            try
            {
                app.parse ((int) cargv.size(), cargv.data());
            }
            catch (const CLI::ParseError& e)
            {
                return app.exit (e);
            }

            return std::nullopt;
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

    //==============================================================================
    ParseResult parseTokens (const juce::StringArray& tokens, const EnvProvider& env)
    {
        ParseResult result;
        auto& s = result.settings;

        // --config-base64 carries a fully-resolved settings set from the parent
        // process and is authoritative. It is for internal use only and must not be
        // combined with other options (which would otherwise be silently ignored);
        // the only companion allowed is --validate.
        if (const auto b64 = valueForOption (tokens, "--config-base64"); b64.isNotEmpty())
        {
            for (const auto& token : tokens)
            {
                if (! token.startsWith ("-") || token == "-")
                    continue; // a value, not an option

                if (const auto name = token.upToFirstOccurrenceOf ("=", false, false);
                    name != "--config-base64" && name != "--validate")
                {
                    std::cerr << "*** FAILED: --config-base64 is for internal use and cannot be combined "
                              << "with other options (got " << name << ")" << std::endl;
                    result.exitCode = 1;
                    result.handled = true;
                    return result;
                }
            }

            s = settings_serializer::fromJsonString (decodeBase64 (b64).toStdString());
            s.validatePath = resolvePluginPath (juce::String (s.validatePath)).toStdString();
            return result;
        }

        // 1. Environment layer (env-var names derived from the registered options).
        {
            CLI::App envApp;
            configureApp (envApp, s);

            if (const auto code = runParse (envApp, buildEnvArgv (envApp, env)); code)
            {
                result.exitCode = *code;
                result.handled = true;
                return result;
            }
        }

        // 2. --config layer: repeatable, inline JSON or file, merged per key in
        //    command-line order (last wins). Beats the environment, loses to CLI.
        if (const auto configs = allValuesForOption (tokens, "--config"); ! configs.isEmpty())
        {
            auto merged = settings_serializer::toJson (s);

            for (const auto& source : configs)
                merged.merge_patch (loadConfigFile (source));

            s = merged.get<PluginvalSettings>();
        }

        // 3. Command-line layer: the individual options beat everything.
        {
            CLI::App cliApp { "Validate plugins to test compatibility with hosts and verify plugin API conformance" };
            configureApp (cliApp, s);

            std::vector<std::string> argv { "pluginval" };

            for (const auto& t : tokens)
                argv.push_back (t.toStdString());

            if (const auto code = runParse (cliApp, argv); code)
            {
                result.exitCode = *code;
                result.handled = true;
                return result;
            }
        }

        s.validatePath = resolvePluginPath (juce::String (s.validatePath)).toStdString();
        return result;
    }

    PluginvalSettings parse (const juce::String& commandLine, const EnvProvider& env)
    {
        return parseTokens (preprocess (commandLine), env).settings;
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
