/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

/**
 * Wrapper file for VST3 SDK module_win32.cpp
 * This wrapper ensures proper preprocessor setup before including the SDK source.
 */

#if defined(_WIN32)

// Prevent Windows headers from defining min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Reduce Windows header bloat
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Silence deprecation warning for experimental/filesystem
#ifndef _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#endif

// Ensure _UNICODE is defined for proper Windows API usage
#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef UNICODE
#define UNICODE
#endif

// Include the actual VST3 SDK module implementation
// Note: The path is relative to this file's location, but the build system
// sets up include directories so we can use the SDK path directly
#include "public.sdk/source/vst/hosting/module_win32.cpp"

#endif // _WIN32
