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

#include <string>

namespace vst3validator {

/** Options for running the VST3 validator. */
struct Options
{
    std::string pluginPath;     /**< Path to the VST3 plugin to validate. */
    bool extendedMode = false;  /**< If true, run extended validation tests. */
    bool verbose = false;       /**< If true, output verbose information. */
};

/** Result of running the VST3 validator. */
struct Result
{
    bool success = false;       /**< True if all tests passed. */
    std::string output;         /**< Captured output from the validator. */
    int exitCode = 1;           /**< Exit code (0 = success). */
};

/**
    Runs the Steinberg VST3 validator on a plugin.

    This function calls the embedded VST3 SDK validator code directly,
    capturing its output and returning the results.

    @param options  The validation options including plugin path
    @return         The validation result including captured output
*/
Result runValidator (const Options& options);

}  // namespace vst3validator
