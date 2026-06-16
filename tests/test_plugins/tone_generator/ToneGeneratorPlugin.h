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
    A minimal, fully deterministic tone-generator plugin used to dogfood the
    acceptance-testing feature.

    The output is computed in closed form from a sample index that resets to 0
    on prepareToPlay(), so the same configuration always renders the identical
    buffer (ideal for the bit-exact / sample comparator). There is no randomness
    and no denormal-sensitive feedback path. Audio input and MIDI are ignored -
    it is a pure generator.
*/
class ToneGeneratorProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    enum class Waveform { sine = 0, square = 1 };

    ToneGeneratorProcessor();
    ~ToneGeneratorProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override   { return nullptr; }
    bool hasEditor() const override                       { return false; }

    //==============================================================================
    const juce::String getName() const override           { return "pluginval Tone Generator"; }
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
    juce::AudioParameterChoice* waveform = nullptr;
    juce::AudioParameterFloat*  frequency = nullptr;
    juce::AudioParameterFloat*  gain = nullptr;

    double currentSampleRate = 44100.0;
    juce::int64 sampleIndex = 0;   // resets to 0 on prepareToPlay for determinism

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneGeneratorProcessor)
};
