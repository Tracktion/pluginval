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

#include "PluginvalSettings.h"
#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

/**
    JSON (de)serialisation for PluginvalSettings plus the couple of value
    conversions the CLI parser still needs (hex seed, disabled-tests file).
*/
namespace settings_serializer
{
    //==============================================================================
    /** Full JSON representation of a settings set (all fields present). */
    nlohmann::json toJson (const PluginvalSettings&);

    /** Parses a complete settings set from a JSON string (missing keys -> defaults). */
    PluginvalSettings fromJsonString (const std::string&);

    /** Parses a complete settings set from a JSON file (missing keys -> defaults). */
    PluginvalSettings fromJsonFile (const juce::File&);

    //==============================================================================
    /** Parses a random seed from "0x..." hex or a decimal integer, preserving the
        historical character-set validation. Throws std::invalid_argument if invalid.
    */
    std::int64_t parseRandomSeed (const juce::String& raw);

    /** Resolves --disabled-tests: an absolute path is read line-by-line, otherwise
        the value is treated as a comma-separated list.
    */
    std::vector<std::string> disabledTestsToList (const juce::String& raw);
}
