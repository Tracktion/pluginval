/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "PluginTests.h"
#include "TestUtilities.h"

PluginTests::PluginTests (const String& fileOrIdentifier, int strictnessLevelToTest)
    : UnitTest ("PluginValidator"),
      fileOrID (fileOrIdentifier),
      strictnessLevel (strictnessLevelToTest)
{
    jassert (isPositiveAndNotGreaterThan (strictnessLevelToTest, 10));
    formatManager.addDefaultFormats();
}

PluginTests::PluginTests (const PluginDescription& desc, int strictnessLevelToTest)
    : PluginTests (String(), strictnessLevelToTest)
{
    typesFound.add (new PluginDescription (desc));
}

void PluginTests::runTest()
{
    logMessage ("Validation started: " + Time::getCurrentTime().toString (true, true) + "\n");

    if (fileOrID.isNotEmpty())
    {
        beginTest ("Scan for known types: " + fileOrID);
        knownPluginList.scanAndAddDragAndDroppedFiles (formatManager, StringArray (fileOrID), typesFound);
        logMessage ("Num types found: " + String (typesFound.size()));
        expect (! typesFound.isEmpty(), "No types found");
    }

    for (auto pd : typesFound)
        testType (*pd);
}

std::unique_ptr<AudioPluginInstance> PluginTests::testOpenPlugin (const PluginDescription& pd)
{
    String errorMessage;
    auto instance = std::unique_ptr<AudioPluginInstance> (formatManager.createPluginInstance (pd, 44100.0, 512, errorMessage));
    expectEquals (errorMessage, String());
    expect (instance != nullptr, "Unable to create AudioPluginInstance");

    return instance;
}

void PluginTests::testType (const PluginDescription& pd)
{
    const auto idString = pd.createIdentifierString();
    logMessage ("\nTesting plugin: " + idString);

    {
        beginTest ("Open plugin (cold)");
        StopwatchTimer sw;
        testOpenPlugin (pd);
        logMessage ("\nTime taken to open plugin (cold): " + sw.getDescription());
    }

    {
        beginTest ("Open plugin (warm)");
        StopwatchTimer sw;

        if (auto instance = testOpenPlugin (pd))
        {
            logMessage ("\nTime taken to open plugin (warm): " + sw.getDescription());

            for (auto t : PluginTest::getAllTests())
            {
                if (strictnessLevel < t->strictnessLevel)
                    continue;

                StopwatchTimer sw2;
                beginTest (t->name);

                if (t->needsToRunOnMessageThread())
                {
                    WaitableEvent completionEvent;
                    MessageManager::getInstance()->callAsync ([&, this]() mutable
                                                              {
                                                                  t->runTest (*this, *instance);
                                                                  completionEvent.signal();
                                                              });
                    completionEvent.wait();
                }
                else
                {
                    t->runTest (*this, *instance);
                }

                logMessage ("\nTime taken to run test: " + sw2.getDescription());
            }
        }
    }
}
