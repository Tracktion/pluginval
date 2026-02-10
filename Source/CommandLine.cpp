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
#include "PluginTests.h"
#include "PluginvalSettings.h"

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

inline std::mutex& getCoutMutex()
{
    static std::mutex m;
    return m;
}

inline void logLine (const juce::String& m)
{
    const std::scoped_lock sl (getCoutMutex());
    std::cout << m << std::endl;
}

inline void logAndFlush (const juce::String& m)
{
    const std::scoped_lock sl (getCoutMutex());
    std::cout << m << std::flush;
}

//==============================================================================
#if JUCE_MAC
static void kill9WithSomeMercy (int signal)
{
    juce::Logger::writeToLog ("pluginval received " + juce::String(::strsignal(signal)) + ", exiting immediately");

    // Use std::_Exit here instead of kill as kill doesn't seem to set the exit code of the process so is picked up as a "pass" in the host process
    std::_Exit (SIGKILL);
}

// Avoid showing the macOS crash dialog, which can cause the process to hang
static void setupSignalHandling()
{
    const int signals[] = { SIGFPE, SIGILL, SIGSEGV, SIGBUS, SIGABRT };

    for (int i = 0; i < juce::numElementsInArray (signals); ++i)
    {
        ::signal (signals[i], kill9WithSomeMercy);
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
                                                      logLine ("Started validating: " + id);
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
                                                      logAndFlush (m);
                                                  });
}


//==============================================================================
//==============================================================================
static void printStrictnessHelp (int level)
{
    level = juce::jlimit (1, 10, level);

    std::cout << "Tests at strictness level " << level << ":\n\n";

    auto& allTests = PluginTest::getAllTests();

    std::vector<PluginTest*> sortedTests;
    for (auto* test : allTests)
        sortedTests.push_back (test);

    std::sort (sortedTests.begin(), sortedTests.end(),
               [] (const PluginTest* a, const PluginTest* b)
               { return a->strictnessLevel < b->strictnessLevel; });

    for (auto* test : sortedTests)
    {
        if (test->strictnessLevel > level)
            continue;

        auto descriptions = test->getDescription (level);

        for (const auto& desc : descriptions)
        {
            std::cout << "  " << desc.title;
            if (desc.description.isNotEmpty())
                std::cout << ": " << desc.description;
            std::cout << "\n";
        }
    }

    std::cout << std::endl;
}

static int getNumTestFailures (juce::UnitTestRunner& testRunner)
{
    int numFailures = 0;

    for (int i = 0; i < testRunner.getNumResults(); ++i)
        if (auto result = testRunner.getResult (i))
            numFailures += result->failures;

    return numFailures;
}

static int runUnitTests()
{
    juce::UnitTestRunner testRunner;
    testRunner.runTestsInCategory ("pluginval");
    const int numFailures = getNumTestFailures (testRunner);

    if (numFailures > 0)
    {
        std::cerr << numFailures << " tests failed!!!" << std::endl;
        return 1;
    }

    return 0;
}

//==============================================================================
// Special commands that are handled before magic_args parsing
//==============================================================================
static bool handleSpecialCommands (const juce::String& commandLine)
{
    auto cleaned = commandLine.replace ("-NSDocumentRevisionsDebugMode YES", "").trim();

    juce::StringArray tokens;
    tokens.addTokens (cleaned, true);

    if (tokens.contains ("--run-tests"))
    {
        hideDockIcon();
        int failures = runUnitTests();

        if (failures > 0)
            juce::JUCEApplication::getInstance()->setApplicationReturnValue (1);

        juce::JUCEApplication::getInstance()->quit();
        return true;
    }

    if (tokens.contains ("--strictness-help"))
    {
        hideDockIcon();
        int level = 5;
        int idx = tokens.indexOf ("--strictness-help");

        if (idx + 1 < tokens.size())
        {
            auto nextArg = tokens[idx + 1];

            if (! nextArg.startsWith ("--"))
                level = nextArg.getIntValue();
        }

        printStrictnessHelp (level);
        juce::JUCEApplication::getInstance()->quit();
        return true;
    }

    return false;
}

//==============================================================================
void performCommandLine (CommandLineValidator& validator, const juce::String& commandLine)
{
    hideDockIcon();

    // Handle special commands that don't go through magic_args
    if (handleSpecialCommands (commandLine))
        return;

    // Parse with magic_args via our unified settings
    auto settings = parseCommandLineToSettings (commandLine);

    if (! settings.has_value())
    {
        // Help or version was shown, or parse error was reported
        juce::JUCEApplication::getInstance()->quit();
        return;
    }

    if (settings->pluginPath.empty())
    {
        exitWithError ("Expected a plugin path for the --validate option");
        return;
    }

    auto options = settings->toTestOptions();
    validator.validate (juce::String (settings->pluginPath), options);
}

bool shouldPerformCommandLine (const juce::String& commandLine)
{
    auto preprocessed = preProcessCommandLine (commandLine);

    for (const auto& arg : preprocessed)
    {
        if (arg == "--help" || arg == "-?" || arg == "--version"
            || arg == "--validate" || arg == "--run-tests"
            || arg == "--strictness-help")
            return true;
    }

    return false;
}


//==============================================================================
//==============================================================================
#include "CommandLineTests.cpp"
