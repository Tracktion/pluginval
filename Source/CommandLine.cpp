/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "CommandLine.h"
#include "Validator.h"
#include "CrashHandler.h"

#if JUCE_MAC
 #include <signal.h>
 #include <sys/types.h>
 #include <unistd.h>
#endif


//==============================================================================
static void exitWithError (const juce::String& error)
{
    std::cout << error << std::endl << std::endl;
    juce::JUCEApplication::getInstance()->setApplicationReturnValue (1);
    juce::JUCEApplication::getInstance()->quit();
}

static void hideDockIcon()
{
   #if JUCE_MAC
     juce::Process::setDockIconVisible (false);
   #endif
}

//==============================================================================
#if JUCE_MAC
static void killWithoutMercy (int)
{
    kill (getpid(), SIGKILL);
}

static void setupSignalHandling()
{
    const int signals[] = { SIGFPE, SIGILL, SIGSEGV, SIGBUS, SIGABRT };

    for (int i = 0; i < juce::numElementsInArray (signals); ++i)
    {
        ::signal (signals[i], killWithoutMercy);
        ::siginterrupt (signals[i], 1);
    }
}
#endif


//==============================================================================
//==============================================================================
CommandLineValidator::CommandLineValidator()
{
   #if JUCE_MAC
    setupSignalHandling();
   #endif
}

CommandLineValidator::~CommandLineValidator()
{
}

void CommandLineValidator::validate (const juce::String& fileOrID, PluginTests::Options options)
{
    validator = std::make_unique<ValidationPass> (fileOrID, options, ValidationType::inProcess,
                                                  [] (auto id)
                                                  {
                                                      std::cout << "Started validating: " << id << std::endl;
                                                  },
                                                  [] (auto, uint32_t exitCode)
                                                  {
                                                      if (exitCode > 0)
                                                          exitWithError ("*** FAILED");
                                                      else
                                                          juce::JUCEApplication::getInstance()->quit();
                                                  },
                                                  [] (auto m)
                                                  {
                                                      std::cout << m << std::flush;
                                                  });
}


//==============================================================================
//==============================================================================
namespace
{
    juce::ArgumentList::Argument getArgumentAfterOption (const juce::ArgumentList& args, juce::StringRef option)
    {
        for (int i = 0; i < args.size() - 1; ++i)
            if (args[i] == option)
                return args[i + 1];

        return {};
    }

    juce::var getOptionValue (const juce::ArgumentList& args, juce::StringRef option, juce::var defaultValue, juce::StringRef errorMessage)
    {
        if (args.containsOption (option))
        {
            const auto nextArg = getArgumentAfterOption (args, option);

            if (nextArg.isShortOption() || nextArg.isLongOption())
                juce::ConsoleApplication::fail (errorMessage, -1);

            return nextArg.text;
        }

        return defaultValue;
    }

    int getStrictnessLevel (const juce::ArgumentList& args)
    {
        return juce::jlimit (1, 10, (int) getOptionValue (args, "--strictness-level", 5, "Missing strictness level argument! (Must be between 1 - 10)"));
    }

    juce::int64 getRandomSeed (const juce::ArgumentList& args)
    {
        const juce::String seedString = getOptionValue (args, "--random-seed", "0", "Missing random seed argument!").toString();

        if (! seedString.containsOnly ("x-0123456789acbdef"))
            juce::ConsoleApplication::fail ("Invalid random seed argument!", -1);

        if (seedString.startsWith ("0x"))
            return seedString.getHexValue64();

        return seedString.getLargeIntValue();
    }

    juce::int64 getTimeout (const juce::ArgumentList& args)
    {
        return getOptionValue (args, "--timeout-ms", 30000, "Missing timeout-ms level argument!");
    }

    int getNumRepeats (const juce::ArgumentList& args)
    {
        return juce::jmax (1, (int) getOptionValue (args, "--repeat", 1, "Missing repeat argument! (Must be greater than 0)"));
    }

    juce::File getDataFile (const juce::ArgumentList& args)
    {
        return getOptionValue (args, "--data-file", {}, "Missing data-file path argument!").toString();
    }

    juce::File getOutputDir (const juce::ArgumentList& args)
    {
        return getOptionValue (args, "--output-dir", {}, "Missing output-dir path argument!").toString();
    }

    std::vector<double> getSampleRates (const juce::ArgumentList& args)
    {
        juce::StringArray input = juce::StringArray::fromTokens (getOptionValue (args,
                                                                     "--sample-rates",
                                                                    juce::String ("44100,48000,96000"),
                                                                     "Missing sample rate list argument!")
                                                         .toString(),
                                                     ",",
                                                     "\"");
        std::vector<double> output;

        for (juce::String sr : input)
            output.push_back (sr.getDoubleValue());

        return output;
    }

    std::vector<int> getBlockSizes (const juce::ArgumentList& args)
    {
        juce::StringArray input = juce::StringArray::fromTokens (getOptionValue (args,
                                                                     "--block-sizes",
                                                                    juce::String ("64,128,256,512,1024"),
                                                                     "Missing block size list argument!")
                                                         .toString(),
                                                     ",",
                                                     "\"");
        std::vector<int> output;

        for (juce::String sr : input)
            output.push_back (sr.getIntValue());

        return output;
    }

    juce::StringArray getDisabledTests (const juce::ArgumentList& args)
    {
        const auto value = getOptionValue (args, "--disabled-tests", {}, "Missing disabled-tests path argument!").toString();

        if (juce::File::isAbsolutePath (value))
        {
            const juce::File disabledTestsFile (value);

            juce::StringArray disabledTests;
            disabledTestsFile.readLines (disabledTests);

            return disabledTests;
        }

        return juce::StringArray::fromTokens (value, ",", "");
    }

    bool isPluginArgument (juce::String arg)
    {
        juce::AudioPluginFormatManager formatManager;
        formatManager.addDefaultFormats();

        for (auto format : formatManager.getFormats())
            if (format->fileMightContainThisPluginType (arg))
                return true;

        // The above will check if the file actually exists which isn't really what we want for CLI parsing
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

//==============================================================================
struct Option
{
    const char* name;
    bool requiresValue;
};

static juce::String getEnvironmentVariableName (Option opt)
{
    return juce::String (opt.name).trimCharactersAtStart ("-").replace ("-", "_").toUpperCase();
}

static Option possibleOptions[] =
{
    { "--strictness-level",     true    },
    { "--random-seed",          true    },
    { "--timeout-ms",           true    },
    { "--verbose",              true    },
    { "--skip-gui-tests",       false   },
    { "--data-file",            true    },
    { "--output-dir",           true    },
    { "--repeat",               true    },
    { "--randomise",            false   },
    { "--sample-rates",         true    },
    { "--block-sizes",          true    },
    { "--vst3validator",        true    },
};

static juce::StringArray mergeEnvironmentVariables (juce::StringArray args, std::function<juce::String (const juce::String& name, const juce::String& defaultValue)> environmentVariableProvider = [] (const juce::String& name, const juce::String& defaultValue) { return juce::SystemStats::getEnvironmentVariable (name, defaultValue); })
{
    for (auto arg : possibleOptions)
    {
        auto envVarName = getEnvironmentVariableName (arg);
        auto envVarValue = environmentVariableProvider (envVarName, {});

        if (envVarValue.isNotEmpty())
        {
            const int index = args.indexOf (arg.name);

            if (index != -1)
            {
                std::cout << "Skipping environment variable " << envVarName << " due to " << arg.name << " set" << std::endl;
                continue;
            }

            if (arg.requiresValue)
                args.insert (0, envVarValue);

            args.insert (0, arg.name);
        }
    }

    return args;
}


//==============================================================================
//==============================================================================
static juce::String getHelpMessage()
{
    const juce::String appName (juce::JUCEApplication::getInstance()->getApplicationName());

    auto& newLine = juce::newLine;
    juce::String help;
    help << "//==============================================================================" << newLine
         << appName << newLine
         << juce::SystemStats::getJUCEVersion() << newLine
         << newLine
         << "Description: " << newLine
         << "  Validate plugins to test compatibility with hosts and verify plugin API conformance" << newLine << newLine
         << "Usage: "
         << newLine
         << "  --validate [pathToPlugin]" << newLine
         << "    Validates the plugin at the given path." << newLine
         << "    N.B. the --validate flag is optional if the path is the last argument. This enables you to validate a plugin with simply \"pluginval path_to_plugin\"." << newLine
         << "  --strictness-level [1-10]" << newLine
         << "    Sets the strictness level to use. A minimum level of 5 (also the default) is recomended for compatibility. Higher levels include longer, more thorough tests such as fuzzing." << newLine
         << "  --random-seed [hex or int]" << newLine
         << "    Sets the random seed to use for the tests. Useful for replicating test environments." << newLine
         << "  --timeout-ms [numMilliseconds]" << newLine
         << "    Sets a timout which will stop validation with an error if no output from any test has happened for this number of ms." << newLine
         << "    By default this is 30s but can be set to -1 to never timeout." << newLine
         << "  --verbose" << newLine
         << "    If specified, outputs additional logging information. It can be useful to turn this off when building with CI to avoid huge log files." << newLine
         << "  --skip-gui-tests" << newLine
         << "    If specified, avoids tests that create GUI windows, which can cause problems on headless CI systems." << newLine
         << "  --repeat [num repeats]" << newLine
         << "    If specified repeats the tests a given number of times. Note that this does not delete and re-instantiate the plugin for each repeat."
         << "  --randomise" << newLine
         << "    If specified the tests are run in a random order per repeat."
         << "  --data-file [pathToFile]" << newLine
         << "    If specified, sets a path to a data file which can be used by tests to configure themselves. This can be useful for things like known audio output." << newLine
         << "  --output-dir [pathToDir]" << newLine
         << "    If specified, sets a directory to store the log files. This can be useful for continuous integration." << newLine
         << "  --disabled-tests [pathToFile]" << newLine
         << "    If specified, sets a path to a file that should have the names of disabled tests on each row." << newLine
         << "  --sample-rates [list of comma separated sample rates]" << newLine
         << "    If specified, sets the list of sample rates at which tests will be executed (default=44100,48000,96000)" << newLine
         << "  --block-sizes [list of comma separated block sizes]" << newLine
         << "    If specified, sets the list of block sizes at which tests will be executed (default=64,128,256,512,1024)" << newLine
         << "  --vst3validator [pathToValidator]" << newLine
         << "    If specified, this will run the VST3 validator as part of the test process." << newLine
         << "  --version" << newLine
         << "    Print pluginval version." << newLine
         << newLine
         << "Exit code: "
         << newLine
         << "  0 if all tests complete successfully" << newLine
         << "  1 if there are any errors" << newLine
         << newLine
         << "Additionally, you can specify any of the command line options as environment varibles by removing prefix dashes,"
            " converting internal dashes to underscores and captialising all letters e.g. \"--skip-gui-tests\" > \"SKIP_GUI_TESTS=1\","
            " \"--timeout-ms 30000\" > \"TIMEOUT_MS=30000\"" << newLine
         << "Specifying specific command-line options will override any environment variables set for that option." << newLine;

    return help;
}

static juce::String getVersionText()
{
    return juce::String ("pluginval") + " - " + VERSION;
}

static int getNumTestFailures (juce::UnitTestRunner& testRunner)
{
    int numFailures = 0;

    for (int i = 0; i < testRunner.getNumResults(); ++i)
        if (auto result = testRunner.getResult (i))
            numFailures += result->failures;

    return numFailures;
}

static void runUnitTests()
{
    juce::UnitTestRunner testRunner;
    testRunner.runTestsInCategory ("pluginval");
    const int numFailures = getNumTestFailures (testRunner);

    if (numFailures > 0)
        juce::ConsoleApplication::fail (juce::String (numFailures) + " tests failed!!!");
}

//==============================================================================
static juce::ArgumentList createCommandLineArgs (juce::String commandLine)
{
    if (commandLine.contains ("strictnessLevel"))
    {
        std::cout << "!!! WARNING:\n\t\"strictnessLevel\" is deprecated and will be removed in a future version.\n"
                  << "\tPlease use --strictness-level instead\n\n";
    }

    commandLine = commandLine.replace ("strictnessLevel", "strictness-level")
                             .replace ("-NSDocumentRevisionsDebugMode YES", "")
                             .trim();

    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

    juce::StringArray args;
    args.addTokens (commandLine, true);
    args = mergeEnvironmentVariables (args);
    args.trim();

    for (auto& s : args)
        s = s.unquoted();

    // If only a plugin path is supplied as the last arg, add an implicit --validate
    // option for it so the rest of the CLI works
    juce::ArgumentList argList (exe.getFullPathName(), args);

    if (argList.size() > 0)
    {
        const bool hasValidateOrOtherCommand = argList.containsOption ("--validate")
                                                || argList.containsOption ("--help|-h")
                                                || argList.containsOption ("--version")
                                                || argList.containsOption ("--run-tests");

        if (! hasValidateOrOtherCommand)
            if (isPluginArgument (argList.arguments.getLast().text))
                argList.arguments.insert (argList.arguments.size() - 1, { "--validate" });
    }

    return argList;
}

static void performCommandLine (CommandLineValidator& validator, const juce::ArgumentList& args)
{
    hideDockIcon();

    juce::ConsoleApplication cli;
    cli.addVersionCommand ("--version", getVersionText());
    cli.addHelpCommand ("--help|-h", getHelpMessage(), true);
    cli.addCommand ({ "--validate",
                      "--validate [pathToPlugin]",
                      "Validates the file (or IDs for AUs).", juce::String(),
                      [&validator] (const auto& validatorArgs)
                      {
                          auto [fileOrIDToValidate, options] = parseCommandLine (validatorArgs);
                          validator.validate (fileOrIDToValidate, options);
                      }});
    cli.addCommand ({ "--run-tests",
                      "--run-tests",
                      "Runs the internal unit tests.", juce::String(),
                      [] (const auto&) { runUnitTests(); }});

    if (const auto retValue = cli.findAndRunCommand (args); retValue != 0)
    {
        juce::JUCEApplication::getInstance()->setApplicationReturnValue (retValue);
        juce::JUCEApplication::getInstance()->quit();
    }

    // --validate runs async so will quit itself when done
    if (! args.containsOption ("--validate"))
        juce::JUCEApplication::getInstance()->quit();
}

//==============================================================================
void performCommandLine (CommandLineValidator& validator, const juce::String& commandLine)
{
    performCommandLine (validator, createCommandLineArgs (commandLine));
}

bool shouldPerformCommandLine (const juce::String& commandLine)
{
    const auto args = createCommandLineArgs (commandLine);
    return args.containsOption ("--help|-h")
        || args.containsOption ("--version")
        || args.containsOption ("--validate")
        || args.containsOption ("--run-tests");
}

//==============================================================================
//==============================================================================
std::pair<juce::String, PluginTests::Options> parseCommandLine (const juce::ArgumentList& args)
{
    auto fileOrID = getOptionValue (args, "--validate", "", "Expected a plugin path for the --validate option").toString();

    // in the case of a path (vs. ID), grab the full path
    // getCurrentWorkingDirectory is needed to handle relative paths
    // It preserves absolute paths and first checks for ~ on Mac/Windows
    if (fileOrID.contains ("~") || fileOrID.contains ("."))
        fileOrID = juce::File::getCurrentWorkingDirectory().getChildFile(fileOrID).getFullPathName();

    PluginTests::Options options;
    options.strictnessLevel     = getStrictnessLevel (args);
    options.randomSeed          = getRandomSeed (args);
    options.timeoutMs           = getTimeout (args);
    options.verbose             = args.containsOption ("--verbose");
    options.numRepeats          = getNumRepeats (args);
    options.randomiseTestOrder  = args.containsOption ("--randomise");
    options.dataFile            = getDataFile (args);
    options.outputDir           = getOutputDir (args);
    options.withGUI             = ! args.containsOption ("--skip-gui-tests");
    options.disabledTests       = getDisabledTests (args);
    options.sampleRates         = getSampleRates (args);
    options.blockSizes          = getBlockSizes (args);
    options.vst3Validator       = getOptionValue (args, "--vst3validator", "", "Expected a path for the --vst3validator option");

    return { fileOrID, options };
}

std::pair<juce::String, PluginTests::Options> parseCommandLine (const juce::String& cmd)
{
    return parseCommandLine (createCommandLineArgs (cmd));
}

juce::StringArray createCommandLine (juce::String fileOrID, PluginTests::Options options)
{
    juce::StringArray args (juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName());
    const PluginTests::Options defaults;

    if (options.strictnessLevel != defaults.strictnessLevel)
        args.addArray ({ "--strictness-level", juce::String (options.strictnessLevel) });

    if (options.randomSeed != defaults.randomSeed)
        args.addArray ({ "--random-seed", juce::String (options.randomSeed) });

    if (options.timeoutMs != defaults.timeoutMs)
        args.addArray ({ "--timeout-ms", juce::String (options.timeoutMs) });

    if (options.verbose)
        args.add ("--verbose");

    if (! options.withGUI)
        args.add ("--skip-gui-tests");

    if (options.numRepeats != defaults.numRepeats)
        args.addArray ({ "--repeat", juce::String (options.numRepeats) });

    if (options.randomiseTestOrder)
        args.add ("--randomise");

    if (options.dataFile != defaults.dataFile)
        args.addArray ({ "--data-file", options.dataFile.getFullPathName() });

    if (options.outputDir != defaults.outputDir)
        args.addArray ({ "--output-dir", options.outputDir.getFullPathName() });

    if (options.disabledTests != defaults.disabledTests)
        args.addArray ({ "--disabled-tests", options.disabledTests.joinIntoString (",") });

    if (! options.sampleRates.empty())
    {
        juce::StringArray sampleRates;

        for (auto rate : options.sampleRates)
            sampleRates.add (juce::String (rate));

        args.addArray ({ "--sample-rates", sampleRates.joinIntoString (",") });
    }

    if (! options.blockSizes.empty())
    {
        juce::StringArray blockSizes;

        for (auto size : options.blockSizes)
            blockSizes.add (juce::String (size));

        args.addArray ({ "--block-sizes", blockSizes.joinIntoString (",") });
    }

    if (options.vst3Validator != juce::File())
        args.addArray ({ "--vst3validator", options.vst3Validator.getFullPathName().quoted() });

    args.addArray ({ "--validate", fileOrID });

    return args;
}

//==============================================================================
//==============================================================================
#include "CommandLineTests.cpp"
