/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "../PluginTests.h"
#include "../TestUtilities.h"
#include <future>

//==============================================================================
struct PluginInfoTest   : public PluginTest
{
    PluginInfoTest()
        : PluginTest ("Plugin info", 1)
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        ut.logMessage ("\nPlugin name: " + instance.getName());
        ut.logMessage ("Alternative names: " + instance.getAlternateDisplayNames().joinIntoString ("|"));
        ut.logMessage ("SupportsDoublePrecision: " + String (instance.supportsDoublePrecisionProcessing() ? "yes" : "no"));
        ut.logMessage ("Reported latency: " + String (instance.getLatencySamples()));
        ut.logMessage ("Reported taillength: " + String (instance.getTailLengthSeconds()));
    }
};

static PluginInfoTest pluginInfoTest;


//==============================================================================
struct EditorTest   : public PluginTest
{
    EditorTest()
        : PluginTest ("Editor", 2,
                      { Requirements::Thread::messageThread, Requirements::GUI::requiresGUI })
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        if (instance.hasEditor())
        {
            StopwatchTimer timer;

            {
                ScopedEditorShower editorShower (instance);
                ut.expect (editorShower.editor != nullptr, "Unable to create editor");
                ut.logMessage ("\nTime taken to open editor (cold): " + timer.getDescription());
            }

            {
                timer.reset();
                ScopedEditorShower editorShower (instance);
                ut.expect (editorShower.editor != nullptr, "Unable to create editor on second attempt");
                ut.logMessage ("Time taken to open editor (warm): " + timer.getDescription());
            }
        }
    }
};

static EditorTest editorTest;


//==============================================================================
struct EditorWhilstProcessingTest   : public PluginTest
{
    EditorWhilstProcessingTest()
        : PluginTest ("Open editor whilst processing", 4,
                      { Requirements::Thread::messageThread, Requirements::GUI::requiresGUI })
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        if (instance.hasEditor())
        {
            instance.releaseResources();
            instance.prepareToPlay (44100.0, 512);

            const int numChannelsRequired = jmax (instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels());
            AudioBuffer<float> ab (numChannelsRequired, instance.getBlockSize());
            MidiBuffer mb;


            WaitableEvent threadStartedEvent;
            std::atomic<bool> shouldProcess { true };

            auto processThread = std::async (std::launch::async,
                                             [&]
                                             {
                                                 while (shouldProcess)
                                                 {
                                                     fillNoise (ab);
                                                     instance.processBlock (ab, mb);
                                                     mb.clear();

                                                     threadStartedEvent.signal();
                                                 }
                                             });

            threadStartedEvent.wait();

            // Show the editor
            ScopedEditorShower editor (instance);
            ut.expect (editor.editor.get() != nullptr || ! instance.hasEditor(), "Unable to create editor");

            shouldProcess = false;
        }
    }
};

static EditorWhilstProcessingTest editorWhilstProcessingTest;


//==============================================================================
struct AudioProcessingTest  : public PluginTest
{
    AudioProcessingTest()
        : PluginTest ("Audio processing", 3)
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        const bool isPluginInstrument = instance.getPluginDescription().isInstrument;
        const double sampleRates[] = { 44100.0, 48000.0, 96000.0 };
        const int blockSizes[] = { 64, 128, 256, 512, 1024 };
        const int numBlocks = 10;
        auto r = ut.getRandom();

        for (auto sr : sampleRates)
        {
            for (auto bs : blockSizes)
            {
                ut.logMessage (String ("Testing with sample rate [SR] and block size [BS]")
                            .replace ("SR", String (sr, 0), false)
                            .replace ("BS", String (bs), false));
                instance.releaseResources();
                instance.prepareToPlay (sr, bs);

                const int numChannelsRequired = jmax (instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels());
                AudioBuffer<float> ab (numChannelsRequired, bs);
                MidiBuffer mb;

                // Add a random note on if the plugin is a synth
                const int noteChannel = r.nextInt ({ 1, 17 });
                const int noteNumber = r.nextInt (128);

                if (isPluginInstrument)
                    addNoteOn (mb, noteChannel, noteNumber, jmin (10, bs));

                for (int i = 0; i < numBlocks; ++i)
                {
                    // Add note off in last block if plugin is a synth
                    if (isPluginInstrument && i == (numBlocks - 1))
                        addNoteOff (mb, noteChannel, noteNumber, 0);

                    fillNoise (ab);
                    instance.processBlock (ab, mb);
                    mb.clear();

                    ut.expectEquals (countNaNs (ab), 0, "NaNs found in buffer");
                    ut.expectEquals (countInfs (ab), 0, "Infs found in buffer");
                    ut.expectEquals (countSubnormals (ab), 0, "Subnormals found in buffer");
                }
            }
        }
    }
};

static AudioProcessingTest audioProcessingTest;


//==============================================================================
struct PluginStateTest  : public PluginTest
{
    PluginStateTest()
        : PluginTest ("Plugin state", 2)
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        auto r = ut.getRandom();

        // Read state
        MemoryBlock originalState;
        instance.getStateInformation (originalState);

        // Set random parameter values
        for (auto parameter : getNonBypassAutomatableParameters (instance))
            parameter->setValue (r.nextFloat());

        // Restore original state
        instance.setStateInformation (originalState.getData(), (int) originalState.getSize());
    }
};

static PluginStateTest pluginStateTest;


//==============================================================================
struct PluginStateTestRestoration   : public PluginTest
{
    PluginStateTestRestoration()
        : PluginTest ("Plugin state restoration", 6)
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        auto r = ut.getRandom();

        // Read state
        MemoryBlock originalState;
        instance.getStateInformation (originalState);

        // Check current sum of parameter values
        const float originalParamsSum = getParametersSum (instance);

        // Set random parameter values
        for (auto parameter : getNonBypassAutomatableParameters (instance))
            parameter->setValue (r.nextFloat());

        // Restore original state
        instance.setStateInformation (originalState.getData(), (int) originalState.getSize());

        // Check parameter values return to original
        ut.expectWithinAbsoluteError (getParametersSum (instance), originalParamsSum, 0.1f,
                                      "Parameters not restored on setStateInformation");

        if (strictnessLevel >= 8)
        {
            // Read state again and compare to what we set
            MemoryBlock duplicateState;
            instance.getStateInformation (duplicateState);
            ut.expect (duplicateState.matches (originalState.getData(), originalState.getSize()),
                       "Returned state differs from that set by host");
        }
    }
};

static PluginStateTestRestoration pluginStateTestRestoration;


//==============================================================================
struct AutomationTest  : public PluginTest
{
    AutomationTest()
        : PluginTest ("Automation", 3)
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        const bool subnormalsAreErrors = ut.getOptions().strictnessLevel > 5;
        const bool isPluginInstrument = instance.getPluginDescription().isInstrument;
        const double sampleRates[] = { 44100.0, 48000.0, 96000.0 };
        const int blockSizes[] = { 64, 128, 256, 512, 1024 };
        auto r = ut.getRandom();

        for (auto sr : sampleRates)
        {
            for (auto bs : blockSizes)
            {
                const int subBlockSize = 32;
                ut.logMessage (String ("Testing with sample rate [SR] and block size [BS] and sub-block size [SB]")
                            .replace ("SR", String (sr, 0), false)
                            .replace ("BS", String (bs), false)
                            .replace ("SB", String (subBlockSize), false));

                instance.releaseResources();
                instance.prepareToPlay (sr, bs);

                int numSamplesDone = 0;
                const int numChannelsRequired = jmax (instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels());
                AudioBuffer<float> ab (numChannelsRequired, bs);
                MidiBuffer mb;

                // Add a random note on if the plugin is a synth
                const int noteChannel = r.nextInt ({ 1, 17 });
                const int noteNumber = r.nextInt (128);

                if (isPluginInstrument)
                    addNoteOn (mb, noteChannel, noteNumber, jmin (10, subBlockSize));

                for (;;)
                {
                    // Set random parameter values
                    {
                        auto parameters = getNonBypassAutomatableParameters (instance);

                        for (int i = 0; i < jmin (10, parameters.size()); ++i)
                        {
                            const int paramIndex = r.nextInt (parameters.size());
                            parameters[paramIndex]->setValue (r.nextFloat());
                        }
                    }

                    // Create a sub-buffer and process
                    const int numSamplesThisTime = jmin (subBlockSize, bs - numSamplesDone);

                    // Trigger a note off in the last sub block
                    if (isPluginInstrument && (bs - numSamplesDone) <= subBlockSize)
                        addNoteOff (mb, noteChannel, noteNumber, jmin (10, subBlockSize));

                    AudioBuffer<float> subBuffer (ab.getArrayOfWritePointers(),
                                                  ab.getNumChannels(),
                                                  numSamplesDone,
                                                  numSamplesThisTime);
                    fillNoise (subBuffer);
                    instance.processBlock (subBuffer, mb);
                    numSamplesDone += numSamplesThisTime;

                    mb.clear();

                    if (numSamplesDone >= bs)
                        break;
                }

                ut.expectEquals (countNaNs (ab), 0, "NaNs found in buffer");
                ut.expectEquals (countInfs (ab), 0, "Infs found in buffer");

                const int subnormals = countSubnormals (ab);

                if (subnormalsAreErrors)
                    ut.expectEquals (countInfs (ab), 0, "Submnormals found in buffer");
                else if (subnormals > 0)
                    ut.logMessage ("!!! WARNGING: " + String (countSubnormals (ab)) + " submnormals found in buffer");
            }
        }
    }
};

static AutomationTest automationTest;


//==============================================================================
struct EditorAutomationTest : public PluginTest
{
    EditorAutomationTest()
        : PluginTest ("Editor Automation", 5,
                      { Requirements::Thread::backgroundThread, Requirements::GUI::requiresGUI })
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        const ScopedEditorShower editor (instance);

        auto r = ut.getRandom();
        const auto& parameters = instance.getParameters();
        int numBlocks = ut.getOptions().strictnessLevel > 5 ? 1000 : 100;

        // Set random parameter values
        while (--numBlocks >= 0)
        {
            for (auto parameter : parameters)
                parameter->setValue (r.nextFloat());

            ut.resetTimeout();
            Thread::sleep (10);
        }
    }
};

static EditorAutomationTest editorAutomationTest;


//==============================================================================
namespace ParameterHelpers
{
    static void testParameterInfo (PluginTests& ut, AudioProcessorParameter& parameter)
    {
        const int index = parameter.getParameterIndex();
        const String paramName = parameter.getName (512);

        const float defaultValue = parameter.getDefaultValue();
        const String label = parameter.getLabel();
        const int numSteps = parameter.getNumSteps();
        const bool isDiscrete = parameter.isDiscrete();
        const bool isBoolean = parameter.isBoolean();
        const StringArray allValueStrings = parameter.getAllValueStrings();

        const bool isOrientationInverted = parameter.isOrientationInverted();
        const bool isAutomatable = parameter.isAutomatable();
        const bool isMetaParameter = parameter.isMetaParameter();
        const auto category = parameter.getCategory();

        #define LOGP(x) JUCE_STRINGIFY(x) + " - " + String (x) + ", "
        #define LOGP_B(x) JUCE_STRINGIFY(x) + " - " + String (static_cast<int> (x)) + ", "
        ut.logVerboseMessage (String ("Parameter info: ")
                              + LOGP(index)
                              + LOGP(paramName)
                              + LOGP(defaultValue)
                              + LOGP(label)
                              + LOGP(numSteps)
                              + LOGP_B(isDiscrete)
                              + LOGP_B(isBoolean)
                              + LOGP_B(isOrientationInverted)
                              + LOGP_B(isAutomatable)
                              + LOGP_B(isMetaParameter)
                              + LOGP_B(category)
                              + "all value strings - " + allValueStrings.joinIntoString ("|"));
    }

    static void testParameterDefaults (PluginTests& ut, AudioProcessorParameter& parameter)
    {
        ut.logVerboseMessage ("Testing accessers");

        const float value = parameter.getValue();
        const String text = parameter.getText (value, 1024);
        const float valueForText = parameter.getValueForText (text);
        const String currentValueAsText = parameter.getCurrentValueAsText();
        ignoreUnused (value, text, valueForText, currentValueAsText);
    }
}

struct AutomatableParametersTest  : public PluginTest
{
    AutomatableParametersTest()
        : PluginTest ("Automatable Parameters", 2)
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        for (auto parameter : getNonBypassAutomatableParameters (instance))
        {
            ut.logVerboseMessage (String ("\nTesting parameter: ") + String (parameter->getParameterIndex()) + " - " + parameter->getName (512));

            ParameterHelpers::testParameterInfo (ut, *parameter);
            ParameterHelpers::testParameterDefaults (ut, *parameter);
        }
    }
};

static AutomatableParametersTest automatableParametersTest;

//==============================================================================
struct AllParametersTest    : public PluginTest
{
    AllParametersTest()
        : PluginTest ("Parameters", 7)
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        for (auto parameter : getNonBypassAutomatableParameters (instance))
        {
            ut.logVerboseMessage (String ("\nTesting parameter: ") + String (parameter->getParameterIndex()) + " - " + parameter->getName (512));

            ParameterHelpers::testParameterInfo (ut, *parameter);
            ParameterHelpers::testParameterDefaults (ut, *parameter);
        }
    }
};

static AllParametersTest allParametersTest;


//==============================================================================
/** Sets plugin state from a background thread whilst the plugin window is
    created on the main thread. This simulates behaviour seen in certain hosts.
 */
struct BackgroundThreadStateTest    : public PluginTest
{
    BackgroundThreadStateTest()
        : PluginTest ("Background thread state", 7,
                      { Requirements::Thread::backgroundThread, Requirements::GUI::requiresGUI })
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        auto r = ut.getRandom();
        ScopedEditorShower editor (instance);

        auto parameters = getNonBypassAutomatableParameters (instance);
        MemoryBlock originalState;

        // Read state
        instance.getStateInformation (originalState);

        // Set random parameter values
        for (auto parameter : parameters)
            parameter->setValue (r.nextFloat());

        // Restore original state
        instance.setStateInformation (originalState.getData(), (int) originalState.getSize());

        // Allow for async reaction to state changes
        Thread::sleep (2000);
    }
};

static BackgroundThreadStateTest backgroundThreadStateTest;


//==============================================================================
/** Sets plugin parameters from a background thread and the main thread at the
    same time, as if via host automation and UI simultenously.
*/
struct ParameterThreadSafetyTest    : public PluginTest
{
    ParameterThreadSafetyTest()
        : PluginTest ("Parameter thread safety", 7)
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        WaitableEvent startWaiter, endWaiter;
        auto r = ut.getRandom();
        auto parameters = getNonBypassAutomatableParameters (instance);
        const bool isPluginInstrument = instance.getPluginDescription().isInstrument;
        const int numBlocks = 500;

        // This emulates the plugin itself setting a value for example from a slider within its UI
        MessageManager::callAsync ([&, threadRandom = r]() mutable
                                   {
                                       startWaiter.signal();

                                       for (int i = 0; i < numBlocks; ++i)
                                           for (auto param : parameters)
                                               param->setValueNotifyingHost (threadRandom.nextFloat());

                                       endWaiter.signal();
                                   });

        const int blockSize = 32;
        instance.releaseResources();
        instance.prepareToPlay (44100.0, blockSize);

        const int numChannelsRequired = jmax (instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels());
        AudioBuffer<float> ab (numChannelsRequired, blockSize);
        MidiBuffer mb;

        // Add a random note on if the plugin is a synth
        const int noteChannel = r.nextInt ({ 1, 17 });
        const int noteNumber = r.nextInt (128);

        if (isPluginInstrument)
            addNoteOn (mb, noteChannel, noteNumber, jmin (10, blockSize));

        startWaiter.wait();

        for (int i = 0; i < numBlocks; ++i)
        {
            // Add note off in last block if plugin is a synth
            if (isPluginInstrument && i == (numBlocks - 1))
                addNoteOff (mb, noteChannel, noteNumber, 0);

            for (auto param : parameters)
                param->setValue (r.nextFloat());

            fillNoise (ab);
            instance.processBlock (ab, mb);
            mb.clear();
        }

        endWaiter.wait();
    }
};

static ParameterThreadSafetyTest parameterThreadSafetyTest;
