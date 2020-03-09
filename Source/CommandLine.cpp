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


//==============================================================================
void exitWithError (const String& error)
{
    std::cout << error << std::endl << std::endl;
    JUCEApplication::getInstance()->setApplicationReturnValue (1);
    JUCEApplication::getInstance()->quit();
}

static void hideDockIcon()
{
   #if JUCE_MAC
    Process::setDockIconVisible (false);
   #endif
}

//==============================================================================
static String& getCurrentID()
{
    static String currentID;
    return currentID;
}

void setCurrentID (const String& currentID)
{
    getCurrentID() = currentID;
}

//==============================================================================
CommandLineValidator::CommandLineValidator()
{
    validator.addChangeListener (this);
    validator.addListener (this);
}

CommandLineValidator::~CommandLineValidator()
{
}

void CommandLineValidator::validate (const StringArray& fileOrIDs, PluginTests::Options options, bool validateInProcess)
{
    validator.setValidateInProcess (validateInProcess);
    validator.validate (fileOrIDs, options);
}

void CommandLineValidator::changeListenerCallback (ChangeBroadcaster*)
{
    if (! validator.isConnected() && currentID.isNotEmpty())
        exitWithError ("\n*** FAILED: VALIDATION CRASHED" + getCrashLog());
}

void CommandLineValidator::validationStarted (const String& id)
{
    setCurrentID (id);
    currentID = id;
    logMessage ("Started validating: " + id);
}

void CommandLineValidator::logMessage (const String& m)
{
    std::cout << m << "\n";
}

void CommandLineValidator::itemComplete (const String& id, int numItemFailures)
{
    logMessage ("\nFinished validating: " + id);

    if (numItemFailures == 0)
        logMessage ("ALL TESTS PASSED");
    else
        logMessage ("*** FAILED: " + String (numItemFailures) + " TESTS");

    numFailures += numItemFailures;
    setCurrentID (String());
    currentID = String();
}

void CommandLineValidator::allItemsComplete()
{
    if (numFailures > 0)
        exitWithError ("*** FAILED: " + String (numFailures) + " TESTS");
    else
        JUCEApplication::getInstance()->quit();
}

void CommandLineValidator::connectionLost()
{
    if (currentID.isNotEmpty())
        exitWithError ("\n*** FAILED: VALIDATION CRASHED WHILST VALIDATING " + currentID + getCrashLog());
    else
        exitWithError ("\n*** FAILED: VALIDATION CRASHED" + getCrashLog());
}


//==============================================================================
static inline ArgumentList::Argument getArgumentAfterOption (const ArgumentList& args, StringRef option)
{
    for (int i = 0; i < args.size() - 1; ++i)
        if (args[i] == option)
            return args[i + 1];

    return {};
}

static var getOptionValue (const ArgumentList& args, StringRef option, var defaultValue, StringRef errorMessage)
{
    if (args.containsOption (option))
    {
        const auto nextArg = getArgumentAfterOption (args, option);

        if (nextArg.isShortOption() || nextArg.isLongOption())
            ConsoleApplication::fail (errorMessage, -1);

        return nextArg.text;
    }

    return defaultValue;
}

static int getStrictnessLevel (const ArgumentList& args)
{
    return jlimit (1, 10, (int) getOptionValue (args, "--strictness-level", 5, "Missing strictness level argument! (Must be between 1 - 10)"));
}

int64 getRandomSeed (const ArgumentList& args)
{
    const String seedString = getOptionValue (args, "--random-seed", "0", "Missing random seed argument!").toString();

    if (! seedString.containsOnly ("x-0123456789acbdef"))
        ConsoleApplication::fail ("Invalid random seed argument!", -1);

    if (seedString.startsWith ("0x"))
        return seedString.getHexValue64();

    return seedString.getLargeIntValue();
}

static int64 getTimeout (const ArgumentList& args)
{
    return getOptionValue (args, "--timeout-ms", 30000, "Missing timeout-ms level argument!");
}

static int getNumRepeats (const ArgumentList& args)
{
    return jmax (1, (int) getOptionValue (args, "--repeat", 1, "Missing repeat argument! (Must be greater than 0)"));
}

File getDataFile (const ArgumentList& args)
{
    return getOptionValue (args, "--data-file", {}, "Missing data-file path argument!").toString();
}

File getOutputDir (const ArgumentList& args)
{
    return getOptionValue (args, "--output-dir", {}, "Missing output-dir path argument!").toString();
}

StringArray getDisabledTest (const ArgumentList& args)
{
    const File disabledTestsFile (getOptionValue (args, "--disabled-tests", {}, "Missing disabled-tests path argument!").toString());
    
    StringArray disabledTests;
    disabledTestsFile.readLines (disabledTests);
  
    return disabledTests;
}

//==============================================================================
String parseCommandLineArgs (String commandLine)
{
    if (commandLine.contains ("strictnessLevel"))
    {
        std::cout << "!!! WARNING:\n\t\"strictnessLevel\" is deprecated and will be removed in a future version.\n"
                  << "\tPlease use --strictness-level instead\n\n";
    }

    return commandLine.replace ("strictnessLevel", "strictness-level")
                      .replace ("-NSDocumentRevisionsDebugMode YES", "")
                      .trim();
}


//==============================================================================
struct Option
{
    const char* name;
    bool requiresValue;
};

String getEnvironmentVariableName (Option opt)
{
    return String (opt.name).trimCharactersAtStart ("-").replace ("-", "_").toUpperCase();
}

static Option possibleOptions[] =
{
    { "--strictness-level",     true    },
    { "--random-seed",          true    },
    { "--timeout-ms",           true    },
    { "--verbose",              true    },
    { "--validate-in-process",  false   },
    { "--skip-gui-tests",       false   },
    { "--data-file",            true    },
    { "--output-dir",           true    },
    { "--repeat",               true    },
    { "--randomise",            false   }
};

StringArray mergeEnvironmentVariables (StringArray args, std::function<String (const String& name, const String& defaultValue)> environmentVariableProvider = [] (const String& name, const String& defaultValue) { return SystemStats::getEnvironmentVariable (name, defaultValue); })
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
static String getHelpMessage()
{
    const String appName (JUCEApplication::getInstance()->getApplicationName());

    String help;
    help << "//==============================================================================" << newLine
         << appName << newLine
         << SystemStats::getJUCEVersion() << newLine
         << newLine
         << "Description: " << newLine
         << "  Validate plugins to test compatibility with hosts and verify plugin API conformance" << newLine << newLine
         << "Usage: "
         << newLine
         << "  --validate [list]" << newLine
         << "    Validates the files (or IDs for AUs)." << newLine
         << "  --strictness-level [1-10]" << newLine
         << "    Sets the strictness level to use. A minimum level of 5 (also the default) is recomended for compatibility. Higher levels include longer, more thorough tests such as fuzzing." << newLine
         << "  --random-seed [hex or int]" << newLine
         << "    Sets the random seed to use for the tests. Useful for replicating test environments." << newLine
         << "  --timeout-ms [numMilliseconds]" << newLine
         << "    Sets a timout which will stop validation with an error if no output from any test has happened for this number of ms." << newLine
         << "    By default this is 30s but can be set to -1 to never timeout." << newLine
         << "  --verbose" << newLine
         << "    If specified, outputs additional logging information. It can be useful to turn this off when building with CI to avoid huge log files." << newLine
         << "  --validate-in-process" << newLine
         << "    If specified, validates the list in the calling process. This can be useful for debugging or when using the command line." << newLine
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

static String getVersionText()
{
    return String (ProjectInfo::projectName) + " - " + ProjectInfo::versionString;
}

static void validate (CommandLineValidator& validator, const ArgumentList& args)
{
    const int startIndex = args.indexOfOption ("--validate");

    if (startIndex == -1)
    {
        jassertfalse;
        return;
    }

    StringArray fileOrIDs;

    for (int i = startIndex + 1; i < args.size(); ++i)
    {
        const auto& arg = args[i];

        if (arg.isLongOption() || arg.isShortOption())
            break;

        fileOrIDs.add (arg.text);
    }

    if (! fileOrIDs.isEmpty())
    {
        const bool validateInProcess = args.containsOption ("--validate-in-process");
        PluginTests::Options options;
        options.strictnessLevel = getStrictnessLevel (args);
        options.randomSeed = getRandomSeed (args);
        options.timeoutMs = getTimeout (args);
        options.verbose = args.containsOption ("--verbose");
        options.numRepeats = getNumRepeats (args);
        options.randomiseTestOrder = args.containsOption ("--randomise");
        options.dataFile = getDataFile (args);
        options.outputDir = getOutputDir (args);
        options.withGUI = ! args.containsOption ("--skip-gui-tests");
        options.disabledTests = getDisabledTest (args);

        validator.validate (fileOrIDs,
                            options,
                            validateInProcess);
    }
}

//==============================================================================
void performCommandLine (CommandLineValidator& validator, const ArgumentList& args)
{
    hideDockIcon();

    ConsoleApplication cli;
    cli.addVersionCommand ("--version", getVersionText());
    cli.addHelpCommand ("--help|-h", getHelpMessage(), true);
    cli.addCommand ({ "--validate",
                      "--validate [list]",
                      "Validates the files (or IDs for AUs).", String(),
                      [&validator] (const auto& args) { validate (validator, args); }});

    JUCEApplication::getInstance()->setApplicationReturnValue (cli.findAndRunCommand (args));

    // --validate runs async so will quit itself when done
    if (! args.containsOption ("--validate"))
        JUCEApplication::getInstance()->quit();
}

bool shouldPerformCommandLine (const ArgumentList& args)
{
    return args.containsOption ("--help|-h")
        || args.containsOption ("--version")
        || args.containsOption ("--validate");
}

//==============================================================================
void performCommandLine (CommandLineValidator& validator, const String& commandLine)
{
    StringArray args;
    args.addTokens (parseCommandLineArgs (commandLine), true);
    args = mergeEnvironmentVariables (args);
    args.trim();

    for (auto& s : args)
        s = s.unquoted();

    performCommandLine (validator, ArgumentList (File::getSpecialLocation (File::currentExecutableFile).getFullPathName(), args));
}

bool shouldPerformCommandLine (const String& commandLine)
{
    return shouldPerformCommandLine (ArgumentList (File::getSpecialLocation (File::currentExecutableFile).getFullPathName(), commandLine));
}


//==============================================================================
//==============================================================================
#include "CommandLineTests.cpp"
