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
#include <thread>

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
struct PluginPrgramsTest    : public PluginTest
{
    PluginPrgramsTest()
        : PluginTest ("Plugin programs", 2)
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        const int numPrograms = instance.getNumPrograms();
        ut.logMessage ("Num programs: " + String (numPrograms));

        for (int i = 0; i < numPrograms; ++i)
            ut.logVerboseMessage (String ("Program 123 name: XYZ")
                                  .replace ("123", String (i))
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
                ut.logVerboseMessage ("Changing program to: " + String (programNum));
                instance.setCurrentProgram (programNum);
            }

            if (currentProgram >= 0)
            {
                ut.logVerboseMessage ("Resetting program to: " + String (currentProgram));
                instance.setCurrentProgram (currentProgram);
            }
            else
            {
                ut.logMessage ("!!! WARNING: Current program is -1... Is this correct?");
            }
        }
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
            callReleaseResourcesOnMessageThreadIfVST3 (instance);

            const std::vector<double>& sampleRates = ut.getOptions().sampleRates;
            const std::vector<int>& blockSizes = ut.getOptions().blockSizes;

            jassert (sampleRates.size() > 0 && blockSizes.size() > 0);
            callPrepareToPlayOnMessageThreadIfVST3 (instance, sampleRates[0], blockSizes[0]);

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

    static void runAudioProcessingTest (PluginTests& ut, AudioPluginInstance& instance,
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
                ut.logMessage (String ("Testing with sample rate [SR] and block size [BS]")
                                   .replace ("SR", String (sr, 0), false)
                                   .replace ("BS", String (bs), false));

                if (callReleaseResourcesBeforeSampleRateChange)
                    callReleaseResourcesOnMessageThreadIfVST3 (instance);

                callPrepareToPlayOnMessageThreadIfVST3 (instance, sr, bs);

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

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        runAudioProcessingTest (ut, instance, true);
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
        : PluginTest ("Non-releasing audio processing", 4)
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        AudioProcessingTest::runAudioProcessingTest (ut, instance, false);
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

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
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
        auto originalState = callGetStateInformationOnMessageThreadIfVST3 (instance);

        // Check current sum of parameter values
        const float originalParamsSum = getParametersSum (instance);

        // Set random parameter values
        for (auto parameter : getNonBypassAutomatableParameters (instance))
            parameter->setValue (r.nextFloat());

        // Restore original state
        callSetStateInformationOnMessageThreadIfVST3 (instance, originalState);

        // Check parameter values return to original
        ut.expectWithinAbsoluteError (getParametersSum (instance), originalParamsSum, 0.1f,
                                      "Parameters not restored on setStateInformation");

        if (strictnessLevel >= 8)
        {
            // Read state again and compare to what we set
            auto duplicateState = callGetStateInformationOnMessageThreadIfVST3 (instance);
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

        const std::vector<double>& sampleRates = ut.getOptions().sampleRates;
        const std::vector<int>& blockSizes = ut.getOptions().blockSizes;

        jassert (sampleRates.size() > 0 && blockSizes.size() > 0);
        callPrepareToPlayOnMessageThreadIfVST3 (instance, sampleRates[0], blockSizes[0]);

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

                callReleaseResourcesOnMessageThreadIfVST3 (instance);
                callPrepareToPlayOnMessageThreadIfVST3 (instance, sr, bs);

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

        // Read state
        auto originalState = callGetStateInformationOnMessageThreadIfVST3 (instance);

        // Set random parameter values
        for (auto parameter : parameters)
            parameter->setValue (r.nextFloat());

        // Restore original state
        callSetStateInformationOnMessageThreadIfVST3 (instance, originalState);

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
        callReleaseResourcesOnMessageThreadIfVST3 (instance);
        callPrepareToPlayOnMessageThreadIfVST3 (instance, 44100.0, blockSize);

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


//==============================================================================
/** Runs auval on the plugin if it's an Audio Unit.
 */
struct AUvalTest    : public PluginTest
{
    AUvalTest()
        : PluginTest ("auval", 5)
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        const auto desc = instance.getPluginDescription();

        if (desc.pluginFormatName != "AudioUnit")
            return;

        // Use -stress on strictness levels greater than 5
        const auto cmd = juce::String ("auval -strict STRESS -v ").replace ("STRESS", ut.getOptions().strictnessLevel > 5 ? "-stress" : "")
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

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        const auto desc = instance.getPluginDescription();

        if (desc.pluginFormatName != "VST3")
            return;

        auto vst3Validator = ut.getOptions().vst3Validator;

        if (vst3Validator == File())
        {
            ut.logMessage ("INFO: Skipping vst3 validator as validator path hasn't been set");
            return;
        }

        juce::StringArray cmd (vst3Validator.getFullPathName());

        if (ut.getOptions().strictnessLevel > 5)
            cmd.add ("-e");

        cmd.add (desc.fileOrIdentifier);

        juce::ChildProcess cp;
        const auto started = cp.start (cmd);
        ut.expect (started, "VST3 validator app has been set but is unable to start");

        if (! started)
            return;

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

        ut.expect (cp.getExitCode() == 0);
        ut.logMessage ("vst3 validator exited with code: " + juce::String (cp.getExitCode()));
    }
};

static VST3validator vst3validator;
