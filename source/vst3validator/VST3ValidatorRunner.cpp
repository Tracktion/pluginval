/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "VST3ValidatorRunner.h"
#include "vstvalidator_data.h"  // Generated at build time

#if ! JUCE_WINDOWS
 #include <sys/stat.h>
#endif

namespace vst3validator {

std::unique_ptr<juce::TemporaryFile> getValidatorExecutable()
{
   #if JUCE_WINDOWS
    auto tempFile = std::make_unique<juce::TemporaryFile> (".exe");
   #else
    auto tempFile = std::make_unique<juce::TemporaryFile> ();
   #endif

    auto exe = tempFile->getFile();
    exe.replaceWithData (vstvalidator_binary, vstvalidator_binary_len);

   #if ! JUCE_WINDOWS
    chmod (exe.getFullPathName().toRawUTF8(), 0755);
   #endif

    return tempFile;
}

}  // namespace vst3validator
