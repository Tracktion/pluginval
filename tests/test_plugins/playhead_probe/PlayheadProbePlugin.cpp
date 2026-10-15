/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "PlayheadProbePlugin.h"

//==============================================================================
void PlayheadProbeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    buffer.clear();

    auto* playHead = getPlayHead();

    if (playHead == nullptr)
        return;

    const auto position = playHead->getPosition();

    if (! position.hasValue())
        return;

    double ppq = 0.0, bpm = 0.0;

    if (const auto v = position->getPpqPosition())
        ppq = *v;

    if (const auto v = position->getBpm())
        bpm = *v;

    const int numSamples = buffer.getNumSamples();

    // Channel 0: the block's ppqPosition (a per-block staircase that advances with
    // the transport). Channel 1: the tempo, scaled into a sane range.
    if (buffer.getNumChannels() > 0)
        juce::FloatVectorOperations::fill (buffer.getWritePointer (0), (float) ppq, numSamples);

    if (buffer.getNumChannels() > 1)
        juce::FloatVectorOperations::fill (buffer.getWritePointer (1), (float) (bpm / 1000.0), numSamples);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PlayheadProbeProcessor();
}
