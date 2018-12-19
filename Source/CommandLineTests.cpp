/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/


struct CommandLineTests : public UnitTest
{
    CommandLineTests()
        : UnitTest ("CommandLineTests", "pluginval")
    {
    }

    void runTest() override
    {
        
        beginTest ("Merge environment variables");
        {
            StringPairArray envVars;
            envVars.set ("STRICTNESS_LEVEL", "5");
            envVars.set ("RANDOM_SEED", "1234");
            envVars.set ("TIMEOUT_MS", "30000");
            envVars.set ("VERBOSE", "1");
            envVars.set ("REPEAT", "10");
            envVars.set ("RANDOMISE", "1");
            envVars.set ("VALIDATE_IN_PROCESS", "1");
            envVars.set ("SKIP_GUI_TESTS", "1");
            envVars.set ("DATA_FILE", "<path_to_file>");
            envVars.set ("OUTPUT_DIR", "<path_to_dir>");

            const auto merged = mergeEnvironmentVariables (String(), [&envVars] (const String& n, const String& def) { return envVars.getValue (n, def); }).joinIntoString (" ");
            expect (merged.contains ("--strictness-level 5"));
            expect (merged.contains ("--random-seed 1234"));
            expect (merged.contains ("--timeout-ms 30000"));
            expect (merged.contains ("--verbose"));
            expect (merged.contains ("--repeat 10"));
            expect (merged.contains ("--randomise"));
            expect (merged.contains ("--validate-in-process"));
            expect (merged.contains ("--skip-gui-tests"));
            expect (merged.contains ("--data-file <path_to_file>"));
            expect (merged.contains ("--output-dir <path_to_dir>"));
        }

        beginTest ("Command line defaults");
        {
            ArgumentList args ({}, "");
            expectEquals (getStrictnessLevel (args), 5);
            expectEquals (getRandomSeed (args), (int64) 0);
            expectEquals (getTimeout (args), (int64) 30000);
            expectEquals (getNumRepeats (args), 1);
            expectEquals (getOptionValue (args, "--data-file", {}, "Missing data-file path argument!").toString(), String());
            expectEquals (getOptionValue (args, "--output-dir", {}, "Missing output-dir path argument!").toString(), String());
        }

        beginTest ("Command line parser");
        {
            ArgumentList args ({}, "--strictness-level 7 --random-seed 1234 --timeout-ms 20000 --repeat 11 --data-file <path_to_file> --output-dir <path_to_dir>");
            expectEquals (getStrictnessLevel (args), 7);
            expectEquals (getRandomSeed (args), (int64) 1234);
            expectEquals (getTimeout (args), (int64) 20000);
            expectEquals (getNumRepeats (args), 11);
            expectEquals (getOptionValue (args, "--data-file", {}, "Missing data-file path argument!").toString(), String ("<path_to_file>"));
            expectEquals (getOptionValue (args, "--output-dir", {}, "Missing output-dir path argument!").toString(), String ("<path_to_dir>"));
        }

        beginTest ("Command line random");
        {
            expectEquals (getRandomSeed (ArgumentList ({}, "--random-seed 0x7f2da1")), (int64) 8334753);
            expectEquals (getRandomSeed (ArgumentList ({}, "--random-seed 0x692bc1f")), (int64) 110279711);
        }
    }
};

static CommandLineTests commandLineTests;
