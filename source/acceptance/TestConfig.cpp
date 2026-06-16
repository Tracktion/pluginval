/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "TestConfig.h"

#include <stdexcept>

namespace acceptance
{

//==============================================================================
namespace
{
    /** The default comparison when none is supplied: one 16-bit LSB. */
    nlohmann::json defaultComparison()
    {
        return nlohmann::json { { "sample", 1.0 / 32768.0 } };
    }

    /** Reads a string member from a (possibly absent) nested object. */
    std::string getNestedString (const nlohmann::json& j, const char* outer, const char* inner)
    {
        if (auto o = j.find (outer); o != j.end() && o->is_object())
            if (auto i = o->find (inner); i != o->end() && i->is_string())
                return i->get<std::string>();

        return {};
    }
}

//==============================================================================
void to_json (nlohmann::json& j, const TestConfig& c)
{
    j = nlohmann::json::object();
    j["name"]   = c.name;
    j["plugin"] = c.plugin;

    if (! c.inputAudio.empty() || ! c.inputMidi.empty())
    {
        auto input = nlohmann::json::object();
        if (! c.inputAudio.empty()) input["audio"] = c.inputAudio;
        if (! c.inputMidi.empty())  input["midi"]  = c.inputMidi;
        j["input"] = input;
    }

    if (! c.reference.empty())
        j["reference"] = c.reference;

    if (! c.stateFile.empty() || ! c.stateParameters.empty())
    {
        auto state = nlohmann::json::object();
        if (! c.stateFile.empty())       state["file"]       = c.stateFile;
        if (! c.stateParameters.empty()) state["parameters"] = c.stateParameters;
        j["state"] = state;
    }

    j["sample_rate"] = c.sampleRate;
    j["block_size"]  = c.blockSize;

    if (c.renderDuration)
        j["render_duration"] = *c.renderDuration;

    if (! c.comparison.is_null())
        j["comparison"] = c.comparison;

    if (c.playhead)
    {
        j["playhead"] = {
            { "bpm",            c.playhead->bpm },
            { "time_signature", { { "numerator",   c.playhead->timeSigNumerator },
                                  { "denominator", c.playhead->timeSigDenominator } } },
            { "start_ppq",      c.playhead->startPpq }
        };
    }
}

void from_json (const nlohmann::json& j, TestConfig& c)
{
    c.name      = j.value ("name", std::string());
    c.plugin    = j.value ("plugin", std::string());
    c.reference = j.value ("reference", std::string());

    c.inputAudio = getNestedString (j, "input", "audio");
    c.inputMidi  = getNestedString (j, "input", "midi");

    c.stateFile = getNestedString (j, "state", "file");

    if (auto state = j.find ("state"); state != j.end() && state->is_object())
        if (auto params = state->find ("parameters"); params != state->end() && params->is_object())
            c.stateParameters = params->get<std::map<std::string, double>>();

    c.sampleRate = j.value ("sample_rate", 44100.0);
    c.blockSize  = j.value ("block_size", 512);

    if (auto d = j.find ("render_duration"); d != j.end() && d->is_number())
        c.renderDuration = d->get<double>();

    if (auto comp = j.find ("comparison"); comp != j.end() && ! comp->is_null())
        c.comparison = *comp;

    if (auto ph = j.find ("playhead"); ph != j.end() && ph->is_object())
    {
        TestConfig::PlayheadConfig p;
        p.bpm      = ph->value ("bpm", 120.0);
        p.startPpq = ph->value ("start_ppq", 0.0);

        if (auto ts = ph->find ("time_signature"); ts != ph->end() && ts->is_object())
        {
            p.timeSigNumerator   = ts->value ("numerator", 4);
            p.timeSigDenominator = ts->value ("denominator", 4);
        }

        if (p.timeSigNumerator <= 0 || p.timeSigDenominator <= 0)
            throw std::runtime_error ("playhead.time_signature numerator and denominator must be positive");

        c.playhead = p;
    }
}

//==============================================================================
nlohmann::json TestConfig::getComparison() const
{
    return comparison.is_null() ? defaultComparison() : comparison;
}

juce::String TestConfig::getPluginPathOrID() const
{
    const juce::String raw (plugin);

    // Resolve relative/home paths against the working directory; leave absolute
    // paths and bare component IDs (no '.' or '~') untouched. Mirrors
    // settings_parser::resolvePluginPath.
    if (raw.contains ("~") || raw.contains ("."))
        return juce::File::getCurrentWorkingDirectory().getChildFile (raw).getFullPathName();

    return raw;
}

juce::File TestConfig::resolveAgainstConfigDir (const std::string& path) const
{
    if (path.empty())
        return {};

    const juce::String s (path);

    if (juce::File::isAbsolutePath (s))
        return juce::File (s);

    const auto base = configDir == juce::File() ? juce::File::getCurrentWorkingDirectory() : configDir;
    return base.getChildFile (s);
}

juce::String TestConfig::getName() const
{
    if (! name.empty())
        return juce::String (name);

    return getReferenceFile().getFileNameWithoutExtension();
}

juce::File TestConfig::getReferenceFile() const
{
    if (! reference.empty())
        return resolveAgainstConfigDir (reference);

    const auto base = configDir == juce::File() ? juce::File::getCurrentWorkingDirectory() : configDir;
    return base.getChildFile (juce::String (name) + ".wav");
}

juce::File TestConfig::getReferenceSidecarFile() const
{
    const auto ref = getReferenceFile();
    return ref.getSiblingFile (ref.getFileName() + ".json");
}

juce::File TestConfig::getDiffFile() const
{
    const auto ref = getReferenceFile();
    return ref.getSiblingFile (ref.getFileNameWithoutExtension() + "-diff.wav");
}

juce::File TestConfig::getInputAudioFile() const { return resolveAgainstConfigDir (inputAudio); }
juce::File TestConfig::getInputMidiFile() const  { return resolveAgainstConfigDir (inputMidi); }
juce::File TestConfig::getStateFile() const      { return resolveAgainstConfigDir (stateFile); }

//==============================================================================
std::vector<TestConfig> TestConfig::loadFromFile (const juce::File& file)
{
    if (! file.existsAsFile())
        throw std::runtime_error (("test config not found: " + file.getFullPathName()).toStdString());

    nlohmann::json j;

    try
    {
        j = nlohmann::json::parse (file.loadFileAsString().toStdString());
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error (("failed to parse test config " + file.getFullPathName() + ": " + e.what()).toStdString());
    }

    std::vector<TestConfig> configs;

    const auto addOne = [&] (const nlohmann::json& entry)
    {
        auto c = entry.get<TestConfig>();
        c.configDir = file.getParentDirectory();
        configs.push_back (std::move (c));
    };

    if (j.is_array())
    {
        for (const auto& entry : j)
            addOne (entry);
    }
    else
    {
        addOne (j);
    }

    return configs;
}

} // namespace acceptance
