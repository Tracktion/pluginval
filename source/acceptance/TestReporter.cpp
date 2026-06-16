/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "TestReporter.h"

#include <iostream>

namespace acceptance::reporter
{

//==============================================================================
static const char* toString (TestResult::Outcome o)
{
    switch (o)
    {
        case TestResult::Outcome::referenceCreated: return "reference-created";
        case TestResult::Outcome::passed:           return "passed";
        case TestResult::Outcome::failed:           return "failed";
        case TestResult::Outcome::error:            return "error";
    }

    return "error";
}

nlohmann::json toJson (const TestResult& r)
{
    nlohmann::json j;
    j["name"]    = r.name.toStdString();
    j["outcome"] = toString (r.outcome);

    if (r.message.isNotEmpty())
        j["message"] = r.message.toStdString();

    if (r.referenceFile != juce::File())
        j["reference"] = r.referenceFile.getFullPathName().toStdString();

    if (r.diffFile != juce::File())
        j["diff"] = r.diffFile.getFullPathName().toStdString();

    if (! r.comparisons.empty())
    {
        auto comparisons = nlohmann::json::object();

        for (const auto& [method, result] : r.comparisons)
        {
            nlohmann::json c;
            c["passed"]  = result.passed;
            c["score"]   = result.score;
            c["summary"] = result.summary.toStdString();
            if (! result.details.is_null())
                c["details"] = result.details;

            comparisons[method.toStdString()] = c;
        }

        j["comparisons"] = comparisons;
    }

    return j;
}

void report (const TestResult& r)
{
    switch (r.outcome)
    {
        case TestResult::Outcome::referenceCreated:
            std::cout << "Reference created: " << r.referenceFile.getFullPathName() << std::endl;
            break;

        case TestResult::Outcome::passed:
            std::cout << "PASSED: " << r.name << std::endl;
            break;

        case TestResult::Outcome::failed:
            std::cout << "*** FAILED: " << r.name << std::endl;
            break;

        case TestResult::Outcome::error:
            std::cout << "*** ERROR: " << r.name << ": " << r.message << std::endl;
            break;
    }

    for (const auto& [method, result] : r.comparisons)
        std::cout << "  [" << method << "] " << (result.passed ? "ok" : "FAIL") << ": " << result.summary << std::endl;

    if (r.diffFile != juce::File())
        std::cout << "  diff written to: " << r.diffFile.getFullPathName() << std::endl;

    // The structured result for machine consumers.
    std::cout << toJson (r).dump (2) << std::endl;
}

int exitCode (const TestResult& r)
{
    return (r.outcome == TestResult::Outcome::referenceCreated
            || r.outcome == TestResult::Outcome::passed) ? 0 : 1;
}

} // namespace acceptance::reporter
