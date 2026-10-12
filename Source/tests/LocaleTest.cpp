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
struct LocaleTest   : public PluginTest
{
    LocaleTest()
        : PluginTest ("Ensuring that the locale does not change during execution", 1,
                      { Requirements::Thread::messageThread, Requirements::GUI::requiresGUI })
    {
        startupLocale = setlocale(LC_ALL, nullptr);
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        ut.logMessage (juce::String ("INFO: Startup Locale: [LOC]")
                           .replace ("LOC",juce::String (startupLocale), false));

        if (instance.hasEditor())
        {
            ScopedPluginDeinitialiser scopedPluginDeinitialiser (instance);

            {
                ut.logMessage ("Opening editor...");
                ScopedEditorShower editor (instance);
            }
        }

        std::string newLocale = setlocale(LC_ALL, nullptr);
        ut.expectEquals (startupLocale, newLocale, "Plugin changed locale. This can cause unexpected behavior.");
        ut.logMessage (juce::String ("INFO: Shutdown Locale: [LOC]")
                           .replace ("LOC",juce::String (newLocale), false));

    }

    std::vector<TestDescription> getDescription (int) const override
    {
        return { { "Locale stability", "Checks that the plugin doesn't change the system locale. "
                   "Some plugins or GUI frameworks change locale settings (e.g., decimal separator from '.' to ','). "
                   "This can corrupt preset files, break float parsing in other plugins, or crash the host" } };
    }

private:
    std::string startupLocale;
};

static LocaleTest localeTest;

