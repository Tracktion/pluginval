/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "ToneGeneratorPlugin.h"

namespace
{
    constexpr double twoPi = 2.0 * 3.14159265358979323846;
}

//==============================================================================
ToneGeneratorProcessor::ToneGeneratorProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Parameter indices are stable (0, 1, 2) so acceptance configs can address
    // them by index as well as by name.
    addParameter (waveform  = new juce::AudioParameterChoice ("waveform", "Waveform",
                                                              juce::StringArray { "sine", "square" }, 0));
    addParameter (frequency = new juce::AudioParameterFloat  ("frequency", "Frequency",
                                                              juce::NormalisableRange<float> (20.0f, 20000.0f), 440.0f));
    addParameter (gain      = new juce::AudioParameterFloat  ("gain", "Gain",
                                                              juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));
}

//==============================================================================
void ToneGeneratorProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    sampleIndex = 0;   // phase resets so renders are reproducible
}

void ToneGeneratorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    buffer.clear();

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    const bool isSquare = waveform->getIndex() == (int) Waveform::square;
    const double freq = (double) frequency->get();
    const float g = gain->get();

    if (numChannels > 0)
    {
        auto* channel0 = buffer.getWritePointer (0);

        for (int s = 0; s < numSamples; ++s)
        {
            const double phase = twoPi * freq * (double) (sampleIndex + s) / currentSampleRate;

            float value;
            if (isSquare)
            {
                // Closed-form square: +1 for the first half of each cycle, -1 for the second.
                const double fractional = phase / twoPi - std::floor (phase / twoPi);
                value = fractional < 0.5 ? 1.0f : -1.0f;
            }
            else
            {
                value = (float) std::sin (phase);
            }

            channel0[s] = value * g;
        }

        // The tone is mono; copy it to every other output channel.
        for (int c = 1; c < numChannels; ++c)
            buffer.copyFrom (c, 0, buffer, 0, 0, numSamples);
    }

    sampleIndex += numSamples;
}

//==============================================================================
void ToneGeneratorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // A tiny, explicit binary state: the three parameters' normalised values.
    // This is the blob that a state.file acceptance test restores. getValue() is
    // a private override on the concrete parameter types, so go via the base.
    const auto normalised = [] (juce::AudioProcessorParameter* p) { return p->getValue(); };

    juce::MemoryOutputStream mos (destData, false);
    mos.writeFloat (normalised (waveform));
    mos.writeFloat (normalised (frequency));
    mos.writeFloat (normalised (gain));
}

void ToneGeneratorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mis (data, (size_t) sizeInBytes, false);

    if (mis.getNumBytesRemaining() >= (int) (3 * sizeof (float)))
    {
        waveform->setValueNotifyingHost  (mis.readFloat());
        frequency->setValueNotifyingHost (mis.readFloat());
        gain->setValueNotifyingHost      (mis.readFloat());
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ToneGeneratorProcessor();
}
