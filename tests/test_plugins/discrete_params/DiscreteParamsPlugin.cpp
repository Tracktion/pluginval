/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "DiscreteParamsPlugin.h"

//==============================================================================
DiscreteParamsProcessor::DiscreteParamsProcessor()
    : juce::AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                             .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    for (int i = 1; i <= 3; ++i)
    {
        bools.push_back (new juce::AudioParameterBool (juce::ParameterID ("bool" + juce::String (i), 1),
                                                       "Bool " + juce::String (i), false));
        addParameter (bools.back());
    }

    for (int i = 1; i <= 2; ++i)
    {
        choices.push_back (new juce::AudioParameterChoice (juce::ParameterID ("choice" + juce::String (i), 1),
                                                           "Choice " + juce::String (i),
                                                           { "One", "Two", "Three", "Four" }, 0));
        addParameter (choices.back());
    }
}

//==============================================================================
void DiscreteParamsProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos (destData, false);

    for (auto p : bools)
        mos.writeBool (p->get());

    for (auto p : choices)
        mos.writeInt (p->getIndex());
}

void DiscreteParamsProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mis (data, (size_t) sizeInBytes, false);

    for (auto p : bools)
        *p = mis.readBool();

    for (auto p : choices)
        *p = mis.readInt();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DiscreteParamsProcessor();
}
