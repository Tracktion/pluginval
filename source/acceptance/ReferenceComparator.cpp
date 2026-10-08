/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "ReferenceComparator.h"

#include <cmath>
#include <limits>

namespace acceptance
{

//==============================================================================
/**
    Per-sample absolute-difference comparator.

    config is the value under the "sample" key: a bare number giving the
    tolerance (0 == bit-exact), or an object { "tolerance": <number> }. Channel
    and length mismatches fail outright, as does any non-finite (NaN / inf)
    difference.
*/
struct SampleComparator : public Comparator
{
    juce::String getName() const override   { return "sample"; }

    static double toleranceFrom (const nlohmann::ordered_json& config)
    {
        if (config.is_number())
            return config.get<double>();

        if (config.is_object())
            if (auto t = config.find ("tolerance"); t != config.end() && t->is_number())
                return t->get<double>();

        return 1.0 / 32768.0;
    }

    ComparisonResult compare (const juce::AudioBuffer<float>& reference,
                              const juce::AudioBuffer<float>& output,
                              const nlohmann::ordered_json& config) override
    {
        const double tolerance = toleranceFrom (config);

        ComparisonResult r;
        r.details["tolerance"] = tolerance;

        if (reference.getNumChannels() != output.getNumChannels()
            || reference.getNumSamples() != output.getNumSamples())
        {
            r.passed = false;
            r.summary = "size mismatch: reference "
                      + juce::String (reference.getNumChannels()) + "ch x " + juce::String (reference.getNumSamples()) + " samples, "
                      + "output " + juce::String (output.getNumChannels()) + "ch x " + juce::String (output.getNumSamples()) + " samples";
            r.details["reference_channels"] = reference.getNumChannels();
            r.details["reference_samples"]  = reference.getNumSamples();
            r.details["output_channels"]    = output.getNumChannels();
            r.details["output_samples"]     = output.getNumSamples();
            return r;
        }

        double maxAbsDiff = 0.0;
        bool nonFinite = false;
        int firstFailChannel = -1, firstFailSample = -1;

        for (int c = 0; c < reference.getNumChannels(); ++c)
        {
            const auto* ref = reference.getReadPointer (c);
            const auto* out = output.getReadPointer (c);

            for (int s = 0; s < reference.getNumSamples(); ++s)
            {
                const double diff = std::abs ((double) out[s] - (double) ref[s]);

                // NaN compares false both ways, so test for failure with
                // ! (diff <= tolerance) and track non-finite diffs separately.
                if (! std::isfinite (diff))
                    nonFinite = true;
                else if (diff > maxAbsDiff)
                    maxAbsDiff = diff;

                if (! (diff <= tolerance) && firstFailChannel < 0)
                {
                    firstFailChannel = c;
                    firstFailSample = s;
                }
            }
        }

        r.score = nonFinite ? std::numeric_limits<double>::infinity() : maxAbsDiff;
        r.passed = firstFailChannel < 0;
        r.details["max_abs_diff"] = maxAbsDiff;
        r.details["non_finite"] = nonFinite;

        if (r.passed)
        {
            r.summary = "max abs diff " + juce::String (maxAbsDiff) + " <= tolerance " + juce::String (tolerance);
        }
        else
        {
            const auto firstFail = " (first at channel " + juce::String (firstFailChannel) + ", sample " + juce::String (firstFailSample) + ")";

            if (nonFinite)
                r.summary = "non-finite (NaN / inf) difference; finite max abs diff " + juce::String (maxAbsDiff) + firstFail;
            else
                r.summary = "max abs diff " + juce::String (maxAbsDiff) + " > tolerance " + juce::String (tolerance) + firstFail;
            r.details["first_fail_channel"] = firstFailChannel;
            r.details["first_fail_sample"]  = firstFailSample;
        }

        return r;
    }
};

//==============================================================================
std::unique_ptr<Comparator> createComparator (const juce::String& name)
{
    if (name == "sample")
        return std::make_unique<SampleComparator>();

    // Future: "peakrms", "spectrum", "crosscorr", "fingerprint" register here.
    return {};
}

} // namespace acceptance
