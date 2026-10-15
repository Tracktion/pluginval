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

#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>

#include <utility>
#include <vector>

//==============================================================================
/**
    Machine-readable reports (JUnit XML and JSON) shared by the validate and
    test commands. Each command converts its own results into a Report, which is
    then written to whichever files were requested (--junit-xml / --json-results).
*/
namespace report_writer
{
    //==============================================================================
    /** One test within a run. */
    struct TestCase
    {
        enum class Outcome
        {
            passed,
            failed,     /**< The test ran and a check failed. */
            error       /**< The test couldn't be run to completion. */
        };

        juce::String name;
        Outcome status = Outcome::passed;
        juce::StringArray messages;     /**< Failure / error text. */
        double durationSeconds = 0.0;
        nlohmann::json details;         /**< Optional command-specific data (JSON report only). */
    };

    /** The results of one run of a command. */
    struct Report
    {
        juce::String command;           /**< "validate" or "test". */
        juce::String target;            /**< Plugin path/ID, or the acceptance test name. */
        juce::Time startTime = juce::Time::getCurrentTime();
        std::vector<std::pair<juce::String, juce::String>> properties;
        std::vector<TestCase> cases;
    };

    //==============================================================================
    /** Returns the report as a JUnit XML document (a single testsuite). */
    juce::String toJUnitXml (const Report&);

    /** Returns the report as JSON (snake_case keys). */
    nlohmann::ordered_json toJson (const Report&);

    /** A report with a single error case, for a run that didn't finish.
        Written before a run starts so a crash or timeout can't leave a stale
        (or missing) report behind; a completed run overwrites it.
    */
    Report makeIncomplete (const juce::String& command, const juce::String& target, const juce::String& reason);

    /** Writes the report to whichever files are set (a default juce::File is
        skipped), creating parent directories. Returns false, with a warning on
        stderr, if a file couldn't be written (which the commands treat as a
        failed run).
    */
    bool write (const Report&, const juce::File& junitXml, const juce::File& jsonResults);
} // namespace report_writer
