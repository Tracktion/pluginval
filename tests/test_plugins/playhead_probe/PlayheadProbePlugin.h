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

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/**
    A minimal, deterministic plugin that writes the host transport into its
    output so the acceptance feature's fixed-playhead support can be verified.

    Each block it reads getPlayHead()->getPosition() and writes the block's
    ppqPosition into channel 0 and the tempo (scaled) into channel 1. With no
    playhead (or no position) it outputs silence - so a recorded reference is a
    direct check that the transport actually reached the plugin.
*/
class PlayheadProbeProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    PlayheadProbeProcessor()
        : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    ~PlayheadProbeProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override   { return nullptr; }
    bool hasEditor() const override                       { return false; }

    //==============================================================================
    const juce::String getName() const override           { return "pluginval Playhead Probe"; }
    bool acceptsMidi() const override                     { return false; }
    bool producesMidi() const override                    { return false; }
    bool isMidiEffect() const override                    { return false; }
    double getTailLengthSeconds() const override          { return 0.0; }

    //==============================================================================
    int getNumPrograms() override                         { return 1; }
    int getCurrentProgram() override                      { return 0; }
    void setCurrentProgram (int) override                 {}
    const juce::String getProgramName (int) override      { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override   {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayheadProbeProcessor)
};
