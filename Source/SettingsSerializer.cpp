/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "SettingsSerializer.h"

#include <stdexcept>

namespace settings_serializer
{
    nlohmann::json toJson (const PluginvalSettings& s)
    {
        return nlohmann::json (s);
    }

    PluginvalSettings fromJsonString (const std::string& jsonString)
    {
        return nlohmann::json::parse (jsonString).get<PluginvalSettings>();
    }

    PluginvalSettings fromJsonFile (const juce::File& file)
    {
        return fromJsonString (file.loadFileAsString().toStdString());
    }

    //==============================================================================
    std::int64_t parseRandomSeed (const juce::String& raw)
    {
        if (! raw.containsOnly ("x-0123456789acbdef"))
            throw std::invalid_argument ("Invalid random seed argument!");

        if (raw.startsWith ("0x"))
            return raw.getHexValue64();

        return raw.getLargeIntValue();
    }

    nlohmann::json commaToDoubleArray (const juce::String& raw)
    {
        auto out = nlohmann::json::array();

        for (const auto& token : juce::StringArray::fromTokens (raw, ",", "\""))
            out.push_back (token.getDoubleValue());

        return out;
    }

    nlohmann::json commaToIntArray (const juce::String& raw)
    {
        auto out = nlohmann::json::array();

        for (const auto& token : juce::StringArray::fromTokens (raw, ",", "\""))
            out.push_back (token.getIntValue());

        return out;
    }

    nlohmann::json disabledTestsToArray (const juce::String& raw)
    {
        auto out = nlohmann::json::array();

        if (juce::File::isAbsolutePath (raw))
        {
            juce::StringArray lines;
            juce::File (raw).readLines (lines);

            for (const auto& line : lines)
                out.push_back (line.toStdString());

            return out;
        }

        for (const auto& token : juce::StringArray::fromTokens (raw, ",", ""))
            out.push_back (token.toStdString());

        return out;
    }
}
