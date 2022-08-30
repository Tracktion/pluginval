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
    void deletePluginAsync (std::unique_ptr<juce::AudioPluginInstance> pluginInstance)
    {
        juce::WaitableEvent completionEvent;

        struct AsyncDeleter : public juce::CallbackMessage
        {
            AsyncDeleter (std::unique_ptr<juce::AudioPluginInstance> api, juce::WaitableEvent& we)
                : instance (std::move (api)), event (we)
            {}

            void messageCallback() override
            {
                instance.reset();
                juce::Thread::sleep (150); // Pause a few ms to let the plugin clean up after itself
                event.signal();
            }

            std::unique_ptr<juce::AudioPluginInstance> instance;
            juce::WaitableEvent& event;
        };

        (new AsyncDeleter (std::move (pluginInstance), completionEvent))->post();
        completionEvent.wait();
    }
}

PluginTests::PluginTests (const juce::String& fileOrIdentifier, Options opts)
    : juce::UnitTest ("pluginval"),
      fileOrID (fileOrIdentifier),
      options (opts)
{
    jassert (fileOrIdentifier.isNotEmpty());
    jassert (juce::isPositiveAndNotGreaterThan (options.strictnessLevel, 10));
    formatManager.addDefaultFormats();
}

PluginTests::PluginTests (const juce::PluginDescription& desc, Options opts)
    : PluginTests (juce::String(), opts)
{
    typesFound.add (new juce::PluginDescription (desc));
}

juce::String PluginTests::getFileOrID() const
{
    if (fileOrID.isNotEmpty())
        return fileOrID;

    if (auto first = typesFound.getFirst())
        return first->createIdentifierString();

    return {};
}

void PluginTests::logVerboseMessage (const juce::String& message)
{
    // We still need to send an empty message or the test may timeout
    logMessage (options.verbose ? message : juce::String());
}

void PluginTests::resetTimeout()
{
    logMessage (juce::String());
}

void PluginTests::runTest()
{
    // This has to be called on a background thread to keep the message thread free
    jassert (! juce::MessageManager::existsAndIsCurrentThread());

    logMessage ("Validation started");
    logVerboseMessage ("\t" + juce::Time::getCurrentTime().toString (true, true) + "\n");
    logMessage ("Strictness level: " + juce::String (options.strictnessLevel));

    if (fileOrID.isNotEmpty())
    {
        beginTest ("Scan for plugins located in: " + fileOrID);

        juce::WaitableEvent completionEvent;
        juce::MessageManager::callAsync ([&, this]() mutable
                                   {
                                       knownPluginList.scanAndAddDragAndDroppedFiles (formatManager, juce::StringArray (fileOrID), typesFound);
                                       completionEvent.signal();
                                   });
        completionEvent.wait();

        logMessage ("Num plugins found: " + juce::String (typesFound.size()));
        expect (! typesFound.isEmpty(),
                "No types found. This usually means the plugin binary is missing or damaged, "
                "an incompatible format or that it is an AU that isn't found by macOS so can't be created.");
    }

    for (auto pd : typesFound)
        testType (*pd);
}

std::unique_ptr<juce::AudioPluginInstance> PluginTests::testOpenPlugin (const juce::PluginDescription& pd)
{
    juce::String errorMessage;
    auto instance = std::unique_ptr<juce::AudioPluginInstance> (formatManager.createPluginInstance (pd, 44100.0, 512, errorMessage));
    expectEquals (errorMessage, juce::String());
    expect (instance != nullptr, "Unable to create juce::AudioPluginInstance");

    return instance;
}

void PluginTests::testType (const juce::PluginDescription& pd)
{
    StopwatchTimer totalTimer;
    logMessage ("\nTesting plugin: " + pd.createIdentifierString());
    logMessage (pd.manufacturerName + ": " + pd.name + " v" + pd.version);

    {
        beginTest ("Open plugin (cold)");
        StopwatchTimer sw;
        deletePluginAsync (testOpenPlugin (pd));
        logVerboseMessage ("\nTime taken to open plugin (cold): " + sw.getDescription());
    }

    {
        beginTest ("Open plugin (warm)");
        StopwatchTimer sw;

        if (auto instance = testOpenPlugin (pd))
        {
            logVerboseMessage ("\nTime taken to open plugin (warm): " + sw.getDescription());
            logMessage (juce::String ("Running tests 123 times").replace ("123",juce::String (options.numRepeats)));

            // This sleep is here to allow time for plugin async initialisation as in most cases
            // plugins will be added to tracks and then be played a little time later. This sleep
            // allows time for this initialisation for tests otherwise they might complete without
            // actually having processed anything.
            // The exception to this is if the plugin is being rendered there's likely to be no gap
            // between construction, initialisation and processing. For this case, plugins should
            // check AudioProcessor::isNonRealtime and force initialisation if rendering.
            juce::Thread::sleep (150);
            auto r = getRandom();

            for (int testRun = 0; testRun < options.numRepeats; ++testRun)
            {
                if (options.numRepeats > 1)
                    logMessage ("\nTest run: " + juce::String (testRun + 1));

                juce::Array<PluginTest*> testsToRun = PluginTest::getAllTests();

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
                        juce::WaitableEvent completionEvent;
                        juce::MessageManager::callAsync ([&, this]() mutable
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

                    logVerboseMessage ("\nTime taken to run test: " + sw2.getDescription());
                }
            }

            deletePluginAsync (std::move (instance));
        }
    }

    logVerboseMessage ("\nTime taken to run all tests: " + totalTimer.getDescription());
}
