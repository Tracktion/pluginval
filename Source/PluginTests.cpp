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
#include <random>

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
    : UnitTest ("pluginval"),
      fileOrID (fileOrIdentifier),
      options (opts)
{
    jassert (fileOrIdentifier.isNotEmpty());
    jassert (isPositiveAndNotGreaterThan (options.strictnessLevel, 10));
    formatManager.addDefaultFormats();
}

PluginTests::PluginTests (const PluginDescription& desc, Options opts)
    : PluginTests (String(), opts)
{
    typesFound.add (new PluginDescription (desc));
}

String PluginTests::getFileOrID() const
{
    if (fileOrID.isNotEmpty())
        return fileOrID;

    if (auto first = typesFound.getFirst())
        return first->createIdentifierString();

    return {};
}

void PluginTests::logVerboseMessage (const String& message)
{
    // We still need to send an empty message or the test may timeout
    logMessage (options.verbose ? message : String());
}

void PluginTests::resetTimeout()
{
    logMessage (String());
}

void PluginTests::runTest()
{
    // This has to be called on a background thread to keep the message thread free
    jassert (! juce::MessageManager::existsAndIsCurrentThread());

    logMessage ("Validation started: " + Time::getCurrentTime().toString (true, true) + "\n");
    logMessage ("Strictness level: " + String (options.strictnessLevel));

    if (fileOrID.isNotEmpty())
    {
        beginTest ("Scan for plugins located in: " + fileOrID);
        WaitableEvent completionEvent;

        MessageManager::callAsync ([&, this]() mutable
                                   {
                                       knownPluginList.scanAndAddDragAndDroppedFiles (formatManager, StringArray (fileOrID), typesFound);
                                       completionEvent.signal();
                                   });
        completionEvent.wait();

        logMessage ("Num plugins found: " + String (typesFound.size()));
        expect (! typesFound.isEmpty(),
                "No types found. This usually means the plugin binary is missing or damaged, "
                "an incompatible format or that it is an AU that isn't found by macOS so can't be created.");
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
    StopwatchTimer totalTimer;
    logMessage ("\nTesting plugin: " + pd.createIdentifierString());
    logMessage (pd.manufacturerName + ": " + pd.name + " v" + pd.version);

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
            logMessage (String ("Running tests 123 times").replace ("123", String (options.numRepeats)));

            // This sleep is here to allow time for plugin async initialisation as in most cases
            // plugins will be added to tracks and then be played a little time later. This sleep
            // allows time for this initialisation for tests otherwise they might complete without
            // actually having processed anything.
            // The exception to this is if the plugin is being rendered there's likely to be no gap
            // between construction, initialisation and processing. For this case, plugins should
            // check AudioProcessor::isNonRealtime and force initialisation if rendering.
            Thread::sleep (150);
            auto r = getRandom();

            for (int testRun = 0; testRun < options.numRepeats; ++testRun)
            {
                if (options.numRepeats > 1)
                    logMessage ("\nTest run: " + String (testRun + 1));

                Array<PluginTest*> testsToRun = PluginTest::getAllTests();

                if (options.randomiseTestOrder)
                {
                    std::mt19937 random (static_cast<unsigned int> (r.nextInt()));
                    std::shuffle (testsToRun.begin(), testsToRun.end(), random);
                }

                for (auto t : testsToRun)
                {
                    if (options.strictnessLevel < t->strictnessLevel
                        || (! options.withGUI && t->requiresGUI()))
                       continue;

                    if (options.disabledTests.contains (t->name))
                    {
                        logMessage ("\nINFO: \"" + t->name + "\" is disabled.");
                        continue;
                    }

                    StopwatchTimer sw2;
                    beginTest (t->name);

                    if (t->needsToRunOnMessageThread())
                    {
                        WaitableEvent completionEvent;
                        MessageManager::callAsync ([&, this]() mutable
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

            deletePluginAsync (std::move (instance));
        }
    }

    logMessage ("\nTime taken to run all tests: " + totalTimer.getDescription());
}
