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

#include <juce_audio_formats/juce_audio_formats.h>
#include <nlohmann/json.hpp>

#include <memory>

namespace acceptance
{

//==============================================================================
/** The outcome of a single comparator run. */
struct ComparisonResult
{
    bool         passed = false;
    double       score  = 0.0;   /**< Method-specific metric, e.g. max abs diff. */
    juce::String summary;        /**< Human-readable one-liner. */
    nlohmann::json details;      /**< Structured detail for the machine-readable report. */
};

//==============================================================================
/**
    Pluggable comparison method. v1 ships only "sample"; further methods
    (spectrum, crosscorr, fingerprint) register later with no changes to config
    parsing or the runner.
*/
struct Comparator
{
    virtual ~Comparator() = default;

    /** The method name, e.g. "sample". */
    virtual juce::String getName() const = 0;

    /** Compares a freshly rendered buffer against the reference. config is the
        value sitting under this comparator's key in the config's "comparison"
        map (a bare number for "sample", or an object for richer methods). */
    virtual ComparisonResult compare (const juce::AudioBuffer<float>& reference,
                                      const juce::AudioBuffer<float>& output,
                                      const nlohmann::json& config) = 0;
};

//==============================================================================
/** Creates a comparator by name, or nullptr if the name is unknown. */
std::unique_ptr<Comparator> createComparator (const juce::String& name);

} // namespace acceptance
