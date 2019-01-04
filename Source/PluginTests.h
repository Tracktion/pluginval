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
        int strictnessLevel = 5;            /**< Max test level to run. */
        int64 randomSeed = 0;               /**< The seed to use for the tests, 0 signifies a randomly generated seed. */
        int64 timeoutMs = 30000;            /**< Timeout after which to kill the test. */
        bool verbose = false;               /**< Whether or not to log additional information. */
        int numRepeats = 1;                 /**< The number of times to repeat the tests. */
        bool randomiseTestOrder = false;    /**< Whether to randomise the order of the tests in each repeat. */
        bool withGUI = true;                /**< Whether or not avoid tests that instantiate a gui. */
        File dataFile;                      /**< File which tests can use to run user provided data. */
        File outputDir;                     /**< Directory in which to write the log files for each test run. */
    };

    /** Creates a set of tests for a fileOrIdentifier. */
    PluginTests (const String& fileOrIdentifier, Options);

    /** Creates a set of tests for a PluginDescription. */
    PluginTests (const PluginDescription&, Options);

    /** Returns the file or ID used to create this. */
    String getFileOrID() const;

    /** Call this after you've run the test to return information about the PluginDescriptions found. */
    const OwnedArray<PluginDescription>& getDescriptions() const    { return typesFound; }

    //==============================================================================
    /** Returns the set of options currently being used to run the tests. */
    Options getOptions() const                       { return options; }

    /** Logs a verbose message which may be ommited from logs if the verbose option is not specified. */
    void logVerboseMessage (const String& message);

    /** Resets the timeout. Call this from long tests that don't log messages. */
    void resetTimeout();

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
        enum class Thread
        {
            backgroundThread,       /**< Test can run on a background thread. */
            messageThread           /**< Test needs to run on the message thread. */
        };

        /** Some test environments may not allow gui windows.
             Set this to true
             if your test tries to create an editor to
        */
        enum class GUI
        {
            noGUI,                  /**< Test can be run without GUI. */
            requiresGUI             /**< Test requires GUI to run. */
        };

        Thread thread = Thread::backgroundThread;
        GUI gui = GUI::noGUI;
    };

    //==============================================================================
    /**
        Creates a named PluginTest.
        @param testName                 The name of the test
        @param testStrictnessLevel      The conformance level of the test
    */
    PluginTest (const String& testName,
                int testStrictnessLevel,
                Requirements testRequirements = { Requirements::Thread::backgroundThread, Requirements::GUI::noGUI })
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
    /** Returns true if the runTest method must be called on the message thread. */
    bool needsToRunOnMessageThread() const
    {
        return requirements.thread == Requirements::Thread::messageThread;
    }

    /** Returns true if the test needs a GUI environment to run. */
    bool requiresGUI() const
    {
        return requirements.gui == Requirements::GUI::requiresGUI;
    }

    //==============================================================================
    /** Override to perform any tests.
        Note that because PluginTest doesn't inherit from UnitTest (due to being passed
        in the AudioPluginInstance), you can use the UnitTest parameter to log messages or
        call expect etc.
    */
    virtual void runTest (PluginTests& runningTest, AudioPluginInstance&) = 0;

    //==============================================================================
    const String name;
    const int strictnessLevel;
    const Requirements requirements;
};
