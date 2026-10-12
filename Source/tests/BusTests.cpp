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

    std::vector<TestDescription> getDescription (int) const override
    {
        return { { name, "Tests audio bus configuration: lists all supported input/output layouts "
                         "(named channel sets like stereo/5.1 and discrete channels), "
                         "tests enableAllBuses() and disableNonMainBuses(), "
                         "then verifies the default layout can be restored" } };
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

//==============================================================================
/** Exhaustively tries every supported combination of main + auxiliary (e.g.
    sidechain) channel layouts and calls prepareToPlay / processBlock for each.

    This is intended to catch crashes that only occur for non-default channel
    counts — typically a plugin that hardcodes assumptions about its sidechain
    being stereo and then accesses out-of-range channels when the host
    reconfigures it to mono. auval exercises this with its "1 Channel Test"
    but pluginval's other tests only process at the plugin's default layout.
*/
struct BusLayoutProcessingTest   : public PluginTest
{
    BusLayoutProcessingTest()
        : PluginTest ("Bus layout processing", 4)
    {
    }

    std::vector<TestDescription> getDescription (int) const override
    {
        return { { name, "For every supported combination of mono/stereo on each "
                         "input and output bus (including sidechain), calls "
                         "setBusesLayout(), prepareToPlay() and processBlock(). "
                         "Detects crashes in plugins that mis-handle non-default "
                         "channel counts (e.g. mono main with stereo sidechain)." } };
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        const ScopedPluginDeinitialiser deinitialiser (instance);
        const auto originalLayout = instance.getBusesLayout();

        const std::vector<double>& sampleRates = ut.getOptions().sampleRates;
        const std::vector<int>& blockSizes = ut.getOptions().blockSizes;
        jassert (sampleRates.size() > 0 && blockSizes.size() > 0);

        const double sampleRate = sampleRates.front();
        const int blockSize = blockSizes.front();

        const auto candidateSets = getCandidateChannelSets();

        // Build all candidate layouts by varying each bus independently across
        // the candidate sets, keeping the bus enabled or disabling it.
        std::vector<juce::AudioProcessor::BusesLayout> layouts;
        layouts.push_back (originalLayout);
        enumerateLayouts (instance, originalLayout, candidateSets, layouts);

        ut.logMessage ("Total candidate layouts to test: " + juce::String ((int) layouts.size()));

        int numTested = 0;
        int numAccepted = 0;

        for (const auto& layout : layouts)
        {
            ++numTested;

            ut.logVerboseMessage ("Trying layout: " + describeLayout (layout));

            callReleaseResourcesOnMessageThreadIfVST3 (instance);

            if (! instance.setBusesLayout (layout))
            {
                ut.logVerboseMessage ("  setBusesLayout() rejected the layout");
                continue;
            }

            ++numAccepted;

            callPrepareToPlayOnMessageThreadIfVST3 (instance, sampleRate, blockSize);

            const int numChannelsRequired = juce::jmax (instance.getTotalNumInputChannels(),
                                                        instance.getTotalNumOutputChannels());

            if (numChannelsRequired <= 0)
                continue;

            juce::AudioBuffer<float> ab (numChannelsRequired, blockSize);
            juce::MidiBuffer mb;
            mb.ensureSize (32);

            for (int i = 0; i < 4; ++i)
            {
                fillNoise (ab);
                instance.processBlock (ab, mb);
                mb.clear();
            }
        }

        ut.logMessage ("Layouts tested: " + juce::String (numTested)
                          + ", accepted by setBusesLayout: " + juce::String (numAccepted));

        // Restore the original configuration
        callReleaseResourcesOnMessageThreadIfVST3 (instance);
        instance.setBusesLayout (originalLayout);
    }

private:
    static juce::Array<juce::AudioChannelSet> getCandidateChannelSets()
    {
        // Keep the set small to avoid combinatorial explosion. These cover the
        // common cases that hosts use and that plugin authors most often get
        // wrong: mono and stereo, with the option of disabling the bus.
        juce::Array<juce::AudioChannelSet> sets;
        sets.add (juce::AudioChannelSet::disabled());
        sets.add (juce::AudioChannelSet::mono());
        sets.add (juce::AudioChannelSet::stereo());
        return sets;
    }

    static void enumerateLayouts (juce::AudioPluginInstance& instance,
                                  const juce::AudioProcessor::BusesLayout& base,
                                  const juce::Array<juce::AudioChannelSet>& candidates,
                                  std::vector<juce::AudioProcessor::BusesLayout>& out)
    {
        const int numInputs  = base.inputBuses.size();
        const int numOutputs = base.outputBuses.size();

        // Guard against combinatorial blow-up on plugins with many buses.
        // 3 ^ 6 = 729 which is already plenty; anything bigger we skip to keep
        // the test runtime reasonable.
        const int totalBuses = numInputs + numOutputs;

        if (totalBuses == 0 || totalBuses > 6)
            return;

        const int numCandidates = candidates.size();
        juce::int64 total = 1;

        for (int i = 0; i < totalBuses; ++i)
            total *= numCandidates;

        for (juce::int64 combo = 0; combo < total; ++combo)
        {
            juce::AudioProcessor::BusesLayout layout = base;
            juce::int64 remaining = combo;

            for (int i = 0; i < numInputs; ++i)
            {
                layout.inputBuses.getReference (i) = candidates[(int) (remaining % numCandidates)];
                remaining /= numCandidates;
            }

            for (int i = 0; i < numOutputs; ++i)
            {
                layout.outputBuses.getReference (i) = candidates[(int) (remaining % numCandidates)];
                remaining /= numCandidates;
            }

            // Most effect plugins require the main output bus to be enabled.
            // Don't bother trying layouts that disable bus 0 of either side.
            if (numOutputs > 0 && layout.outputBuses.getReference (0).isDisabled())
                continue;

            if (numInputs > 0 && layout.inputBuses.getReference (0).isDisabled()
                && ! instance.getPluginDescription().isInstrument)
                continue;

            out.push_back (layout);
        }
    }

    static juce::String describeLayout (const juce::AudioProcessor::BusesLayout& layout)
    {
        juce::StringArray ins, outs;

        for (auto& s : layout.inputBuses)
            ins.add (s.getDescription());

        for (auto& s : layout.outputBuses)
            outs.add (s.getDescription());

        return "in [" + ins.joinIntoString (", ") + "] out [" + outs.joinIntoString (", ") + "]";
    }
};

static BusLayoutProcessingTest busLayoutProcessingTest;
