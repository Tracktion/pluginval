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

#include "ReferenceComparator.h"

#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>

#include <utility>
#include <vector>

namespace acceptance
{

//==============================================================================
/** The result of running one acceptance test. */
struct TestResult
{
    enum class Outcome
    {
        referenceCreated,   /**< No reference existed, so one was recorded. Treated as success. */
        passed,             /**< Reference existed and every comparator passed. */
        failed,             /**< Reference existed and at least one comparator failed. */
        error               /**< Setup failed (plugin load, render, IO, bad config, ...). */
    };

    Outcome outcome = Outcome::error;
    juce::String name;
    juce::String message;       /**< Error text, or an overall summary. */

    std::vector<std::pair<juce::String, ComparisonResult>> comparisons; /**< method name -> result. */

    juce::File referenceFile;
    juce::File diffFile;        /**< Written only on failure. */

    //==============================================================================
    static TestResult makeError (const juce::String& name, const juce::String& message)
    {
        TestResult r;
        r.outcome = Outcome::error;
        r.name = name;
        r.message = message;
        return r;
    }
};

//==============================================================================
namespace reporter
{
    /** Structured, machine-readable representation of a result. */
    nlohmann::json toJson (const TestResult&);

    /** Prints a human-readable summary followed by the JSON to stdout. */
    void report (const TestResult&);

    /** 0 for referenceCreated / passed, 1 for failed / error. */
    int exitCode (const TestResult&);
}

} // namespace acceptance
