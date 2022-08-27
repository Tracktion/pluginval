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
struct AllocationsInRealTimeThreadTest  : public PluginTest
{
    AllocationsInRealTimeThreadTest()
        : PluginTest ("Allocations during process", 9)
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        const bool isPluginInstrument = instance.getPluginDescription().isInstrument;

        const std::vector<double>& sampleRates = ut.getOptions().sampleRates;
        const std::vector<int>& blockSizes = ut.getOptions().blockSizes;

        jassert (sampleRates.size() > 0 && blockSizes.size() > 0);
        instance.prepareToPlay (sampleRates[0], blockSizes[0]);

        const int numBlocks = 10;
        auto r = ut.getRandom();

        for (auto sr : sampleRates)
        {
            for (auto bs : blockSizes)
            {
                ut.logMessage (juce::String ("Testing with sample rate [SR] and block size [BS]")
                                   .replace ("SR",juce::String (sr, 0), false)
                                   .replace ("BS",juce::String (bs), false));
                instance.releaseResources();
                instance.prepareToPlay (sr, bs);

                const int numChannelsRequired = juce::jmax (instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels());
                juce::AudioBuffer<float> ab (numChannelsRequired, bs);
                juce::MidiBuffer mb;

                // Add a random note on if the plugin is a synth
                const int noteChannel = r.nextInt ({ 1, 17 });
                const int noteNumber = r.nextInt (128);

                if (isPluginInstrument)
                    addNoteOn (mb, noteChannel, noteNumber, juce::jmin (10, bs - 1));

                for (int i = 0; i < numBlocks; ++i)
                {
                    // Add note off in last block if plugin is a synth
                    if (isPluginInstrument && i == (numBlocks - 1))
                        addNoteOff (mb, noteChannel, noteNumber, 0);

                    fillNoise (ab);

                    {
                        ScopedAllocationDisabler sad;
                        instance.processBlock (ab, mb);
                    }

                    mb.clear();

                    auto& ai = getAllocatorInterceptor();
                    ut.expect (! ai.getAndClearAllocationViolation(), "Allocations occurred in audio thread: " + juce::String (ai.getAndClearNumAllocationViolations()));

                    ut.expectEquals (countNaNs (ab), 0, "NaNs found in buffer");
                    ut.expectEquals (countInfs (ab), 0, "Infs found in buffer");
                    ut.expectEquals (countSubnormals (ab), 0, "Subnormals found in buffer");
                }
            }
        }
    }
};

static AllocationsInRealTimeThreadTest allocationsInRealTimeThreadTest;

//==============================================================================
struct LargerThanPreparedBlockSizeTest   : public PluginTest
{
    LargerThanPreparedBlockSizeTest()
        : PluginTest ("Process called with a larger than prepared block size", 8)
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        if (! canRunTest (instance))
        {
            // Technically, it is undefined to pass a larger block than was prepared to the major
            // plugin formats (VST, VST3 AU) so we'll skip this test.
            // However, juce::AudioProcessor specifically states that this could happen in the processBlock
            // comment so be aware that if you write an AudioProcessor and attach it directly to an audio
            // device callback, you may get a larger block size.
            // Ideally, juce::AudioProcessor wouldn't support this use case and would split up larger blocks
            // than are prepared internally. Until this happens though, we should be testing it.
            ut.logMessage ("INFO: Skipping test for plugin format");
            return;
        }

        const std::vector<double>& sampleRates = ut.getOptions().sampleRates;
        const std::vector<int>& blockSizes = ut.getOptions().blockSizes;

        jassert (sampleRates.size() > 0 && blockSizes.size() > 0);

        for (auto sr : sampleRates)
        {
            for (auto preparedBlockSize : blockSizes)
            {
                const auto processingBlockSize = preparedBlockSize * 2;
                ut.logMessage (juce::String ("Preparing with sample rate [SR] and block size [BS], processing with block size [BSP]")
                                   .replace ("SR",juce::String (sr, 0), false)
                                   .replace ("BSP",juce::String (processingBlockSize), false)
                                   .replace ("BS",juce::String (preparedBlockSize), false));
                instance.releaseResources();
                instance.prepareToPlay (sr, preparedBlockSize);

                const int numChannelsRequired = juce::jmax (instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels());
                juce::AudioBuffer<float> ab (numChannelsRequired, processingBlockSize);
                juce::MidiBuffer mb;

                for (int i = 0; i < 10; ++i)
                {
                    mb.clear();
                    fillNoise (ab);
                    instance.processBlock (ab, mb);

                    ut.expectEquals (countNaNs (ab), 0, "NaNs found in buffer");
                    ut.expectEquals (countInfs (ab), 0, "Infs found in buffer");
                    ut.expectEquals (countSubnormals (ab), 0, "Subnormals found in buffer");
                }
            }
        }
    }

private:
    static bool canRunTest (const juce::AudioPluginInstance& instance)
    {
        const auto format = instance.getPluginDescription().pluginFormatName;

        return ! (format.contains ("AudioUnit")
                  || format.contains ("VST")
                  || format.contains ("VST3"));

    }
};

static LargerThanPreparedBlockSizeTest largerThanPreparedBlockSizeTest;

