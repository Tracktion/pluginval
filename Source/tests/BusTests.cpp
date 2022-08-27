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
struct BasicBusTest   : public PluginTest
{
    BasicBusTest()
        : PluginTest ("Basic bus", 4)
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        const ScopedPluginDeinitialiser deinitialiser (instance);
        const auto currentLayout = instance.getBusesLayout();

        ut.beginTest ("Listing available buses");
        {
            listBuses (ut, instance, true);
            listBuses (ut, instance, false);

            ut.logMessage ("Main bus num input channels: " + juce::String (instance.getMainBusNumInputChannels()));
            ut.logMessage ("Main bus num output channels: " + juce::String (instance.getMainBusNumOutputChannels()));
        }

        ut.beginTest ("Enabling all buses");
        {
            if (! instance.enableAllBuses())
                ut.logMessage ("!!! WARNING: Enabling all buses failed");
        }

        ut.beginTest ("Disabling non-main busses");
        {
            if (! instance.disableNonMainBuses())
                ut.logMessage ("!!! WARNING: Disabling non-main buses failed");
        }

        ut.beginTest ("Restoring default layout");
        {
            ut.expect (instance.setBusesLayout (currentLayout), "Unable to restore default layout");
            ut.logMessage ("Main bus num input channels: " + juce::String (instance.getMainBusNumInputChannels()));
            ut.logMessage ("Main bus num output channels: " + juce::String (instance.getMainBusNumOutputChannels()));
        }
    }

    static void listBuses (PluginTests& ut, juce::AudioPluginInstance& instance, bool inputs)
    {
        const int numBuses = instance.getBusCount (inputs);
        juce::StringArray namedLayouts, discreteLayouts;

        for (int busNum = 0; busNum < numBuses; ++busNum)
        {
            if (auto* bus = instance.getBus (inputs, busNum))
            {
                for (auto& set : supportedLayoutsWithNamedChannels (*bus))
                    namedLayouts.add (set.getDescription());

                for (auto& set : supportedLayoutsWithDiscreteChannels (*bus))
                    discreteLayouts.add (set.getDescription());
            }
        }

        ut.logMessage (inputs ? "Inputs:" : "Outputs:");
        ut.logMessage ("\tNamed layouts: " + (namedLayouts.isEmpty() ?juce::String ("None") : namedLayouts.joinIntoString (", ")));
        ut.logMessage ("\tDiscrete layouts: " + (discreteLayouts.isEmpty() ?juce::String ("None") : discreteLayouts.joinIntoString (", ")));
    }

    static juce::Array<juce::AudioChannelSet> supportedLayoutsWithNamedChannels (juce::AudioProcessor::Bus& bus)
    {
        juce::Array<juce::AudioChannelSet> sets;

        for (int i = 0; i <= juce::AudioChannelSet::maxChannelsOfNamedLayout; ++i)
        {
            juce::AudioChannelSet set;

            if (! (set = juce::AudioChannelSet::namedChannelSet  (i)).isDisabled() && bus.isLayoutSupported (set))
                sets.add (set);
        }

        return sets;
    }

    static juce::Array<juce::AudioChannelSet> supportedLayoutsWithDiscreteChannels (juce::AudioProcessor::Bus& bus)
    {
        juce::Array<juce::AudioChannelSet> sets;

        for (int i = 0; i <= 32; ++i)
        {
            juce::AudioChannelSet set;

            if (! (set = juce::AudioChannelSet::discreteChannels (i)).isDisabled() && bus.isLayoutSupported (set))
                sets.add (set);
        }

        return sets;
    }
};

static BasicBusTest basicBusTest;
