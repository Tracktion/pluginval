/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "../PluginTests.h"
#include "../TestUtilities.h"


//==============================================================================
/** Opens the plugin editor in various situations. */
struct EditorStressTest   : public PluginTest
{
    EditorStressTest()
        : PluginTest ("Editor stress", 6,
                      { Requirements::Thread::messageThread, Requirements::GUI::requiresGUI })
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        if (instance.hasEditor())
        {
            ScopedPluginDeinitialiser scopedPluginDeinitialiser (instance);

            {
                ut.logMessage ("Testing opening Editor with released processing");
                ScopedEditorShower editor (instance);
            }

            {
                ut.logMessage ("Testing opening Editor with 0 sample rate and block size");
                instance.setRateAndBufferSizeDetails (0.0, 0);
                ScopedEditorShower editor (instance);
            }
        }
    }
};

static EditorStressTest editorStressTest;

#if (JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE) || DOXYGEN
//==============================================================================
/** Opens the plugin editor in various situations. */
struct EditorDPITest   : public PluginTest
{
    EditorDPITest()
        : PluginTest ("Editor DPI Awareness", 3,
                      { Requirements::Thread::messageThread, Requirements::GUI::requiresGUI })
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        if (instance.hasEditor())
        {
            ScopedDPIAwarenessDisabler scopedDPIAwarenessDisabler;

            {
                ut.logMessage ("Testing opening Editor with DPI Awareness disabled");
                ScopedEditorShower editor (instance);
            }
        }
    }
};

static EditorDPITest editorDPITest;
#endif
