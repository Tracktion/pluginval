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

    A single PluginvalSettings is filled by successive layers, lowest to highest
    precedence: hardcoded defaults, then environment variables, then --config
    (repeatable JSON files, later files win per key), then the individual CLI options.
    CLI11 binds each option to a member and also provides the coercion used for
    the environment layer (env names are derived from the registered options, so
    there is no separate env table). Conversion to the JUCE-flavoured
    PluginTests::Options happens at the boundary via toPluginTestOptions().
*/
namespace settings_parser
{
    //==============================================================================
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

    /** Resolves a relative/home plugin path against the working directory,
        leaving absolute paths and bare component IDs untouched.
    */
    juce::String resolvePluginPath (const juce::String& raw);

    //==============================================================================
    /** The outcome of parsing the option tokens. */
    struct ParseResult
    {
        PluginvalSettings settings;
        bool handled = false;   /**< true if --help/--version/parse-error was handled and the app should exit. */
        int exitCode = 0;       /**< the exit code to use when handled is true. */
    };

    /** Parses preprocessed option tokens into settings (and handles --help/--version). */
    ParseResult parseTokens (const juce::StringArray& tokens, const EnvProvider& env = systemEnv);

    /** Convenience: preprocess + parse from a raw command line, returning settings. */
    PluginvalSettings parse (const juce::String& commandLine, const EnvProvider& env = systemEnv);

    //==============================================================================
    /** Serialises options for the child validation process as a base64 JSON
        handoff: { exe, --config-base64 <b64>, --validate <fileOrID> }.
    */
    juce::StringArray createChildProcessCommandLine (const juce::String& fileOrID, const PluginTests::Options&);

    /** The "--version" output string. */
    juce::String getVersionString();
}
