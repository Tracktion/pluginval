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

namespace
{
    /** Deletes a plugin asyncronously on the message thread */
    void deletePluginAsync (std::unique_ptr<AudioPluginInstance> pluginInstance)
    {
        WaitableEvent completionEvent;

        struct AsyncDeleter : public CallbackMessage
        {
            AsyncDeleter (std::unique_ptr<AudioPluginInstance> api, WaitableEvent& we)
                : instance (std::move (api)), event (we)
            {}

            void messageCallback() override
            {
                instance.reset();
                Thread::sleep (150); // Pause a few ms to let the plugin clean up after itself
                event.signal();
            }

            std::unique_ptr<AudioPluginInstance> instance;
            WaitableEvent& event;
        };

        (new AsyncDeleter (std::move (pluginInstance), completionEvent))->post();
        completionEvent.wait();
    }
}

PluginTests::PluginTests (const String& fileOrIdentifier, Options opts)
    : UnitTest ("PluginValidator"),
      fileOrID (fileOrIdentifier),
      options (opts)
{
    jassert (isPositiveAndNotGreaterThan (options.strictnessLevel, 10));
    formatManager.addDefaultFormats();
}

PluginTests::PluginTests (const PluginDescription& desc, Options opts)
    : PluginTests (String(), opts)
{
    typesFound.add (new PluginDescription (desc));
}

void PluginTests::logVerboseMessage (const String& message)
{
    // We still need to send an empty message or the test may timeout
    logMessage (options.verbose ? message : String());
}

void PluginTests::runTest()
{
    logMessage ("Validation started: " + Time::getCurrentTime().toString (true, true) + "\n");

    if (fileOrID.isNotEmpty())
    {
        beginTest ("Scan for known types: " + fileOrID);

        WaitableEvent completionEvent;
        MessageManager::getInstance()->callAsync ([&, this]() mutable
                                                  {
                                                      knownPluginList.scanAndAddDragAndDroppedFiles (formatManager, StringArray (fileOrID), typesFound);
                                                      completionEvent.signal();
                                                  });
        completionEvent.wait();

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
        deletePluginAsync (testOpenPlugin (pd));
        logMessage ("\nTime taken to open plugin (cold): " + sw.getDescription());
    }

    {
        beginTest ("Open plugin (warm)");
        StopwatchTimer sw;

        if (auto instance = testOpenPlugin (pd))
        {
            logMessage ("\nTime taken to open plugin (warm): " + sw.getDescription());
            Thread::sleep (150); // Allow time for plugin async initialisation

            for (auto t : PluginTest::getAllTests())
            {
                if (options.strictnessLevel < t->strictnessLevel)
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

            deletePluginAsync (std::move (instance));
        }
    }
}
