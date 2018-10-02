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

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
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

                    {
                        ScopedAllocationDisabler sad;
                        instance.processBlock (ab, mb);
                    }

                    auto& ai = getAllocatorInterceptor();
                    ut.expect (! ai.getAndClearAllocationViolation(), "Allocations occured in audio thread: " + String (ai.getAndClearNumAllocationViolations()));

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

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        const double sampleRates[] = { 44100.0, 48000.0, 96000.0 };
        const int blockSizes[] = { 64, 128, 256, 512, 1024 };

        for (auto sr : sampleRates)
        {
            for (auto preparedBlockSize : blockSizes)
            {
                const auto processingBlockSize = preparedBlockSize * 2;
                ut.logMessage (String ("Preparing with sample rate [SR] and block size [BS], processing with block size [BSP]")
                               .replace ("SR", String (sr, 0), false)
                               .replace ("BSP", String (processingBlockSize), false)
                               .replace ("BS", String (preparedBlockSize), false));
                instance.releaseResources();
                instance.prepareToPlay (sr, preparedBlockSize);

                const int numChannelsRequired = jmax (instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels());
                AudioBuffer<float> ab (numChannelsRequired, processingBlockSize);
                MidiBuffer mb;

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
};

static LargerThanPreparedBlockSizeTest largerThanPreparedBlockSizeTest;

