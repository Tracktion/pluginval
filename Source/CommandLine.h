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

#include "juce_core/juce_core.h"
#include "Validator.h"

//==============================================================================
struct CommandLineValidator
{
    CommandLineValidator();
    ~CommandLineValidator();

    void validate (const juce::String&, PluginTests::Options);

private:
    std::unique_ptr<ValidationPass> validator;
};

//==============================================================================
void performCommandLine (CommandLineValidator&, const juce::String& commandLine);
bool shouldPerformCommandLine (const juce::String& commandLine);

//==============================================================================
std::pair<juce::String, PluginTests::Options> parseCommandLine (const juce::String&);
std::pair<juce::String, PluginTests::Options> parseCommandLine (const juce::ArgumentList&);
juce::StringArray createCommandLine (juce::String fileOrID, PluginTests::Options);
