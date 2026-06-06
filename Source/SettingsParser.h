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

/**
    The command-line -> settings pipeline.

    CLI11 binds each option directly to a member of a single PluginvalSettings
    instance (environment variables via ->envname() on the same line). A
    --config JSON file seeds the struct before parsing so precedence is
    defaults < config < env < CLI. Conversion to the JUCE-flavoured
    PluginTests::Options happens at the boundary via toPluginTestOptions().
*/
namespace settings_parser
{
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
    ParseResult parseTokens (const juce::StringArray& tokens);

    /** Convenience: preprocess + parse from a raw command line, returning settings. */
    PluginvalSettings parse (const juce::String& commandLine);

    //==============================================================================
    /** Serialises options for the child validation process as a base64 JSON
        handoff: { exe, --config-base64 <b64>, --validate <fileOrID> }.
    */
    juce::StringArray createChildProcessCommandLine (const juce::String& fileOrID, const PluginTests::Options&);

    /** The "--version" output string. */
    juce::String getVersionString();
}
