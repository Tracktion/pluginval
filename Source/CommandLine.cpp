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


//==============================================================================
struct CommandLineError
{
    CommandLineError (const String& s) : message (s) {}

    const String message;
};

void exitWithError (const String& error)
{
    std::cout << error << std::endl << std::endl;
    JUCEApplication::getInstance()->setApplicationReturnValue (1);
    JUCEApplication::getInstance()->quit();
}


//==============================================================================
static bool matchArgument (const String& arg, const String& possible)
{
    return arg == possible
        || arg == "-" + possible
        || arg == "--" + possible;
}


static int indexOfArgument (const StringArray& args, const String& possible)
{
    for (auto& a : args)
    {
        if (a == possible
            || a == "-" + possible
            || a == "--" + possible)
           return args.indexOf (a);
    }

    return -1;
}

static bool containsArgument (const StringArray& args, const String& possible)
{
    return indexOfArgument (args, possible) != -1;
}

static void checkArgumentCount (const StringArray& args, int minNumArgs)
{
    if (args.size() < minNumArgs)
        throw CommandLineError ("Not enough arguments!");
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

void handleCrash (void*)
{
    const auto id = getCurrentID();

    if (id.isNotEmpty())
        std::cout << "\n*** FAILED: VALIDATION CRASHED WHILST VALIDATING " << getCurrentID() << std::endl;
    else
        std::cout << "\n*** FAILED: VALIDATION CRASHED" << std::endl;
}

//==============================================================================
CommandLineValidator::CommandLineValidator()
{
    SystemStats::setApplicationCrashHandler (handleCrash);

    validator.addChangeListener (this);
    validator.addListener (this);
}

CommandLineValidator::~CommandLineValidator()
{
}

void CommandLineValidator::validate (const StringArray& fileOrIDs, int strictnessLevel, bool validateInProcess)
{
    validator.setValidateInProcess (validateInProcess);
    validator.validate (fileOrIDs, strictnessLevel);
}

void CommandLineValidator::changeListenerCallback (ChangeBroadcaster*)
{
    if (! validator.isConnected() && currentID.isNotEmpty())
        exitWithError ("\n*** FAILED: VALIDATION CRASHED");
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

    JUCEApplication::getInstance()->quit();
}

void CommandLineValidator::connectionLost()
{
    if (currentID.isNotEmpty())
        exitWithError ("\n*** FAILED: VALIDATION CRASHED WHILST VALIDATING " + currentID);
    else
        exitWithError ("\n*** FAILED: VALIDATION CRASHED");
}


//==============================================================================
static int getStrictnessLevel (const StringArray& args)
{
    const int strictnessIndex = indexOfArgument (args, "strictness-level");

    if (strictnessIndex != -1)
    {
        if (args.size() > strictnessIndex)
        {
            const int strictness = args[strictnessIndex + 1].getIntValue();

            if (strictness > 1 && strictness <= 10)
                return strictness;
        }

        throw CommandLineError ("Missing strictness level argument! (Must be between 1 - 10)");
    }

    return 5;
}

static void validate (CommandLineValidator& validator, const StringArray& args)
{
    hideDockIcon();
    checkArgumentCount (args, 2);
    const int startIndex = indexOfArgument (args, "validate");

    if (startIndex != -1)
    {
        StringArray fileOrIDs;
        fileOrIDs.addArray (args, startIndex + 1);

        for (int i = fileOrIDs.size(); --i >= 0;)
            if (fileOrIDs.getReference (i).startsWith ("--"))
                fileOrIDs.remove (i);

        if (! fileOrIDs.isEmpty())
            validator.validate (fileOrIDs,
                                getStrictnessLevel (args),
                                containsArgument (args, "validate-in-process"));
    }
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
static void showHelp()
{
    hideDockIcon();

    const String appName (JUCEApplication::getInstance()->getApplicationName());

    std::cout << "//=============================================================================="
              << appName << std::endl
              << SystemStats::getJUCEVersion() << std::endl
              << std::endl
              << "Description: " << std::endl
              << "  Validate plugins to test compatibility with hosts and verify plugin API conformance" << std::endl << std::endl
              << "Usage: "
              << std::endl
              << "  --validate [list]" << std::endl
              << "    Validates the files (or IDs for AUs)." << std::endl
              << "  --strictness-level [1-10]" << std::endl
              << "    Sets the strictness level to use. A minimum level of 5 (also the default) is recomended for compatibility. Higher levels include longer, more thorough tests such as fuzzing." << std::endl
              << "  --validate-in-process" << std::endl
              << "    If specified, validates the list in the calling process. This can be useful for debugging or when using the command line." << std::endl
              << std::endl
              << "Exit code: "
              << std::endl
              << "  0 if all tests complete successfully" << std::endl
              << "  1 if there are any errors" << std::endl;
}


//==============================================================================
void performCommandLine (CommandLineValidator& validator, const String& commandLine)
{
    StringArray args;
    args.addTokens (parseCommandLineArgs (commandLine), true);
    args.trim();

    for (auto& s : args)
        s = s.unquoted();

    String command (args[0]);

    try
    {
        if (matchArgument (command, "help"))                     { showHelp(); }
        if (matchArgument (command, "h"))                        { showHelp(); }
        if (containsArgument (args, "validate"))                 { validate (validator, args); return; }
    }
    catch (const CommandLineError& error)
    {
        std::cout << error.message << std::endl << std::endl;
        JUCEApplication::getInstance()->setApplicationReturnValue (1);
    }

    // If we get here the command has completed so quit the app
    JUCEApplication::getInstance()->quit();
}

bool shouldPerformCommandLine (const String& commandLine)
{
    StringArray args;
    args.addTokens (commandLine, true);
    args.trim();

    String command (args[0]);

    if (matchArgument (command, "help"))        return true;
    if (matchArgument (command, "h"))           return true;
    if (containsArgument (args, "validate"))    return true;

    return false;
}

