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
        : PluginTest ("Fuzz parameters", 6, getDefaultRequirements())
    {
    }

    void runTest (PluginTests& ut, AudioPluginInstance& instance) override
    {
        for (auto parameter : instance.getParameters())
            fuzzTestParameter (ut, *parameter);
    }

private:
    void fuzzTestParameter (UnitTest& ut, AudioProcessorParameter& parameter)
    {
        ut.logMessage (String ("Fuzz testing parameter: ") + String (parameter.getParameterIndex()) + " - " + parameter.getName (512));

        for (int i = 0; i < 5; ++i)
        {
            const float value = ut.getRandom().nextFloat();

            parameter.setValue (value);
            const float v = parameter.getValue();

            const String currentValueAsText = parameter.getCurrentValueAsText();
            const String text = parameter.getText (value, 1024);
            const float valueForText = parameter.getValueForText (text);
            ignoreUnused (v, text, valueForText, currentValueAsText);
        }
    }
};

static FuzzParametersTest fuzzParametersTest;
