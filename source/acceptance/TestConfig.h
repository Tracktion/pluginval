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
#include <nlohmann/json.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace acceptance
{

//==============================================================================
/**
    The definition of a single acceptance test.

    This is a plain, std-typed struct deserialised from the positional
    <config.json> of the "pluginval test" command. JSON keys are snake_case
    (e.g. sample_rate); the C++ members stay JUCE-style camelCase, so the mapping
    is done explicitly in the per-struct to_json / from_json (see TestConfig.cpp)
    rather than the bare NLOHMANN macro.

    This config is completely independent of PluginvalSettings / the --config
    settings layering used by the validate command.
*/
struct TestConfig
{
    /** A fixed transport supplied to the plugin during the render, for
        time-dependent plugins (tempo-synced LFOs, arpeggiators, ...). The tempo
        and time signature are constant; the position advances with the render. */
    struct PlayheadConfig
    {
        double bpm = 120.0;
        int    timeSigNumerator = 4;
        int    timeSigDenominator = 4;
        double startPpq = 0.0;       /**< Transport position (in quarter notes) at sample 0. */
    };

    std::string name;                              /**< Labels results; derives the default reference path. */
    std::string plugin;                            /**< Plugin path or AU id. Required. */
    std::string inputAudio;                        /**< input.audio: path to an input audio file, or empty for silence. */
    std::string inputMidi;                         /**< input.midi: path to a .mid file, or empty for none. */
    std::string reference;                         /**< The golden file. Empty -> derived from name. */
    std::string stateFile;                         /**< state.file: binary getStateInformation blob. */
    std::map<std::string, double> stateParameters; /**< state.parameters: name-or-index -> normalised value. */
    double sampleRate = 44100.0;
    int blockSize = 512;
    std::optional<double> renderDuration;          /**< Seconds. Unset -> derive from the input length. */
    nlohmann::json comparison;                     /**< Map of comparator name -> sub-config. Empty -> default. */
    std::optional<PlayheadConfig> playhead;        /**< Fixed transport. Unset -> no playhead supplied to the plugin. */

    //==============================================================================
    /** The directory the config was loaded from. Relative reference / input /
        state paths resolve against this; the plugin path resolves against the
        working directory (per the spec). Not part of the JSON. */
    juce::File configDir;

    //==============================================================================
    /** Returns the resolved comparison map, substituting the default
        ({ "sample": 1/32768 }) when none was supplied. */
    nlohmann::json getComparison() const;

    /** The plugin path or AU id, resolved against the working directory. */
    juce::String getPluginPathOrID() const;

    /** The golden reference file (explicit, or <configDir>/<name>.wav). */
    juce::File getReferenceFile() const;

    /** The reference's JSON sidecar (<reference>.json). */
    juce::File getReferenceSidecarFile() const;

    /** The diff WAV written next to the reference on a comparison failure. */
    juce::File getDiffFile() const;

    /** Resolved input audio file, or an empty File if none. */
    juce::File getInputAudioFile() const;

    /** Resolved input MIDI file, or an empty File if none. */
    juce::File getInputMidiFile() const;

    /** Resolved state file, or an empty File if none. */
    juce::File getStateFile() const;

    /** The effective test name (explicit, or the reference's basename). */
    juce::String getName() const;

    //==============================================================================
    /** Loads one or more configs from a JSON file. The top level may be a single
        object or an array of objects (phase 2 multiplexing); v1 callers use the
        first entry. Throws std::runtime_error on a load / parse error. */
    static std::vector<TestConfig> loadFromFile (const juce::File&);

private:
    juce::File resolveAgainstConfigDir (const std::string& path) const;
};

//==============================================================================
void to_json (nlohmann::json&, const TestConfig&);
void from_json (const nlohmann::json&, TestConfig&);

} // namespace acceptance
