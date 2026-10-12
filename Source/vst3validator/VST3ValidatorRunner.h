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

#include <juce_core/juce_core.h>

namespace vst3validator {

/** Extracts the embedded vstvalidator binary to a temporary file.
    The returned TemporaryFile auto-deletes when destroyed, so the caller
    must keep it alive while the child process runs. */
std::unique_ptr<juce::TemporaryFile> getValidatorExecutable();

}  // namespace vst3validator
