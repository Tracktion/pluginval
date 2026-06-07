/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include <cstdlib>

struct CommandLineTests : public juce::UnitTest
{
    CommandLineTests()
        : juce::UnitTest ("CommandLineTests", "pluginval")
    {
    }

    static constexpr const char* knownEnvVars[] = {
        "STRICTNESS_LEVEL", "RANDOM_SEED", "TIMEOUT_MS", "VERBOSE", "REPEAT",
        "RANDOMISE", "SKIP_GUI_TESTS", "DATA_FILE", "OUTPUT_DIR", "OUTPUT_FILENAME",
        "SAMPLE_RATES", "BLOCK_SIZES", "RTCHECK"
    };

    static void setEnv (const char* name, const char* value)
    {
       #if JUCE_WINDOWS
        _putenv_s (name, value);
       #else
        ::setenv (name, value, 1);
       #endif
    }

    static void unsetEnv (const char* name)
    {
       #if JUCE_WINDOWS
        _putenv_s (name, "");
       #else
        ::unsetenv (name);
       #endif
    }

    static void clearKnownEnv()
    {
        for (auto* n : knownEnvVars)
            unsetEnv (n);
    }

    static PluginvalSettings parse (const juce::String& cmd)
    {
        return settings_parser::parse (cmd);
    }

    void runTest() override
    {
        // Start from a clean environment so host env vars don't affect the deterministic tests.
        clearKnownEnv();

        beginTest ("Command line defaults");
        {
            const auto opts = parse ("").toPluginTestOptions();
            expectEquals (opts.strictnessLevel, 5);
            expectEquals (opts.randomSeed, (juce::int64) 0);
            expectEquals (opts.timeoutMs, (juce::int64) 30000);
            expectEquals (opts.numRepeats, 1);
            expect (opts.verbose == false);
            expect (opts.randomiseTestOrder == false);
            expect (opts.withGUI == true);
            expect (opts.realtimeCheck == RealtimeCheck::disabled);
            expect (opts.dataFile == juce::File());
            expect (opts.outputDir == juce::File());
            expect (opts.sampleRates == PluginvalSettings::defaultSampleRates());
            expect (opts.blockSizes == PluginvalSettings::defaultBlockSizes());
        }

        beginTest ("Command line parser");
        {
            const auto settings = parse ("--strictness-level 7 --random-seed 1234 --timeout-ms 20000 --repeat 11 "
                                         "--data-file /path/to/file --output-dir /path/to/dir --validate /path/to/plugin");
            const auto opts = settings.toPluginTestOptions();
            expectEquals (opts.strictnessLevel, 7);
            expectEquals (opts.randomSeed, (juce::int64) 1234);
            expectEquals (opts.timeoutMs, (juce::int64) 20000);
            expectEquals (opts.numRepeats, 11);
            expectEquals (opts.dataFile.getFullPathName(), juce::String ("/path/to/file"));
            expectEquals (opts.outputDir.getFullPathName(), juce::String ("/path/to/dir"));
            expectEquals (juce::String (settings.validatePath), juce::String ("/path/to/plugin"));
        }

        beginTest ("Negative timeout");
        {
            expectEquals ((juce::int64) parse ("--timeout-ms -1 --validate x").timeoutMs, (juce::int64) -1);
        }

        beginTest ("Command line random (hex and int)");
        {
            expectEquals ((juce::int64) parse ("--random-seed 0x7f2da1 --validate x").randomSeed, (juce::int64) 8334753);
            expectEquals ((juce::int64) parse ("--random-seed 0x692bc1f --validate x").randomSeed, (juce::int64) 110279711);
            expectEquals ((juce::int64) parse ("--random-seed 1234 --validate x").randomSeed, (juce::int64) 1234);
        }

        beginTest ("Comma-separated lists");
        {
            const auto opts = parse ("--sample-rates 22050,44100 --block-sizes 32,64,128 --validate x").toPluginTestOptions();
            expect (opts.sampleRates == std::vector<double> ({ 22050.0, 44100.0 }));
            expect (opts.blockSizes == std::vector<int> ({ 32, 64, 128 }));
        }

        beginTest ("rtcheck enum parsing");
        {
            expect (parse ("--rtcheck relaxed --validate x").realtimeCheck == RealtimeCheck::relaxed);
            expect (parse ("--rtcheck enabled --validate x").realtimeCheck == RealtimeCheck::enabled);
            expect (parse ("--validate x").realtimeCheck == RealtimeCheck::disabled);
        }

        beginTest ("Handles an absolute path to the plugin");
        {
            const auto homeDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName();
            expectEquals (juce::String (parse ("--validate " + homeDir + "/path/to/MyPlugin").validatePath),
                          homeDir + "/path/to/MyPlugin");
        }

        beginTest ("Handles a quoted absolute path to the plugin");
        {
            const auto homeDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName();
            const auto pathToQuote = homeDir + "/path/to/MyPlugin";
            expectEquals (juce::String (parse ("--validate " + pathToQuote.quoted()).validatePath),
                          homeDir + "/path/to/MyPlugin");
        }

        beginTest ("Handles a relative path");
        {
            const auto currentDir = juce::File::getCurrentWorkingDirectory();
            expectEquals (juce::String (parse ("--validate MyPlugin.vst3").validatePath),
                          currentDir.getChildFile ("MyPlugin.vst3").getFullPathName());
        }

        beginTest ("Handles a quoted relative path with spaces to the plugin");
        {
            const auto currentDir = juce::File::getCurrentWorkingDirectory();
            expectEquals (juce::String (parse (R"(--validate "My Plugin.vst3")").validatePath),
                          currentDir.getChildFile ("My Plugin.vst3").getFullPathName());
        }

       #if !JUCE_WINDOWS
        beginTest ("Handles a relative path with ./ to the plugin");
        {
            const auto currentDir = juce::File::getCurrentWorkingDirectory().getFullPathName();
            expectEquals (juce::String (parse ("--validate ./path/to/MyPlugin").validatePath),
                          currentDir + "/path/to/MyPlugin");
        }

        beginTest ("Handles a home directory relative path to the plugin");
        {
            expectEquals (juce::String (parse ("--validate ~/path/to/MyPlugin").validatePath),
                          juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName() + "/path/to/MyPlugin");
        }

        beginTest ("Handles quoted strings, spaces, and home directory relative path to the plugin");
        {
            const auto cmd = R"(--data-file "~/path/to/My File" --output-dir "~/path/to/My Directory" --validate "~/path/to/My Plugin")";
            expectEquals (juce::String (parse (cmd).validatePath),
                          juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName() + "/path/to/My Plugin");
        }
       #endif

        beginTest ("Implicit validate with a relative path");
        {
            const auto currentDir = juce::File::getCurrentWorkingDirectory();
            expectEquals (juce::String (parse ("MyPlugin.vst3").validatePath),
                          currentDir.getChildFile ("MyPlugin.vst3").getFullPathName());
        }

        beginTest ("Doesn't alter component IDs");
        {
            expectEquals (juce::String (parse ("--validate MyPluginID").validatePath), juce::String ("MyPluginID"));
        }

        beginTest ("Allows for other options after explicit --validate");
        {
            const auto currentDir = juce::File::getCurrentWorkingDirectory();
            const auto settings = parse ("--validate MyPlugin.vst3 --randomise");
            expectEquals (juce::String (settings.validatePath), currentDir.getChildFile ("MyPlugin.vst3").getFullPathName());
            expect (settings.randomiseTestOrder);
        }

        beginTest ("Should perform command line");
        {
            juce::TemporaryFile temp ("path_to_file.vst3");
            expect (temp.getFile().create());
            expect (shouldPerformCommandLine (temp.getFile().getFullPathName()));
            expect (shouldPerformCommandLine ("--run-tests"));
            expect (shouldPerformCommandLine ("--version"));
            expect (! shouldPerformCommandLine (""));
        }

        beginTest ("Environment variables");
        {
            setEnv ("STRICTNESS_LEVEL", "7");
            setEnv ("RANDOM_SEED", "1234");
            setEnv ("TIMEOUT_MS", "20000");
            setEnv ("VERBOSE", "1");
            setEnv ("REPEAT", "11");
            setEnv ("RANDOMISE", "1");
            setEnv ("SKIP_GUI_TESTS", "1");
            setEnv ("DATA_FILE", "/path/to/file");
            setEnv ("OUTPUT_DIR", "/path/to/dir");
            setEnv ("SAMPLE_RATES", "22050,44100");
            setEnv ("BLOCK_SIZES", "32,64");
            setEnv ("RTCHECK", "relaxed");

            const auto opts = parse ("--validate x").toPluginTestOptions();

            clearKnownEnv();

            expectEquals (opts.strictnessLevel, 7);
            expectEquals (opts.randomSeed, (juce::int64) 1234);
            expectEquals (opts.timeoutMs, (juce::int64) 20000);
            expect (opts.verbose);
            expectEquals (opts.numRepeats, 11);
            expect (opts.randomiseTestOrder);
            expect (opts.withGUI == false);
            expect (opts.sampleRates == std::vector<double> ({ 22050.0, 44100.0 }));
            expect (opts.blockSizes == std::vector<int> ({ 32, 64 }));
            expect (opts.realtimeCheck == RealtimeCheck::relaxed);
        }

        beginTest ("Command line overrides environment variables");
        {
            setEnv ("STRICTNESS_LEVEL", "3");
            const auto envOnly = parse ("--validate x").strictnessLevel;
            const auto cliWins = parse ("--strictness-level 9 --validate x").strictnessLevel;
            clearKnownEnv();

            expectEquals (envOnly, 3);   // env only
            expectEquals (cliWins, 9);   // CLI wins
        }

        beginTest ("Config file and precedence (CLI > env > config > defaults)");
        {
            juce::TemporaryFile configFile (".json");
            configFile.getFile().replaceWithText (R"({ "strictnessLevel": 2, "timeoutMs": 12345, "numRepeats": 4 })");
            const auto cfg = "--config " + configFile.getFile().getFullPathName().quoted();

            // config alone
            {
                const auto s = parse (cfg + " --validate x");
                expectEquals (s.strictnessLevel, 2);
                expectEquals ((juce::int64) s.timeoutMs, (juce::int64) 12345);
                expectEquals (s.numRepeats, 4);
            }

            // env overrides config
            {
                setEnv ("STRICTNESS_LEVEL", "6");
                const auto s = parse (cfg + " --validate x");
                clearKnownEnv();
                expectEquals (s.strictnessLevel, 6);                 // env beats config
                expectEquals ((juce::int64) s.timeoutMs, (juce::int64) 12345);     // still from config
            }

            // CLI overrides env and config
            {
                setEnv ("STRICTNESS_LEVEL", "6");
                const auto s = parse (cfg + " --strictness-level 9 --validate x");
                clearKnownEnv();
                expectEquals (s.strictnessLevel, 9);
                expectEquals ((juce::int64) s.timeoutMs, (juce::int64) 12345);
            }
        }

        beginTest ("Child-process command line round-trip");
        {
            PluginTests::Options opts;
            opts.strictnessLevel    = 8;
            opts.randomSeed         = 8334753;
            opts.timeoutMs          = 15000;
            opts.verbose            = true;
            opts.numRepeats         = 3;
            opts.randomiseTestOrder = true;
            opts.withGUI            = false;
            opts.outputFilename     = "log.txt";
            opts.disabledTests      = juce::StringArray ({ "Test A", "Test B" });
            opts.sampleRates        = { 44100.0, 96000.0 };
            opts.blockSizes         = { 64, 512 };
            opts.realtimeCheck      = RealtimeCheck::relaxed;

            const juce::String fileOrID = "/some/dir/MyPlugin.vst3";

            const auto args = createCommandLine (fileOrID, opts);

            juce::StringArray childArgs (args);
            childArgs.remove (0); // drop the executable path
            const auto childCommandLine = childArgs.joinIntoString (" ");

            const auto [fileOrID2, opts2] = parseCommandLine (childCommandLine);

            auto expected = PluginvalSettings::fromPluginTestOptions (opts, fileOrID);
            expected.validatePath = settings_parser::resolvePluginPath (fileOrID).toStdString();

            expect (PluginvalSettings::fromPluginTestOptions (opts2, fileOrID2) == expected);
        }
    }
};

static CommandLineTests commandLineTests;
