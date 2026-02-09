/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/


struct CommandLineTests : public juce::UnitTest
{
    CommandLineTests()
        : juce::UnitTest ("CommandLineTests", "pluginval")
    {
    }

    void runTest() override
    {
        beginTest ("Merge environment variables");
        {
            juce::StringPairArray envVars;
            envVars.set ("STRICTNESS_LEVEL", "5");
            envVars.set ("RANDOM_SEED", "1234");
            envVars.set ("TIMEOUT_MS", "30000");
            envVars.set ("VERBOSE", "1");
            envVars.set ("REPEAT", "10");
            envVars.set ("RANDOMISE", "1");
            envVars.set ("SKIP_GUI_TESTS", "1");
            envVars.set ("DATA_FILE", "<path_to_file>");
            envVars.set ("OUTPUT_DIR", "<path_to_dir>");

            const auto merged = preProcessCommandLine (juce::String(),
                [&envVars] (const juce::String& n, const juce::String& def) { return envVars.getValue (n, def); });

            // Convert to a single string for contains checks
            juce::StringArray mergedArr;
            for (const auto& s : merged)
                mergedArr.add (juce::String (s));
            auto mergedStr = mergedArr.joinIntoString (" ");

            expect (mergedStr.contains ("--strictness-level 5"));
            expect (mergedStr.contains ("--random-seed 1234"));
            expect (mergedStr.contains ("--timeout-ms 30000"));
            expect (mergedStr.contains ("--verbose"));
            expect (mergedStr.contains ("--repeat 10"));
            expect (mergedStr.contains ("--randomise"));
            expect (mergedStr.contains ("--skip-gui-tests"));
            expect (mergedStr.contains ("--data-file <path_to_file>"));
            expect (mergedStr.contains ("--output-dir <path_to_dir>"));
        }

        beginTest ("Command line defaults");
        {
            auto settings = parseCommandLineToSettings ("--validate DummyPlugin");
            expect (settings.has_value(), "Parse should succeed for --validate with dummy path");

            if (settings)
            {
                expectEquals (settings->strictnessLevel, 5);
                expectEquals (settings->randomSeed, (int64_t) 0);
                expectEquals (settings->timeoutMs, (int64_t) 30000);
                expectEquals (settings->numRepeats, 1);
                expect (settings->dataFile.empty());
                expect (settings->outputDir.empty());
            }
        }

        beginTest ("Command line parser");
        {
            auto settings = parseCommandLineToSettings (
                "--strictness-level 7 --random-seed 1234 --timeout-ms 20000 --repeat 11 "
                "--data-file /path/to/file --output-dir /path/to/dir --validate /path/to/plugin");
            expect (settings.has_value(), "Parse should succeed");

            if (settings)
            {
                expectEquals (settings->strictnessLevel, 7);
                expectEquals (settings->randomSeed, (int64_t) 1234);
                expectEquals (settings->timeoutMs, (int64_t) 20000);
                expectEquals (settings->numRepeats, 11);
                expectEquals (juce::String (settings->dataFile), juce::String ("/path/to/file"));
                expectEquals (juce::String (settings->outputDir), juce::String ("/path/to/dir"));
                expectEquals (juce::String (settings->pluginPath), juce::String ("/path/to/plugin"));
            }
        }

        beginTest ("Handles an absolute path to the plugin");
        {
            const auto homeDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName();
            const auto commandLineString = "--validate " + homeDir + "/path/to/MyPlugin";
            auto settings = parseCommandLineToSettings (commandLineString);
            expect (settings.has_value());
            if (settings)
                expectEquals (juce::String (settings->pluginPath), homeDir + "/path/to/MyPlugin");
        }

        beginTest ("Handles a quoted absolute path to the plugin");
        {
            const auto homeDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName();
            const auto pathToQuote = homeDir + "/path/to/MyPlugin";
            const auto commandLineString = "--validate " + pathToQuote.quoted();
            auto settings = parseCommandLineToSettings (commandLineString);
            expect (settings.has_value());
            if (settings)
                expectEquals (juce::String (settings->pluginPath), homeDir + "/path/to/MyPlugin");
        }

        beginTest ("Handles a relative path");
        {
            const auto currentDir = juce::File::getCurrentWorkingDirectory();
            auto settings = parseCommandLineToSettings ("--validate MyPlugin.vst3");
            expect (settings.has_value());
            if (settings)
                expectEquals (juce::String (settings->pluginPath), currentDir.getChildFile ("MyPlugin.vst3").getFullPathName());
        }

        beginTest ("Handles a quoted relative path with spaces to the plugin");
        {
            const auto currentDir = juce::File::getCurrentWorkingDirectory();
            auto settings = parseCommandLineToSettings (R"(--validate "My Plugin.vst3")");
            expect (settings.has_value());
            if (settings)
                expectEquals (juce::String (settings->pluginPath), currentDir.getChildFile ("My Plugin.vst3").getFullPathName());
        }

        #if !JUCE_WINDOWS

        beginTest ("Handles a relative path with ./ to the plugin");
        {
            const auto currentDir = juce::File::getCurrentWorkingDirectory().getFullPathName();
            auto settings = parseCommandLineToSettings ("--validate ./path/to/MyPlugin");
            expect (settings.has_value());
            if (settings)
                expectEquals (juce::String (settings->pluginPath), currentDir + "/path/to/MyPlugin");
        }

        beginTest ("Handles a home directory relative path to the plugin");
        {
            auto settings = parseCommandLineToSettings ("--validate ~/path/to/MyPlugin");
            expect (settings.has_value());
            if (settings)
                expectEquals (juce::String (settings->pluginPath), juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName() + "/path/to/MyPlugin");
        }

        beginTest ("Handles quoted strings, spaces, and home directory relative path to the plugin");
        {
            auto settings = parseCommandLineToSettings (
                R"(--data-file "~/path/to/My File" --output-dir "~/path/to/My Directory" --validate "~/path/to/My Plugin")");
            expect (settings.has_value());
            if (settings)
                expectEquals (juce::String (settings->pluginPath), juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName() + "/path/to/My Plugin");
        }
        #endif

        beginTest ("Implicit validate with a relative path");
        {
            const auto currentDir = juce::File::getCurrentWorkingDirectory();
            auto settings = parseCommandLineToSettings ("MyPlugin.vst3");
            expect (settings.has_value());
            if (settings)
                expectEquals (juce::String (settings->pluginPath), currentDir.getChildFile ("MyPlugin.vst3").getFullPathName());
        }

        beginTest ("Doesn't alter component IDs");
        {
            auto settings = parseCommandLineToSettings ("--validate MyPluginID");
            expect (settings.has_value());
            if (settings)
                expectEquals (juce::String (settings->pluginPath), juce::String ("MyPluginID"));
        }

        beginTest ("Command line random hex seeds");
        {
            auto settings1 = parseCommandLineToSettings ("--random-seed 0x7f2da1 --validate DummyPlugin");
            expect (settings1.has_value());
            if (settings1)
                expectEquals (settings1->randomSeed, (int64_t) 8334753);

            auto settings2 = parseCommandLineToSettings ("--random-seed 0x692bc1f --validate DummyPlugin");
            expect (settings2.has_value());
            if (settings2)
                expectEquals (settings2->randomSeed, (int64_t) 110279711);
        }

        beginTest ("Implicit validate options");
        {
            juce::TemporaryFile temp ("path_to_file.vst3");
            expect (temp.getFile().create());
            expect (shouldPerformCommandLine (temp.getFile().getFullPathName()));
        }

        beginTest ("Allows for other options after explicit --validate");
        {
            const auto currentDir = juce::File::getCurrentWorkingDirectory();
            auto settings = parseCommandLineToSettings ("--validate MyPlugin.vst3 --randomise");
            expect (settings.has_value());
            if (settings)
            {
                expectEquals (juce::String (settings->pluginPath), currentDir.getChildFile ("MyPlugin.vst3").getFullPathName());
                expect (settings->randomise);
            }
        }

        beginTest ("Round-trip: settings -> CLI args -> parse -> settings");
        {
            PluginvalSettings original;
            original.pluginPath = "/path/to/test.vst3";
            original.strictnessLevel = 7;
            original.randomSeed = 42;
            original.timeoutMs = 60000;
            original.verbose = true;
            original.numRepeats = 3;
            original.randomise = true;
            original.skipGuiTests = true;
            original.sampleRates = { 44100, 96000 };
            original.blockSizes = { 256, 512 };

            auto cmdLine = original.toCommandLineArgs();
            // Remove the exe path (first element) and rebuild as a single string
            cmdLine.remove (0);
            auto cmdString = cmdLine.joinIntoString (" ");

            auto parsed = parseCommandLineToSettings (cmdString);
            expect (parsed.has_value(), "Round-trip parse should succeed");

            if (parsed)
            {
                expectEquals (parsed->pluginPath, original.pluginPath);
                expectEquals (parsed->strictnessLevel, original.strictnessLevel);
                expectEquals (parsed->randomSeed, original.randomSeed);
                expectEquals (parsed->timeoutMs, original.timeoutMs);
                expect (parsed->verbose == original.verbose);
                expectEquals (parsed->numRepeats, original.numRepeats);
                expect (parsed->randomise == original.randomise);
                expect (parsed->skipGuiTests == original.skipGuiTests);
                expectEquals ((int) parsed->sampleRates.size(), (int) original.sampleRates.size());
                expectEquals ((int) parsed->blockSizes.size(), (int) original.blockSizes.size());
            }
        }

        beginTest ("JSON round-trip");
        {
            PluginvalSettings original;
            original.pluginPath = "/path/to/test.vst3";
            original.strictnessLevel = 8;
            original.timeoutMs = 45000;
            original.verbose = true;
            original.sampleRates = { 48000 };
            original.blockSizes = { 512 };
            original.realtimeCheck = RealtimeCheck::relaxed;

            auto json = original.toJson();
            auto parsed = PluginvalSettings::fromJson (json);

            expectEquals (parsed.pluginPath, original.pluginPath);
            expectEquals (parsed.strictnessLevel, original.strictnessLevel);
            expectEquals (parsed.timeoutMs, original.timeoutMs);
            expect (parsed.verbose == original.verbose);
            expectEquals ((int) parsed.sampleRates.size(), 1);
            expectEquals (parsed.sampleRates[0], 48000.0);
            expectEquals ((int) parsed.blockSizes.size(), 1);
            expectEquals (parsed.blockSizes[0], 512);
            expect (parsed.realtimeCheck == RealtimeCheck::relaxed);
        }

        beginTest ("Env var + CLI override precedence");
        {
            juce::StringPairArray envVars;
            envVars.set ("STRICTNESS_LEVEL", "3");
            envVars.set ("TIMEOUT_MS", "10000");

            // CLI provides strictness-level, env provides timeout
            auto preprocessed = preProcessCommandLine (
                "--strictness-level 7 --validate DummyPlugin",
                [&envVars] (const juce::String& n, const juce::String& def) { return envVars.getValue (n, def); });

            juce::StringArray arr;
            for (const auto& s : preprocessed)
                arr.add (juce::String (s));
            auto mergedStr = arr.joinIntoString (" ");

            // CLI value should win for strictness-level
            expect (mergedStr.contains ("--strictness-level 7"), "CLI should override env var");
            // Env var should be added for timeout since CLI doesn't specify it
            expect (mergedStr.contains ("--timeout-ms 10000"), "Env var should be added when CLI doesn't specify it");
        }

        beginTest ("Custom type: CommaSeparatedDoubles");
        {
            auto settings = parseCommandLineToSettings ("--sample-rates 22050,44100 --validate DummyPlugin");
            expect (settings.has_value());
            if (settings)
            {
                expectEquals ((int) settings->sampleRates.size(), 2);
                expectEquals (settings->sampleRates[0], 22050.0);
                expectEquals (settings->sampleRates[1], 44100.0);
            }
        }

        beginTest ("Custom type: CommaSeparatedInts");
        {
            auto settings = parseCommandLineToSettings ("--block-sizes 32,64 --validate DummyPlugin");
            expect (settings.has_value());
            if (settings)
            {
                expectEquals ((int) settings->blockSizes.size(), 2);
                expectEquals (settings->blockSizes[0], 32);
                expectEquals (settings->blockSizes[1], 64);
            }
        }

        beginTest ("Rtcheck option parsing");
        {
            auto settings = parseCommandLineToSettings ("--rtcheck enabled --validate DummyPlugin");
            expect (settings.has_value());
            if (settings)
                expect (settings->realtimeCheck == RealtimeCheck::enabled);

            auto settings2 = parseCommandLineToSettings ("--rtcheck relaxed --validate DummyPlugin");
            expect (settings2.has_value());
            if (settings2)
                expect (settings2->realtimeCheck == RealtimeCheck::relaxed);
        }

        beginTest ("toTestOptions bridge");
        {
            PluginvalSettings s;
            s.strictnessLevel = 8;
            s.randomSeed = 42;
            s.timeoutMs = 60000;
            s.verbose = true;
            s.numRepeats = 3;
            s.randomise = true;
            s.skipGuiTests = true;
            s.sampleRates = { 44100 };
            s.blockSizes = { 256 };
            s.realtimeCheck = RealtimeCheck::enabled;

            auto opts = s.toTestOptions();
            expectEquals (opts.strictnessLevel, 8);
            expectEquals (opts.randomSeed, (juce::int64) 42);
            expectEquals (opts.timeoutMs, (juce::int64) 60000);
            expect (opts.verbose);
            expectEquals (opts.numRepeats, 3);
            expect (opts.randomiseTestOrder);
            expect (! opts.withGUI);
            expectEquals ((int) opts.sampleRates.size(), 1);
            expectEquals ((int) opts.blockSizes.size(), 1);
            expect (opts.realtimeCheck == RealtimeCheck::enabled);
        }
    }
};

static CommandLineTests commandLineTests;
