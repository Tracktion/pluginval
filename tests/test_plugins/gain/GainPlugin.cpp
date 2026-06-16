/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "GainPlugin.h"

//==============================================================================
GainProcessor::GainProcessor()
    : juce::AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                             .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Default gain of 1.0 passes the input through unchanged.
    addParameter (gain = new juce::AudioParameterFloat ("gain", "Gain",
                                                        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));
}

//==============================================================================
void GainProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const float g = gain->get();

    for (int c = 0; c < buffer.getNumChannels(); ++c)
        buffer.applyGain (c, 0, buffer.getNumSamples(), g);
}

//==============================================================================
void GainProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // getValue() is a private override on the concrete type, so go via the base.
    juce::MemoryOutputStream mos (destData, false);
    mos.writeFloat (static_cast<juce::AudioProcessorParameter*> (gain)->getValue());
}

void GainProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mis (data, (size_t) sizeInBytes, false);

    if (mis.getNumBytesRemaining() >= (int) sizeof (float))
        gain->setValueNotifyingHost (mis.readFloat());
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GainProcessor();
}
