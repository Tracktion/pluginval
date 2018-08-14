/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    The UnitTest which will create the plugins and run each of the registered tests on them.
*/
struct PluginTests : public UnitTest
{
    //==============================================================================
    /** A set of options to use when running tests. */
    struct Options
    {
        int strictnessLevel = 5;    /**< Max test level to run. */
        int64 timeoutMs = 30000;    /**< Timeout after which to kill the test. */
        bool verbose = false;       /**< Whether or not to log additional information. */
        bool noGui = false;         /**< Whether or not avoid tests that instantiate a gui. */
        File dataFile;              /**< File which tests can use to run user provided data. */
    };

    /** Creates a set of tests for a fileOrIdentifier. */
    PluginTests (const String& fileOrIdentifier, Options);

    /** Creates a set of tests for a PluginDescription. */
    PluginTests (const PluginDescription&, Options);

    //==============================================================================
    /** Returns the set of options currently being used to run the tests. */
    Options getOptions() const                       { return options; }

    /** Logs a verbose message which may be ommited from logs if the verbose option is not specified. */
    void logVerboseMessage (const String& message);

    //==============================================================================
    /** @internal. */
    void runTest() override;

private:
    const String fileOrID;
    const Options options;
    AudioPluginFormatManager formatManager;
    KnownPluginList knownPluginList;

    OwnedArray<PluginDescription> typesFound;

    std::unique_ptr<AudioPluginInstance> testOpenPlugin (const PluginDescription&);
    void testType (const PluginDescription&);
};

//==============================================================================
/**
    Represents a test to be run on a plugin instance.
    Override the runTest test method to perform the tests.
    Create a static instance of any subclasses to automatically register tests.
*/
struct PluginTest
{
    //==============================================================================
    /** Describes some requirements of the test, allowing the runner to execute
        the test in a suitable environment.
     */
    struct Requirements
    {
        /** By default tests are run on a background thread. Set this if you need
            the runTest method to be called on the message thread.
         */
        enum Thread { backgroundThread, messageThread };

        /** Some test environments may not allow gui windows. set this to true
            if your test tries to create an editor to
         */
        enum Gui { noGui, requiresGui };

        Thread thread = Thread::backgroundThread;
        Gui gui = Gui::noGui;
    };

    static Requirements getDefaultRequirements() { return {}; }

    //==============================================================================
    /**
        Creates a named PluginTest.
        @param testName                 The name of the test
        @param testStrictnessLevel      The conformance level of the test
    */
    PluginTest (const String& testName,
                int testStrictnessLevel,
                Requirements testRequirements)
        : name (testName),
          strictnessLevel (testStrictnessLevel),
          requirements (testRequirements)
    {
        jassert (isPositiveAndNotGreaterThan (strictnessLevel, 10));
        getAllTests().add (this);
    }

    /** Destructor. */
    virtual ~PluginTest()
    {
        getAllTests().removeFirstMatchingValue (this);
    }

    /** Returns a static list of all the tests. */
    static Array<PluginTest*>& getAllTests()
    {
        static Array<PluginTest*> tests;
        return tests;
    }

    //==============================================================================
    /** Override to perform any tests.
        Note that because PluginTest doesn't inherit from UnitTest (due to being passed
        in the AudioPluginInstance), you can use the UnitTest parameter to log messages or
        call expect etc.
    */
    virtual void runTest (PluginTests& runningTest, AudioPluginInstance&) = 0;

    //==============================================================================
    bool needsToRunOnMessageThread() const { return requirements.thread == Requirements::messageThread; }
    bool requiresGui() const { return requirements.gui == Requirements::requiresGui; }

    const String name;
    const int strictnessLevel;
    const Requirements requirements;
};
