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

            const auto merged = mergeEnvironmentVariables (juce::String(), [&envVars] (const juce::String& n, const juce::String& def) { return envVars.getValue (n, def); }).joinIntoString (" ");
            expect (merged.contains ("--strictness-level 5"));
            expect (merged.contains ("--random-seed 1234"));
            expect (merged.contains ("--timeout-ms 30000"));
            expect (merged.contains ("--verbose"));
            expect (merged.contains ("--repeat 10"));
            expect (merged.contains ("--randomise"));
            expect (merged.contains ("--skip-gui-tests"));
            expect (merged.contains ("--data-file <path_to_file>"));
            expect (merged.contains ("--output-dir <path_to_dir>"));
        }

        beginTest ("Command line defaults");
        {
            juce::ArgumentList args ({}, "");
            expectEquals (getStrictnessLevel (args), 5);
            expectEquals (getRandomSeed (args), (juce::int64) 0);
            expectEquals (getTimeout (args), (juce::int64) 30000);
            expectEquals (getNumRepeats (args), 1);
            expectEquals (getOptionValue (args, "--data-file", {}, "Missing data-file path argument!").toString(), juce::String());
            expectEquals (getOptionValue (args, "--output-dir", {}, "Missing output-dir path argument!").toString(), juce::String());
        }

        beginTest ("Command line parser");
        {
            juce::ArgumentList args ({}, "--strictness-level 7 --random-seed 1234 --timeout-ms 20000 --repeat 11 --data-file /path/to/file --output-dir /path/to/dir --validate /path/to/plugin");
            expectEquals (getStrictnessLevel (args), 7);
            expectEquals (getRandomSeed (args), (juce::int64) 1234);
            expectEquals (getTimeout (args), (juce::int64) 20000);
            expectEquals (getNumRepeats (args), 11);
            expectEquals (getOptionValue (args, "--data-file", {}, "Missing data-file path argument!").toString(),juce::String ("/path/to/file"));
            expectEquals (getOptionValue (args, "--output-dir", {}, "Missing output-dir path argument!").toString(),juce::String ("/path/to/dir"));
            expectEquals (getOptionValue (args, "--validate", {}, "Missing validate argument!").toString(),juce::String ("/path/to/plugin"));
        }

        beginTest ("Handles an absolute path to the plugin");
        {
            const auto homeDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName();
            const auto commandLineString = "--validate " + homeDir + "/path/to/MyPlugin";
            const auto args = createCommandLineArgs (commandLineString);
            expectEquals (parseCommandLine (args).first, homeDir + "/path/to/MyPlugin");
        }

        beginTest ("Handles a quoted absolute path to the plugin");
        {
            const auto homeDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName();
            const auto pathToQuote = homeDir + "/path/to/MyPlugin";
            const auto commandLineString = "--validate " + pathToQuote.quoted();
            const auto args = createCommandLineArgs (commandLineString);
            expectEquals (parseCommandLine (args).first, homeDir + "/path/to/MyPlugin");
        }

        beginTest ("Handles a relative path");
        {
            const auto currentDir = juce::File::getCurrentWorkingDirectory();
            const auto args = createCommandLineArgs ("--validate MyPlugin.vst3");
            expectEquals (parseCommandLine (args).first, currentDir.getChildFile ("MyPlugin.vst3").getFullPathName());
        }

        beginTest ("Handles a quoted relative path with spaces to the plugin");
        {
            const auto currentDir = juce::File::getCurrentWorkingDirectory();
            const auto args = createCommandLineArgs (R"(--validate "My Plugin.vst3")");
            expectEquals (parseCommandLine (args).first, currentDir.getChildFile ("My Plugin.vst3").getFullPathName());
        }

        #if !JUCE_WINDOWS

        beginTest ("Handles a relative path with ./ to the plugin");
        {
            const auto currentDir = juce::File::getCurrentWorkingDirectory().getFullPathName();
            const auto commandLineString = "--validate ./path/to/MyPlugin";
            const auto args = createCommandLineArgs(commandLineString);
            expectEquals (parseCommandLine (args).first, currentDir + "/path/to/MyPlugin");
        }

        beginTest ("Handles a home directory relative path to the plugin");
        {
            const auto commandLineString = "--validate ~/path/to/MyPlugin";
            const auto args = createCommandLineArgs(commandLineString);
            expectEquals (parseCommandLine (args).first, juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName() + "/path/to/MyPlugin");
        }

        beginTest ("Handles quoted strings, spaces, and home directory relative path to the plugin");
        {
            const auto commandLineString = R"(--data-file "~/path/to/My File" --output-dir "~/path/to/My Directory" --validate "~/path/to/My Plugin")";
            const auto args = createCommandLineArgs(commandLineString);
            expectEquals (parseCommandLine (args).first, juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName() + "/path/to/My Plugin");
        }
        #endif

        beginTest ("Implicit validate with a relative path");
        {
            const auto currentDir = juce::File::getCurrentWorkingDirectory();
            const auto args = createCommandLineArgs ("MyPlugin.vst3");
            expectEquals (parseCommandLine (args).first, currentDir.getChildFile ("MyPlugin.vst3").getFullPathName());
        }

        beginTest ("Doesn't alter component IDs");
        {
            const auto commandLineString = "--validate MyPluginID";
            const auto args = createCommandLineArgs(commandLineString);
            expectEquals (parseCommandLine (args).first,juce::String ("MyPluginID"));
        }

        beginTest ("Command line random");
        {
            expectEquals (getRandomSeed (juce::ArgumentList ({}, "--random-seed 0x7f2da1")), (juce::int64) 8334753);
            expectEquals (getRandomSeed (juce::ArgumentList ({}, "--random-seed 0x692bc1f")), (juce::int64) 110279711);
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
            const auto args = createCommandLineArgs ("--validate MyPlugin.vst3 --randomise");
            expectEquals (parseCommandLine (args).first, currentDir.getChildFile ("MyPlugin.vst3").getFullPathName());
            expect (parseCommandLine(args).second.randomiseTestOrder);
        }
    }
};

static CommandLineTests commandLineTests;
