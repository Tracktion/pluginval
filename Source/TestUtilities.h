/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#pragma once

//==============================================================================
struct StopwatchTimer
{
    StopwatchTimer()
    {
        reset();
    }

    void reset()
    {
        startTime = Time::getMillisecondCounter();
    }

    String getDescription() const
    {
        const auto relTime = RelativeTime::milliseconds (static_cast<int> (Time::getMillisecondCounter() - startTime));
        return relTime.getDescription();
    }

private:
    uint32 startTime;
};

//==============================================================================
template<typename UnaryFunction>
void iterateAudioBuffer (AudioBuffer<float>& ab, UnaryFunction fn)
{
    float** sampleData = ab.getArrayOfWritePointers();

    for (int c = ab.getNumChannels(); --c >= 0;)
        for (int s = ab.getNumSamples(); --s >= 0;)
            fn (sampleData[c][s]);
}

static inline void fillNoise (AudioBuffer<float>& ab) noexcept
{
    Random r;
    ScopedNoDenormals noDenormals;

    float** sampleData = ab.getArrayOfWritePointers();

    for (int c = ab.getNumChannels(); --c >= 0;)
        for (int s = ab.getNumSamples(); --s >= 0;)
            sampleData[c][s] = r.nextFloat() * 2.0f - 1.0f;
}

static inline int countNaNs (AudioBuffer<float>& ab) noexcept
{
    int count = 0;
    iterateAudioBuffer (ab, [&count] (float s)
                            {
                                if (std::isnan (s))
                                    ++count;
                            });

    return count;
}

static inline int countInfs (AudioBuffer<float>& ab) noexcept
{
    int count = 0;
    iterateAudioBuffer (ab, [&count] (float s)
    {
        if (std::isinf (s))
            ++count;
    });

    return count;
}

static inline int countSubnormals (AudioBuffer<float>& ab) noexcept
{
    int count = 0;
    iterateAudioBuffer (ab, [&count] (float s)
    {
        if (s != 0.0f && std::fpclassify (s) == FP_SUBNORMAL)
            ++count;
    });

    return count;
}

static inline void addNoteOn (MidiBuffer& mb, int channel, int noteNumber, int sample) noexcept
{
    mb.addEvent (juce::MidiMessage::noteOn (channel, noteNumber, 0.5f), sample);
}

static inline void addNoteOff (MidiBuffer& mb, int channel, int noteNumber, int sample) noexcept
{
    mb.addEvent (juce::MidiMessage::noteOff (channel, noteNumber, 0.5f), sample);
}

//==============================================================================
struct ScopedPluginDeinitialiser
{
    ScopedPluginDeinitialiser (AudioProcessor& ap)
        : processor (ap), sampleRate (ap.getSampleRate()), blockSize (ap.getBlockSize())
    {
        processor.releaseResources();
    }

    ~ScopedPluginDeinitialiser()
    {
        if (blockSize != 0 && sampleRate != 0.0)
            processor.prepareToPlay (sampleRate, blockSize);
    }

    AudioProcessor& processor;
    const double sampleRate;
    const int blockSize;
};

//==============================================================================
struct ScopedBusesLayout
{
    ScopedBusesLayout (AudioProcessor& ap)
        : processor (ap), currentLayout (ap.getBusesLayout())
    {
    }

    ~ScopedBusesLayout()
    {
        processor.setBusesLayout (currentLayout);
    }

    AudioProcessor& processor;
    AudioProcessor::BusesLayout currentLayout;
};
