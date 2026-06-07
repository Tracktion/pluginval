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
#include "SettingsParser.h"

#include <exception>
#include <iostream>

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

static void runUnitTests()
{
    juce::UnitTestRunner testRunner;
    testRunner.runTestsInCategory ("pluginval");
    const int numFailures = getNumTestFailures (testRunner);

    // Set the return value directly rather than juce::ConsoleApplication::fail(),
    // which throws and would terminate the process when called outside a
    // ConsoleApplication command handler.
    if (numFailures > 0)
    {
        std::cout << numFailures << " tests failed!!!" << std::endl;
        juce::JUCEApplication::getInstance()->setApplicationReturnValue (1);
    }
}

//==============================================================================
//==============================================================================
std::pair<juce::String, PluginTests::Options> parseCommandLine (const juce::String& commandLine)
{
    const auto settings = settings_parser::parse (commandLine);
    return { juce::String (settings.validatePath), settings.toPluginTestOptions() };
}

juce::StringArray createCommandLine (juce::String fileOrID, PluginTests::Options options)
{
    return settings_parser::createChildProcessCommandLine (fileOrID, options);
}

//==============================================================================
void performCommandLine (CommandLineValidator& validator, const juce::String& commandLine)
{
    hideDockIcon();

    auto& app = *juce::JUCEApplication::getInstance();
    const auto tokens = settings_parser::preprocess (commandLine);

    if (tokens.contains ("--run-tests"))
    {
        runUnitTests();
        app.quit();
        return;
    }

    if (tokens.contains ("--strictness-help"))
    {
        int level = 5;

        if (const auto idx = tokens.indexOf ("--strictness-help"); idx >= 0 && idx + 1 < tokens.size())
            if (const auto next = tokens[idx + 1]; ! next.startsWith ("-"))
                level = next.getIntValue();

        printStrictnessHelp (level);
        app.quit();
        return;
    }

    // Otherwise this is a validation run (explicit or implicit --validate).
    // CLI11 handles --help/--version and parse errors.
    try
    {
        const auto result = settings_parser::parseTokens (tokens);

        if (result.handled)
        {
            app.setApplicationReturnValue (result.exitCode);
            app.quit();
            return;
        }

        const auto fileOrID = juce::String (result.settings.validatePath);

        if (fileOrID.isEmpty())
        {
            exitWithError ("*** FAILED: No plugin path or ID specified to validate");
            return;
        }

        // --validate runs async so will quit itself when done
        validator.validate (fileOrID, result.settings.toPluginTestOptions());
    }
    catch (const std::exception& e)
    {
        exitWithError (juce::String ("*** FAILED: ") + e.what());
    }
}

bool shouldPerformCommandLine (const juce::String& commandLine)
{
    return settings_parser::isCommandLine (settings_parser::preprocess (commandLine));
}

//==============================================================================
//==============================================================================
#include "CommandLineTests.cpp"
