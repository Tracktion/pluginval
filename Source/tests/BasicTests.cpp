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
#include "../RTCheck.h"

#include <future>
#include <thread>
#include <chrono>

//==============================================================================
struct PluginInfoTest   : public PluginTest
{
    PluginInfoTest()
        : PluginTest ("Plugin info", 1,
                      { Requirements::Thread::messageThread })
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        ut.logMessage ("\nPlugin name: " + instance.getName());
        ut.logMessage ("Alternative names: " + instance.getAlternateDisplayNames().joinIntoString ("|"));
        ut.logMessage ("SupportsDoublePrecision: " + juce::String (instance.supportsDoublePrecisionProcessing() ? "yes" : "no"));
        ut.logMessage ("Reported latency: " + juce::String (instance.getLatencySamples()));
        ut.logMessage ("Reported taillength: " + juce::String (instance.getTailLengthSeconds()));
    }

    std::vector<TestDescription> getDescription (int) const override
    {
        return { { name, "Logs getName(), getAlternateDisplayNames(), supportsDoublePrecisionProcessing(), "
                         "getLatencySamples(), getTailLengthSeconds()" } };
    }
};

static PluginInfoTest pluginInfoTest;


//==============================================================================
struct PluginPrgramsTest    : public PluginTest
{
    PluginPrgramsTest()
        : PluginTest ("Plugin programs", 2)
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        const int numPrograms = instance.getNumPrograms();
        ut.logMessage ("Num programs: " + juce::String (numPrograms));

        for (int i = 0; i < numPrograms; ++i)
            ut.logVerboseMessage (juce::String ("Program 123 name: XYZ")
                                  .replace ("123",juce::String (i))
                                  .replace ("XYZ", instance.getProgramName (i)));

        ut.logMessage ("All program names checked");

        if (numPrograms > 0)
        {
            ut.logMessage ("\nChanging program");
            const int currentProgram = instance.getCurrentProgram();
            auto r = ut.getRandom();

            for (int i = 0; i < 5; ++i)
            {
                const int programNum = r.nextInt (numPrograms);
                ut.logVerboseMessage ("Changing program to: " + juce::String (programNum));
                instance.setCurrentProgram (programNum);
            }

            if (currentProgram >= 0)
            {
                ut.logVerboseMessage ("Resetting program to: " + juce::String (currentProgram));
                instance.setCurrentProgram (currentProgram);
            }
            else
            {
                ut.logMessage ("!!! WARNING: Current program is -1... Is this correct?");
            }
        }
    }

    std::vector<TestDescription> getDescription (int) const override
    {
        return { { name, "Calls getNumPrograms() and getProgramName() for each, "
                         "then randomly switches programs 5 times via setCurrentProgram()" } };
    }
};

static PluginPrgramsTest pluginPrgramsTest;


//==============================================================================
struct EditorTest   : public PluginTest
{
    EditorTest()
        : PluginTest ("Editor", 2,
                      { Requirements::Thread::messageThread, Requirements::GUI::requiresGUI })
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        if (instance.hasEditor())
        {
            StopwatchTimer timer;

            {
                ScopedEditorShower editorShower (instance);
                ut.expect (editorShower.editor != nullptr, "Unable to create editor");
                ut.logVerboseMessage ("\nTime taken to open editor (cold): " + timer.getDescription());
            }

            {
                timer.reset();
                ScopedEditorShower editorShower (instance);
                ut.expect (editorShower.editor != nullptr, "Unable to create editor on second attempt");
                ut.logVerboseMessage ("Time taken to open editor (warm): " + timer.getDescription());
            }
        }
    }

    std::vector<TestDescription> getDescription (int) const override
    {
        return { { name, "Calls createEditor() twice (cold and warm), logs time taken, "
                         "expects non-null editor pointer" } };
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

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        if (instance.hasEditor())
        {
            callReleaseResourcesOnMessageThreadIfVST3 (instance);

            const std::vector<double>& sampleRates = ut.getOptions().sampleRates;
            const std::vector<int>& blockSizes = ut.getOptions().blockSizes;

            jassert (sampleRates.size() > 0 && blockSizes.size() > 0);
            callPrepareToPlayOnMessageThreadIfVST3 (instance, sampleRates[0], blockSizes[0]);

            const int numChannelsRequired = juce::jmax (instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels());
            juce::AudioBuffer<float> ab (numChannelsRequired, instance.getBlockSize());
            juce::MidiBuffer mb;
            mb.ensureSize (32);


            juce::WaitableEvent threadStartedEvent;
            std::atomic<bool> shouldProcess { true };

            auto processThread = std::async (std::launch::async,
                                             [&]
                                             {
                                                 int blockNum = 0;

                                                 while (shouldProcess)
                                                 {
                                                     fillNoise (ab);

                                                     {
                                                         RTC_REALTIME_CONTEXT_IF_ENABLED(ut.getOptions().realtimeCheck, blockNum)
                                                         instance.processBlock (ab, mb);
                                                     }

                                                     mb.clear();
                                                     ++blockNum;

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

    std::vector<TestDescription> getDescription (int) const override
    {
        return { { name, "Starts async thread calling processBlock() repeatedly, "
                         "then calls createEditor() on message thread. Tests concurrent access" } };
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

    static void runAudioProcessingTest (PluginTests& ut, juce::AudioPluginInstance& instance,
                                        bool callReleaseResourcesBeforeSampleRateChange)
    {
        const bool isPluginInstrument = instance.getPluginDescription().isInstrument;

        const std::vector<double>& sampleRates = ut.getOptions().sampleRates;
        const std::vector<int>& blockSizes = ut.getOptions().blockSizes;

        jassert (sampleRates.size()>0 && blockSizes.size()>0);
        callPrepareToPlayOnMessageThreadIfVST3 (instance, sampleRates[0], blockSizes[0]);

        const int numBlocks = 10;
        auto r = ut.getRandom();

        for (auto sr : sampleRates)
        {
            for (auto bs : blockSizes)
            {
                ut.logMessage (juce::String ("Testing with sample rate [SR] and block size [BS]")
                                   .replace ("SR",juce::String (sr, 0), false)
                                   .replace ("BS",juce::String (bs), false));

                if (callReleaseResourcesBeforeSampleRateChange)
                    callReleaseResourcesOnMessageThreadIfVST3 (instance);

                callPrepareToPlayOnMessageThreadIfVST3 (instance, sr, bs);

                const int numChannelsRequired = juce::jmax (instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels());
                juce::AudioBuffer<float> ab (numChannelsRequired, bs);
                juce::MidiBuffer mb;
                mb.ensureSize (32);

                // Add a random note on if the plugin is a synth
                const int noteChannel = r.nextInt ({ 1, 17 });
                const int noteNumber = r.nextInt (128);

                if (isPluginInstrument)
                    addNoteOn (mb, noteChannel, noteNumber, juce::jmin (10, bs));

                for (int i = 0; i < numBlocks; ++i)
                {
                    // Add note off in last block if plugin is a synth
                    if (isPluginInstrument && i == (numBlocks - 1))
                        addNoteOff (mb, noteChannel, noteNumber, 0);

                    fillNoise (ab);

                    {
                      RTC_REALTIME_CONTEXT_IF_ENABLED(ut.getOptions().realtimeCheck, i)
                      instance.processBlock (ab, mb);
                    }

                    mb.clear();

                    ut.expectEquals (countNaNs (ab), 0, "NaNs found in buffer");
                    ut.expectEquals (countInfs (ab), 0, "Infs found in buffer");
                    ut.expectEquals (countSubnormals (ab), 0, "Subnormals found in buffer");
                }
            }
        }
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        runAudioProcessingTest (ut, instance, true);
    }

    std::vector<TestDescription> getDescription (int) const override
    {
        return { { name, "Processes 10 blocks at each sample rate / block size combo. "
                         "For instruments, sends noteOn/noteOff. Checks for NaNs, Infs, subnormals" } };
    }
};

static AudioProcessingTest audioProcessingTest;


//==============================================================================
/**
    Test that process some audio changing the sample rate between runs but doesn't
    call releaseResources between calls to prepare to play.
*/
struct NonReleasingAudioProcessingTest  : public PluginTest
{
    NonReleasingAudioProcessingTest()
        : PluginTest ("Non-releasing audio processing", 6)
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        AudioProcessingTest::runAudioProcessingTest (ut, instance, false);
    }

    std::vector<TestDescription> getDescription (int) const override
    {
        return { { name, "Same as audio processing, but calls prepareToPlay() at new sample rate "
                         "WITHOUT calling releaseResources() first" } };
    }
};

static NonReleasingAudioProcessingTest nonReleasingAudioProcessingTest;


//==============================================================================
struct PluginStateTest  : public PluginTest
{
    PluginStateTest()
    : PluginTest ("Plugin state", 2)
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        auto r = ut.getRandom();

        // Read state
        auto originalState = callGetStateInformationOnMessageThreadIfVST3 (instance);

        // Set random parameter values
        for (auto parameter : getNonBypassAutomatableParameters (instance))
            parameter->setValue (r.nextFloat());

        // Restore original state
        callSetStateInformationOnMessageThreadIfVST3 (instance, originalState);
    }

    std::vector<TestDescription> getDescription (int) const override
    {
        return { { name, "Saves state via getStateInformation(), randomises all automatable params, "
                         "restores via setStateInformation()" } };
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

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        auto r = ut.getRandom();

        // Read state
        auto originalState = callGetStateInformationOnMessageThreadIfVST3 (instance);

        const auto tolaratedDiff = 0.1f;

        // Set random parameter values
        for (auto parameter : getNonBypassAutomatableParameters(instance))
        {
			const auto originalValue = parameter->getValue();
            parameter->setValue(r.nextFloat());

            // Restore original state
            callSetStateInformationOnMessageThreadIfVST3(instance, originalState);

            // Check parameter values return to original
            ut.expectWithinAbsoluteError(parameter->getValue(), originalValue, tolaratedDiff,
                parameter->getName(1024) + juce::String(" not restored on setStateInformation"));
        }

        if (strictnessLevel >= 8)
        {
            // Read state again and compare to what we set
            auto duplicateState = callGetStateInformationOnMessageThreadIfVST3 (instance);
            ut.expect (duplicateState.matches (originalState.getData(), originalState.getSize()),
                       "Returned state differs from that set by host");
        }
    }

    std::vector<TestDescription> getDescription (int level) const override
    {
        if (level >= 8)
            return { { name, "For each param: saves original value, randomises, restores state, "
                             "expects value within 0.1 of original. Also requires exact binary state match" } };

        return { { name, "For each param: saves original value, randomises, restores state, "
                         "expects value within 0.1 of original" } };
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

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        const bool subnormalsAreErrors = ut.getOptions().strictnessLevel > 5;
        const bool isPluginInstrument = instance.getPluginDescription().isInstrument;

        const std::vector<double>& sampleRates = ut.getOptions().sampleRates;
        const std::vector<int>& blockSizes = ut.getOptions().blockSizes;

        jassert (sampleRates.size() > 0 && blockSizes.size() > 0);
        callReleaseResourcesOnMessageThreadIfVST3 (instance);
        callPrepareToPlayOnMessageThreadIfVST3 (instance, sampleRates[0], blockSizes[0]);

        auto r = ut.getRandom();

        for (auto sr : sampleRates)
        {
            for (auto bs : blockSizes)
            {
                const int subBlockSize = 32;
                ut.logMessage (juce::String ("Testing with sample rate [SR] and block size [BS] and sub-block size [SB]")
                                   .replace ("SR",juce::String (sr, 0), false)
                                   .replace ("BS",juce::String (bs), false)
                                   .replace ("SB",juce::String (subBlockSize), false));

                callReleaseResourcesOnMessageThreadIfVST3 (instance);
                callPrepareToPlayOnMessageThreadIfVST3 (instance, sr, bs);

                int numSamplesDone = 0;
                const int numChannelsRequired = juce::jmax (instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels());
                juce::AudioBuffer<float> ab (numChannelsRequired, bs);
                juce::MidiBuffer mb;
                mb.ensureSize (32);

                // Add a random note on if the plugin is a synth
                const int noteChannel = r.nextInt ({ 1, 17 });
                const int noteNumber = r.nextInt (128);

                if (isPluginInstrument)
                    addNoteOn (mb, noteChannel, noteNumber, juce::jmin (10, subBlockSize));

                for (int blockNum = 0;; ++blockNum)
                {
                    // Set random parameter values
                    {
                        auto parameters = getNonBypassAutomatableParameters (instance);

                        for (int i = 0; i < juce::jmin (10, parameters.size()); ++i)
                        {
                            const int paramIndex = r.nextInt (parameters.size());
                            parameters[paramIndex]->setValue (r.nextFloat());
                        }
                    }

                    // Create a sub-buffer and process
                    const int numSamplesThisTime = juce::jmin (subBlockSize, bs - numSamplesDone);

                    // Trigger a note off in the last sub block
                    if (isPluginInstrument && (bs - numSamplesDone) <= subBlockSize)
                        addNoteOff (mb, noteChannel, noteNumber, juce::jmin (10, subBlockSize));

                    juce::AudioBuffer<float> subBuffer (ab.getArrayOfWritePointers(),
                                                  ab.getNumChannels(),
                                                  numSamplesDone,
                                                  numSamplesThisTime);
                    fillNoise (subBuffer);

                    {
                        RTC_REALTIME_CONTEXT_IF_ENABLED(ut.getOptions().realtimeCheck, blockNum)
                        instance.processBlock (subBuffer, mb);
                    }

                    numSamplesDone += numSamplesThisTime;

                    mb.clear();

                    if (numSamplesDone >= bs)
                        break;
                }

                ut.expectEquals (countNaNs (ab), 0, "NaNs found in buffer");
                ut.expectEquals (countInfs (ab), 0, "Infs found in buffer");

                const int subnormals = countSubnormals (ab);

                if (subnormalsAreErrors)
                    ut.expectEquals (countInfs (ab), 0, "Subnormals found in buffer");
                else if (subnormals > 0)
                    ut.logMessage ("!!! WARNING: " + juce::String (countSubnormals (ab)) + " subnormals found in buffer");
            }
        }
    }

    std::vector<TestDescription> getDescription (int level) const override
    {
        if (level > 5)
            return { { name, "Processes in 32-sample sub-blocks, randomly changing up to 10 params between each. "
                             "Subnormals treated as errors" } };

        return { { name, "Processes in 32-sample sub-blocks, randomly changing up to 10 params between each. "
                         "Checks for NaNs, Infs; subnormals logged as warnings" } };
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

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
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
            juce::Thread::sleep (10);
        }
    }

    std::vector<TestDescription> getDescription (int level) const override
    {
        if (level > 5)
            return { { name, "With editor open, loops 1000x calling setValue(random) on ALL parameters "
                             "with 10ms sleep between iterations" } };

        return { { name, "With editor open, loops 100x calling setValue(random) on ALL parameters "
                         "with 10ms sleep between iterations" } };
    }
};

static EditorAutomationTest editorAutomationTest;


//==============================================================================
namespace ParameterHelpers
{
    static void testParameterInfo (PluginTests& ut, juce::AudioProcessorParameter& parameter)
    {
        const int index = parameter.getParameterIndex();
        const juce::String paramName = parameter.getName (512);

        const float defaultValue = parameter.getDefaultValue();
        const juce::String label = parameter.getLabel();
        const int numSteps = parameter.getNumSteps();
        const bool isDiscrete = parameter.isDiscrete();
        const bool isBoolean = parameter.isBoolean();
        const juce::StringArray allValueStrings = parameter.isDiscrete() ? parameter.getAllValueStrings() : juce::StringArray();


        const bool isOrientationInverted = parameter.isOrientationInverted();
        const bool isAutomatable = parameter.isAutomatable();
        const bool isMetaParameter = parameter.isMetaParameter();
        const auto category = parameter.getCategory();

        #define LOGP(x) JUCE_STRINGIFY(x) + " - " + juce::String (x) + ", "
        #define LOGP_B(x) JUCE_STRINGIFY(x) + " - " + juce::String (static_cast<int> (x)) + ", "
        ut.logVerboseMessage (juce::String ("Parameter info: ")
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

    static void testParameterDefaults (PluginTests& ut, juce::AudioProcessorParameter& parameter)
    {
        ut.logVerboseMessage ("Testing accessors");

        const float value = parameter.getValue();
        const juce::String text = parameter.getText (value, 1024);
        const float valueForText = parameter.getValueForText (text);
        const juce::String currentValueAsText = parameter.getCurrentValueAsText();
        ignoreUnused (value, text, valueForText, currentValueAsText);
    }
}

struct AutomatableParametersTest  : public PluginTest
{
    AutomatableParametersTest()
        : PluginTest ("Automatable Parameters", 2)
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        for (auto parameter : getNonBypassAutomatableParameters (instance))
        {
            ut.logVerboseMessage (juce::String ("\nTesting parameter: ") + juce::String (parameter->getParameterIndex()) + " - " + parameter->getName (512));

            ParameterHelpers::testParameterInfo (ut, *parameter);
            ParameterHelpers::testParameterDefaults (ut, *parameter);
        }
    }

    std::vector<TestDescription> getDescription (int) const override
    {
        return { { name, "For each non-bypass automatable parameter: logs index, name, defaultValue, label, "
                         "numSteps, isDiscrete, isBoolean, isAutomatable, category" } };
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

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        for (auto parameter : getNonBypassAutomatableParameters (instance))
        {
            ut.logVerboseMessage (juce::String ("\nTesting parameter: ") + juce::String (parameter->getParameterIndex()) + " - " + parameter->getName (512));

            ParameterHelpers::testParameterInfo (ut, *parameter);
            ParameterHelpers::testParameterDefaults (ut, *parameter);
        }
    }

    std::vector<TestDescription> getDescription (int) const override
    {
        return { { name, "Same as automatable parameters test - logs info for all non-bypass automatable parameters" } };
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

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        auto r = ut.getRandom();
        ScopedEditorShower editor (instance);

        auto parameters = getNonBypassAutomatableParameters (instance);

        // Read state
        auto originalState = callGetStateInformationOnMessageThreadIfVST3 (instance);

        // Set random parameter values
        for (auto parameter : parameters)
            parameter->setValue (r.nextFloat());

        // Restore original state
        callSetStateInformationOnMessageThreadIfVST3 (instance, originalState);

        // Allow for async reaction to state changes
        juce::Thread::sleep (2000);
    }

    std::vector<TestDescription> getDescription (int) const override
    {
        return { { name, "Opens editor on message thread. From background thread: getStateInformation(), "
                         "randomise params, setStateInformation(). Sleeps 2s for async updates" } };
    }
};

static BackgroundThreadStateTest backgroundThreadStateTest;


//==============================================================================
/** Sets plugin parameters from a background thread and the main thread at the
    same time, as if via host automation and UI simultaneously.
*/
struct ParameterThreadSafetyTest    : public PluginTest
{
    ParameterThreadSafetyTest()
        : PluginTest ("Parameter thread safety", 7)
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        juce::WaitableEvent startWaiter, endWaiter;
        auto r = ut.getRandom();
        auto parameters = getNonBypassAutomatableParameters (instance);
        const bool isPluginInstrument = instance.getPluginDescription().isInstrument;
        const int numBlocks = 500;

        // This emulates the plugin itself setting a value for example from a slider within its UI
        juce::MessageManager::callAsync ([&, threadRandom = r]() mutable
                                   {
                                       startWaiter.signal();

                                       for (int i = 0; i < numBlocks; ++i)
                                           for (auto param : parameters)
                                               param->setValueNotifyingHost (threadRandom.nextFloat());

                                       endWaiter.signal();
                                   });

        const int blockSize = 32;
        callReleaseResourcesOnMessageThreadIfVST3 (instance);
        callPrepareToPlayOnMessageThreadIfVST3 (instance, 44100.0, blockSize);

        const int numChannelsRequired = juce::jmax (instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels());
        juce::AudioBuffer<float> ab (numChannelsRequired, blockSize);
        juce::MidiBuffer mb;
        mb.ensureSize (32);

        // Add a random note on if the plugin is a synth
        const int noteChannel = r.nextInt ({ 1, 17 });
        const int noteNumber = r.nextInt (128);

        if (isPluginInstrument)
            addNoteOn (mb, noteChannel, noteNumber, juce::jmin (10, blockSize));

        startWaiter.wait();

        for (int i = 0; i < numBlocks; ++i)
        {
            // Add note off in last block if plugin is a synth
            if (isPluginInstrument && i == (numBlocks - 1))
                addNoteOff (mb, noteChannel, noteNumber, 0);

            for (auto param : parameters)
                param->setValue (r.nextFloat());

            fillNoise (ab);

            {
                RTC_REALTIME_CONTEXT_IF_ENABLED(ut.getOptions().realtimeCheck, i)
                instance.processBlock (ab, mb);
            }

            mb.clear();
        }

        endWaiter.wait();
    }

    std::vector<TestDescription> getDescription (int) const override
    {
        return { { name, "Message thread calls setValueNotifyingHost() 500x on all params. "
                         "Simultaneously, this thread calls setValue() and processBlock() 500x" } };
    }
};

static ParameterThreadSafetyTest parameterThreadSafetyTest;


//==============================================================================
/** Runs auval on the plugin if it's an Audio Unit.
 */
struct AUvalTest    : public PluginTest
{
    AUvalTest()
        : PluginTest ("auval", 5)
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        const auto desc = instance.getPluginDescription();

        if (desc.pluginFormatName != "AudioUnit")
            return;

        // Use -stress on strictness levels greater than 5
        const auto cmd = juce::String ("auval -strict STRESS -v ").replace ("STRESS", ut.getOptions().strictnessLevel > 5 ? "-stress 20" : "")
                            + desc.fileOrIdentifier.fromLastOccurrenceOf ("/", false, false).replace (",", " ");

        juce::ChildProcess cp;
        const auto started = cp.start (cmd);
        ut.expect (started);

        if (! started)
            return;

        juce::MemoryOutputStream outputBuffer;

        for (;;)
        {
            for (;;)
            {
                char buffer[2048];

                if (const auto numBytesRead = cp.readProcessOutput (buffer, sizeof (buffer));
                    numBytesRead > 0)
                {
                    std::string msg (buffer, (size_t) numBytesRead);
                    ut.logVerboseMessage (msg);
                    outputBuffer << juce::String (msg);
                }
                else
                {
                    break;
                }
            }

            if (! cp.isRunning())
                break;

            using namespace std::literals;
            std::this_thread::sleep_for (100ms);
        }

        const auto exitedCleanly = cp.getExitCode() == 0;
        ut.expect (exitedCleanly);
        ut.logMessage ("auval exited with code: " + juce::String (cp.getExitCode()));

        if (! exitedCleanly && ! ut.getOptions().verbose)
            ut.logMessage (outputBuffer.toString());
    }

    std::vector<TestDescription> getDescription (int level) const override
    {
        if (level > 5)
            return { { name, "Runs 'auval -strict -stress 20 -v <type> <subtype> <manu>' (Audio Units only)" } };

        return { { name, "Runs 'auval -strict -v <type> <subtype> <manu>' (Audio Units only)" } };
    }
};

static AUvalTest auvalTest;

//==============================================================================
/** Runs Steinberg's validator on the plugin if it's a VST3.
 */
struct VST3validator    : public PluginTest
{
    VST3validator()
        : PluginTest ("vst3 validator", 5)
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        const auto desc = instance.getPluginDescription();

        if (desc.pluginFormatName != "VST3")
            return;

        auto vst3Validator = ut.getOptions().vst3Validator;

        if (vst3Validator == juce::File())
        {
            ut.logMessage ("INFO: Skipping vst3 validator as validator path hasn't been set");
            return;
        }

        juce::StringArray cmd (vst3Validator.getFullPathName());

        if (ut.getOptions().strictnessLevel > 5)
            cmd.add ("-e");

        cmd.add (ut.getFileOrID());

        juce::ChildProcess cp;
        const auto started = cp.start (cmd);
        ut.expect (started, "VST3 validator app has been set but is unable to start");

        if (! started)
            return;

        juce::MemoryOutputStream outputBuffer;

        for (;;)
        {
            for (;;)
            {
                char buffer[2048];

                if (const auto numBytesRead = cp.readProcessOutput (buffer, sizeof (buffer));
                    numBytesRead > 0)
                {
                    std::string msg (buffer, (size_t) numBytesRead);
                    ut.logVerboseMessage (msg);
                    outputBuffer << juce::String (msg);
                }
                else
                {
                    break;
                }
            }

            if (! cp.isRunning())
                break;

            using namespace std::literals;
            std::this_thread::sleep_for (100ms);
        }

        const auto exitedCleanly = cp.getExitCode() == 0;
        ut.expect (exitedCleanly);

        ut.logMessage ("vst3 validator exited with code: " + juce::String (cp.getExitCode()));

        if (! exitedCleanly && ! ut.getOptions().verbose)
            ut.logMessage (outputBuffer.toString());
    }

    std::vector<TestDescription> getDescription (int level) const override
    {
        if (level > 5)
            return { { name, "Runs Steinberg's vstvalidator with -e flag for extended validation (VST3 only)" } };

        return { { name, "Runs Steinberg's vstvalidator on the plugin file (VST3 only)" } };
    }
};

static VST3validator vst3validator;
