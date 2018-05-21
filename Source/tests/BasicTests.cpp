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

//==============================================================================
struct PluginInfoTest   : public PluginTest
{
    PluginInfoTest()
        : PluginTest ("Plugin info", 1)
    {
    }

    void runTest (UnitTest& ut, AudioPluginInstance& instance) override
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
        : PluginTest ("Editor", 2)
    {
    }

    bool needsToRunOnMessageThread() override
    {
        return true;
    }

    void runTest (UnitTest& ut, AudioPluginInstance& instance) override
    {
        if (instance.hasEditor())
        {
            StopwatchTimer timer;

            {
                std::unique_ptr<AudioProcessorEditor> editor (instance.createEditor());
                ut.expect (editor != nullptr, "Unable to create editor");

                if (editor)
                {
                    editor->addToDesktop (0);
                    editor->setVisible (true);
                }

                ut.logMessage ("\nTime taken to open editor (cold): " + timer.getDescription());
            }

            {
                timer.reset();
                std::unique_ptr<AudioProcessorEditor> editor (instance.createEditor());
                ut.expect (editor != nullptr, "Unable to create editor on second attempt");

                if (editor)
                {
                    editor->addToDesktop (0);
                    editor->setVisible (true);
                }

                ut.logMessage ("Time taken to open editor (warm): " + timer.getDescription());
            }
        }
    }
};

static EditorTest editorTest;


//==============================================================================
struct AudioProcessingTest  : public PluginTest
{
    AudioProcessingTest()
        : PluginTest ("Audio processing", 3)
    {
    }

    void runTest (UnitTest& ut, AudioPluginInstance& instance) override
    {
        const double sampleRates[] = { 44100.0, 48000.0, 96000.0 };
        const int blockSizes[] = { 64, 128, 256, 512, 1024 };

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

                for (int i = 0; i < 10; ++i)
                {
                    mb.clear();
                    fillNoise (ab);
                    instance.processBlock (ab, mb);
                }

                ut.expectEquals (countNaNs (ab), 0, "NaNs found in buffer");
                ut.expectEquals (countInfs (ab), 0, "Infs found in buffer");
                ut.expectEquals (countSubnormals (ab), 0, "Subnormals found in buffer");
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

    void runTest (UnitTest& ut, AudioPluginInstance& instance) override
    {
        auto& parameters = instance.getParameters();
        MemoryBlock originalState;

        // Read state
        instance.getStateInformation (originalState);

        // Set random parameter values
        for (auto parameter : parameters)
            parameter->setValue (ut.getRandom().nextFloat());

        // Restore original state
        instance.setStateInformation (originalState.getData(), (int) originalState.getSize());
    }
};

static PluginStateTest pluginStateTest;


//==============================================================================
struct AutomationTest  : public PluginTest
{
    AutomationTest()
        : PluginTest ("Automation", 3)
    {
    }

    void runTest (UnitTest& ut, AudioPluginInstance& instance) override
    {
        const double sampleRates[] = { 44100.0, 48000.0, 96000.0 };
        const int blockSizes[] = { 64, 128, 256, 512, 1024 };

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

                for (;;)
                {
                    // Set random parameter values
                    {
                        auto& parameters = instance.getParameters();

                        for (int i = 0; i < jmin (10, parameters.size()); ++i)
                        {
                            const int paramIndex = ut.getRandom().nextInt (parameters.size());
                            parameters[paramIndex]->setValue (ut.getRandom().nextFloat());
                        }
                    }

                    // Create a sub-buffer and process
                    const int numSamplesThisTime = jmin (subBlockSize, bs - numSamplesDone);
                    mb.clear();

                    AudioBuffer<float> subBuffer (ab.getArrayOfWritePointers(),
                                                  ab.getNumChannels(),
                                                  numSamplesDone,
                                                  numSamplesThisTime);
                    fillNoise (subBuffer);
                    instance.processBlock (subBuffer, mb);
                    numSamplesDone += numSamplesThisTime;

                    if (numSamplesDone >= bs)
                        break;
                }

                ut.expectEquals (countNaNs (ab), 0, "NaNs found in buffer");
                ut.expectEquals (countInfs (ab), 0, "Infs found in buffer");
                ut.expectEquals (countSubnormals (ab), 0, "Subnormals found in buffer");
            }
        }
    }
};

static AutomationTest automationTest;


//==============================================================================
namespace ParameterHelpers
{
    static void testParameterInfo (UnitTest& ut, AudioProcessorParameter& parameter)
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
        ut.logMessage (String ("Parameter info: ")
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

    static void testParameterDefaults (UnitTest& ut, AudioProcessorParameter& parameter)
    {
        ut.logMessage ("Testing accessers");
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

    void runTest (UnitTest& ut, AudioPluginInstance& instance) override
    {
        for (auto parameter : instance.getParameters())
        {
            if (! parameter->isAutomatable())
                continue;

            ut.logMessage (String ("\nTesting parameter: ") + String (parameter->getParameterIndex()) + " - " + parameter->getName (512));
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

    void runTest (UnitTest& ut, AudioPluginInstance& instance) override
    {
        for (auto parameter : instance.getParameters())
        {
            ut.logMessage (String ("\nTesting parameter: ") + String (parameter->getParameterIndex()) + " - " + parameter->getName (512));
            ParameterHelpers::testParameterInfo (ut, *parameter);
            ParameterHelpers::testParameterDefaults (ut, *parameter);
        }
    }
};

//==============================================================================
struct BackgroundThreadStateTest    : public PluginTest
{
    BackgroundThreadStateTest()
        : PluginTest ("Background thread state", 7)
    {
    }

    void runTest (UnitTest& ut, AudioPluginInstance& instance) override
    {
        WaitableEvent waiter;
        std::unique_ptr<AudioProcessorEditor> editor;
        MessageManager::getInstance()->callAsync ([&]
                                                  {
                                                      editor.reset (instance.createEditor());
                                                      ut.expect (editor != nullptr, "Unable to create editor");

                                                      if (editor)
                                                      {
                                                          editor->addToDesktop (0);
                                                          editor->setVisible (true);
                                                      }

                                                      waiter.signal();
                                                  });

        auto& parameters = instance.getParameters();
        MemoryBlock originalState;

        // Read state
        instance.getStateInformation (originalState);

        // Set random parameter values
        for (auto parameter : parameters)
            parameter->setValue (ut.getRandom().nextFloat());

        // Restore original state
        instance.setStateInformation (originalState.getData(), (int) originalState.getSize());

        waiter.wait();
        Thread::sleep (2000);

        MessageManager::getInstance()->callAsync ([&]
                                                  {
                                                      editor.reset();
                                                      waiter.signal();
                                                  });
        waiter.wait();
    }
};

static BackgroundThreadStateTest backgroundThreadStateTest;
