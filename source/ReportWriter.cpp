/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "ReportWriter.h"

#include <cmath>
#include <iostream>

namespace report_writer
{

//==============================================================================
namespace
{
    struct Totals
    {
        int tests = 0, failed = 0, errors = 0;
        double seconds = 0.0;
    };

    Totals getTotals (const Report& report)
    {
        Totals t;

        for (const auto& c : report.cases)
        {
            ++t.tests;
            t.failed += c.status == TestCase::Status::failed ? 1 : 0;
            t.errors += c.status == TestCase::Status::error ? 1 : 0;
            t.seconds += c.durationSeconds;
        }

        return t;
    }

    const char* toString (TestCase::Status s)
    {
        switch (s)
        {
            case TestCase::Status::passed:  return "passed";
            case TestCase::Status::failed:  return "failed";
            case TestCase::Status::error:   return "error";
        }

        return "error";
    }

    juce::String formatSeconds (double seconds)
    {
        return juce::String (seconds, 3);
    }

    juce::int64 toMilliseconds (double seconds)
    {
        return (juce::int64) std::llround (seconds * 1000.0);
    }
}

//==============================================================================
juce::String toJUnitXml (const Report& report)
{
    const auto totals = getTotals (report);

    const auto setTotals = [&totals] (juce::XmlElement& e)
    {
        e.setAttribute ("tests", totals.tests);
        e.setAttribute ("failures", totals.failed);
        e.setAttribute ("errors", totals.errors);
        e.setAttribute ("time", formatSeconds (totals.seconds));
    };

    // A plugin path makes for an unwieldy suite name, so use just the file name
    const auto shortTarget = juce::File::isAbsolutePath (report.target) ? juce::File (report.target).getFileName()
                                                                        : report.target;

    juce::XmlElement root ("testsuites");
    root.setAttribute ("name", "pluginval");
    setTotals (root);

    auto suite = root.createNewChildElement ("testsuite");
    suite->setAttribute ("name", "pluginval " + report.command + ": " + shortTarget);
    setTotals (*suite);
    suite->setAttribute ("skipped", 0);
    suite->setAttribute ("timestamp", report.startTime.toISO8601 (true));

    if (! report.properties.empty())
    {
        auto properties = suite->createNewChildElement ("properties");

        for (const auto& [name, value] : report.properties)
        {
            auto property = properties->createNewChildElement ("property");
            property->setAttribute ("name", name);
            property->setAttribute ("value", value);
        }
    }

    for (const auto& c : report.cases)
    {
        auto testCase = suite->createNewChildElement ("testcase");

        // A fixed classname: plugin names contain dots, which JUnit viewers
        // treat as package separators
        testCase->setAttribute ("classname", "pluginval." + report.command);
        testCase->setAttribute ("name", c.name);
        testCase->setAttribute ("time", formatSeconds (c.durationSeconds));

        if (c.status != TestCase::Status::passed)
        {
            auto problem = testCase->createNewChildElement (c.status == TestCase::Status::failed ? "failure" : "error");
            problem->setAttribute ("message", c.messages[0].upToFirstOccurrenceOf ("\n", false, false));
            problem->addTextElement (c.messages.joinIntoString ("\n"));
        }
    }

    return root.toString();
}

nlohmann::ordered_json toJson (const Report& report)
{
    const auto totals = getTotals (report);

    nlohmann::ordered_json j;
    j["pluginval_version"] = VERSION;
    j["command"]           = report.command.toStdString();
    j["target"]            = report.target.toStdString();
    j["start_time"]        = report.startTime.toISO8601 (true).toStdString();
    j["duration_ms"]       = toMilliseconds (totals.seconds);

    auto properties = nlohmann::ordered_json::object();

    for (const auto& [name, value] : report.properties)
        properties[name.toStdString()] = value.toStdString();

    j["properties"] = properties;

    j["summary"] = { { "tests",  totals.tests },
                     { "passed", totals.tests - totals.failed - totals.errors },
                     { "failed", totals.failed },
                     { "errors", totals.errors } };

    auto tests = nlohmann::ordered_json::array();

    for (const auto& c : report.cases)
    {
        nlohmann::ordered_json test;
        test["name"]        = c.name.toStdString();
        test["status"]      = toString (c.status);
        test["duration_ms"] = toMilliseconds (c.durationSeconds);

        auto messages = nlohmann::ordered_json::array();

        for (const auto& m : c.messages)
            messages.push_back (m.toStdString());

        test["messages"] = messages;

        if (! c.details.is_null())
            test["details"] = c.details;

        tests.push_back (test);
    }

    j["tests"] = tests;
    return j;
}

Report makeIncomplete (const juce::String& command, const juce::String& target, const juce::String& reason)
{
    TestCase c;
    c.name = "pluginval " + command;
    c.status = TestCase::Status::error;
    c.messages.add (reason);

    Report report;
    report.command = command;
    report.target = target;
    report.cases.push_back (c);
    return report;
}

//==============================================================================
bool write (const Report& report, const juce::File& junitXml, const juce::File& jsonResults)
{
    const auto writeFile = [] (const juce::File& file, const juce::String& text)
    {
        if (file.getParentDirectory().createDirectory().wasOk()
            && file.replaceWithText (text, false, false, "\n"))
            return true;

        std::cerr << "!!! WARNING: Unable to write the report to " << file.getFullPathName() << std::endl;
        return false;
    };

    bool ok = true;

    if (junitXml != juce::File())
        ok = writeFile (junitXml, toJUnitXml (report)) && ok;

    if (jsonResults != juce::File())
        ok = writeFile (jsonResults, juce::String::fromUTF8 (toJson (report).dump (2, ' ', false, nlohmann::ordered_json::error_handler_t::replace).c_str()) + "\n") && ok;

    return ok;
}

} // namespace report_writer
