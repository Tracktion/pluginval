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

struct TestError
{
    TestError (const String& s) : message (s) {}
    TestError (const String& s, int code) : message (s), exitCode (code) {}

    const String message;
    const int exitCode = 1;
};


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
struct CommandLineValidator : private ChangeListener,
                              private Validator::Listener
{
    CommandLineValidator()
    {
        validator.addChangeListener (this);
        validator.addListener (this);
    }

    ~CommandLineValidator()
    {
        validator.removeChangeListener (this);
        validator.removeListener (this);
    }

    void validate (const StringArray& fileOrIDs, int strictnessLevel)
    {
        inProgress = true;
        validator.validate (fileOrIDs, strictnessLevel);

        while (inProgress && validator.isConnected())
            Thread::sleep (100);

        if (exitError)
            throw (*exitError);

        if (numFailures > 0)
            throw (TestError ("*** FAILED: " + String (numFailures) + " TESTS"));
    }

private:
    Validator validator;
    String currentID;
    std::atomic<bool> inProgress { false };
    std::atomic<int> numFailures { 0 };
    std::unique_ptr<TestError> exitError;

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        if (! validator.isConnected() && currentID.isNotEmpty())
        {
            logMessage ("\n*** FAILED: VALIDATION CRASHED");
            currentID = String();
        }

        if (! validator.isConnected())
            inProgress = false;
    }

    void validationStarted (const String& id) override
    {
        currentID = id;
        logMessage ("Started validating: " + id);
    }

    void logMessage (const String& m) override
    {
        std::cout << m << "\n";
    }

    void itemComplete (const String& id, int numItemFailures) override
    {
        logMessage ("\nFinished validating: " + id);

        if (numItemFailures == 0)
            logMessage ("ALL TESTS PASSED");
        else
            logMessage ("*** FAILED: " + String (numItemFailures) + " TESTS");

        numFailures += numItemFailures;
        currentID = String();
        inProgress = false;
    }

    void allItemsComplete() override
    {
    }

    void connectionLost() override
    {
        if (currentID.isNotEmpty())
        {
            logMessage ("\n*** FAILED: VALIDATION CRASHED");
            exitError = std::make_unique<TestError> ("\n*** FAILED: VALIDATION CRASHED WHILST VALIDATING " + currentID);
            currentID = String();
        }
        else
        {
            exitError = std::make_unique<TestError> ("\n*** FAILED: VALIDATION CRASHED");
        }

        inProgress = false;
    }
};

static void validate (const StringArray& args, int strictnessLevel)
{
    hideDockIcon();
    checkArgumentCount (args, 2);
    const int startIndex = indexOfArgument (args, "--validate");

    if (startIndex != -1)
    {
        StringArray fileOrIDs;
        fileOrIDs.addArray (args, startIndex + 1);

        for (int i = fileOrIDs.size(); --i >= 0;)
            if (fileOrIDs.getReference (i).startsWith ("--"))
                fileOrIDs.remove (i);

        if (! fileOrIDs.isEmpty())
            CommandLineValidator().validate (fileOrIDs, strictnessLevel);
    }
}

static int getStrictnessLevel (const StringArray& args)
{
    const int strictnessIndex = indexOfArgument (args, "strictnessLevel");

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
              << "  --strictnessLevel [1-10]" << std::endl
              << "    Sets the strictness level to use. A minimum level of 5 (also the default) is recomended for compatibility. Higher levels include longer, more thorough tests such as fuzzing." << std::endl
              << std::endl
              << "Exit code: "
              << std::endl
              << "  0 if all tests complete successfully" << std::endl
              << "  1 if there are any errors" << std::endl;
}


//==============================================================================
int performCommandLine (const String& commandLine)
{
    StringArray args;
    args.addTokens (commandLine, true);
    args.trim();

    for (auto& s : args)
        s = s.unquoted();

    String command (args[0]);

    try
    {
        if (matchArgument (command, "help"))                     { showHelp(); return 0; }
        if (matchArgument (command, "h"))                        { showHelp(); return 0; }
        if (containsArgument (args, "validate"))                 { validate (args, getStrictnessLevel (args)); return 0; }
    }
    catch (const TestError& error)
    {
        std::cout << error.message << std::endl << std::endl;
        JUCEApplication::getInstance()->setApplicationReturnValue (error.exitCode);

        return error.exitCode;
    }
    catch (const CommandLineError& error)
    {
        std::cout << error.message << std::endl << std::endl;
        return 1;
    }

    return commandLineNotPerformed;
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

