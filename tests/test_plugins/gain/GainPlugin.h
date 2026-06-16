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
    A minimal, fully deterministic gain effect used to dogfood the acceptance
    feature's audio-input path.

    Unlike the tone generator (a pure generator), this is an effect: it
    multiplies its input by a single linear gain parameter. Feeding it a known
    input file and comparing the gained output exercises the input.audio render
    path. Multiplying by an exact-in-float gain (e.g. 0.5) keeps it bit-exact.
*/
class GainProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    GainProcessor();
    ~GainProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override   { return nullptr; }
    bool hasEditor() const override                       { return false; }

    //==============================================================================
    const juce::String getName() const override           { return "pluginval Gain"; }
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
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    //==============================================================================
    juce::AudioParameterFloat* gain = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainProcessor)
};
