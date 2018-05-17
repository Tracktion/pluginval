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
    Represents a test to be run on a plugin instance.
    Override the runTest test method to perform the tests.
    Create a static instance of any subclasses to automatically register tests.
*/
struct PluginTest
{
    /**
        Creates a named PluginTest.
        @param testName                 The name of the test
        @param testStrictnessLevel      The conformance level of the test
    */
    PluginTest (const String& testName, int testStrictnessLevel)
        : name (testName), strictnessLevel (testStrictnessLevel)
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
    /** By default tests are run on background threads, you can override this to
        return true of you need the runTest method to be called on the message thread.
    */
    virtual bool needsToRunOnMessageThread()                    { return false; }

    /** Override to perform any tests.
        Note that because PluginTest doesn't not inherit from UnitTest (due to being passed
        in the AudioPluginInstance), you can use the UnitTest parameter to log messages or
        call expect etc.
    */
    virtual void runTest (UnitTest& runningTest, AudioPluginInstance&) = 0;

    //==============================================================================
    const String name;
    const int strictnessLevel;
};


//==============================================================================
/**
    The UnitTest which will create the plugins and run each of the registered tests on them.
*/
struct PluginTests : public UnitTest
{
    /** Creates a set of tests for a fileOrIdentifier. */
    PluginTests (const String& fileOrIdentifier, int strictnessLevelToTest);

    /** Creates a set of tests for a PluginDescription. */
    PluginTests (const PluginDescription&, int strictnessLevelToTest);

    /** @internal. */
    void runTest() override;

private:
    const String fileOrID;
    const int strictnessLevel;
    AudioPluginFormatManager formatManager;
    KnownPluginList knownPluginList;

    OwnedArray<PluginDescription> typesFound;

    std::unique_ptr<AudioPluginInstance> testOpenPlugin (const PluginDescription&);
    void testType (const PluginDescription&);
};
