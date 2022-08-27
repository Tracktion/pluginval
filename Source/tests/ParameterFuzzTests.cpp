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
struct FuzzParametersTest  : public PluginTest
{
    FuzzParametersTest()
        : PluginTest ("Fuzz parameters", 6)
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        for (auto parameter : getNonBypassAutomatableParameters (instance))
            fuzzTestParameter (ut, *parameter);
    }

private:
    void fuzzTestParameter (PluginTests& ut, juce::AudioProcessorParameter& parameter)
    {
        auto r = ut.getRandom();
        ut.logVerboseMessage (juce::String ("Fuzz testing parameter: ") + juce::String (parameter.getParameterIndex()) + " - " + parameter.getName (512));

        for (int i = 0; i < 5; ++i)
        {
            const float value = r.nextFloat();

            parameter.setValue (value);
            const float v = parameter.getValue();

            const juce::String currentValueAsText = parameter.getCurrentValueAsText();
            const juce::String text = parameter.getText (value, 1024);
            const float valueForText = parameter.getValueForText (text);
            ignoreUnused (v, text, valueForText, currentValueAsText);
        }
    }
};

static FuzzParametersTest fuzzParametersTest;
