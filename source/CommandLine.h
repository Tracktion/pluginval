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
#include "acceptance/AcceptanceTest.h"

//==============================================================================
struct CommandLineValidator
{
    CommandLineValidator();
    ~CommandLineValidator();

    void validate (const juce::String&, PluginTests::Options);
    void runAcceptanceTest (const juce::File& configFile, const acceptance::RunOptions&);

private:
    std::unique_ptr<ValidationPass> validator;
    std::unique_ptr<acceptance::TestRunner> testRunner;
};

//==============================================================================
void performCommandLine (CommandLineValidator&, const juce::String& commandLine);
bool shouldPerformCommandLine (const juce::String& commandLine);

//==============================================================================
/** Parses a command line into the plugin path/ID and resolved test options. */
std::pair<juce::String, PluginTests::Options> parseCommandLine (const juce::String&);

/** Serialises options for the child validation process. */
juce::StringArray createCommandLine (juce::String fileOrID, PluginTests::Options);
