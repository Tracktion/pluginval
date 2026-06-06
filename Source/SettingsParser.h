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
#include "PluginTests.h"

#include <functional>

/**
    The command-line -> settings pipeline.

    Raw command line is preprocessed into argv tokens, then each input layer
    (config file, environment, CLI) becomes a sparse JSON object. The layers are
    merged (CLI wins last) and deserialised into a single PluginvalSettings.
*/
namespace settings_parser
{
    /** Returns the value of an environment variable, or "" if unset. */
    using EnvProvider = std::function<juce::String (const juce::String& name)>;
    juce::String systemEnv (const juce::String& name);

    //==============================================================================
    /** Tokenises + preprocesses a raw command line: rewrites the deprecated
        "strictnessLevel", strips the macOS "-NSDocumentRevisionsDebugMode YES"
        flag, and inserts an implicit "--validate" when the last argument is a
        bare plugin path. Returns argv tokens (without the program name).
    */
    juce::StringArray preprocess (const juce::String& commandLine);

    /** True if the tokens contain a recognised command that triggers CLI mode. */
    bool isCommandLine (const juce::StringArray& tokens);

    //==============================================================================
    /** Resolves the merged settings from preprocessed tokens + environment. */
    PluginvalSettings resolveSettings (const juce::StringArray& tokens, const EnvProvider& env = systemEnv);

    /** Convenience: preprocess + resolveSettings from a raw command line. */
    PluginvalSettings parse (const juce::String& commandLine, const EnvProvider& env = systemEnv);

    /** Resolves a relative/home plugin path against the working directory,
        leaving absolute paths and bare component IDs untouched.
    */
    juce::String resolvePluginPath (const juce::String& raw);

    //==============================================================================
    /** Serialises options for the child validation process as a base64 JSON
        handoff: { exe, --config-base64 <b64>, --validate <fileOrID> }.
    */
    juce::StringArray createChildProcessCommandLine (const juce::String& fileOrID, const PluginTests::Options&);

    //==============================================================================
    /** Prints the auto-generated usage (magic_args) plus the environment-variable
        and commands trailer to stdout.
    */
    void printHelp (const juce::String& exeName);

    /** The "--version" output string. */
    juce::String getVersionString();
}
