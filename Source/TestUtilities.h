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

#include "juce_audio_processors/juce_audio_processors.h"

//==============================================================================
struct StopwatchTimer
{
    StopwatchTimer()
    {
        reset();
    }

    void reset()
    {
        startTime = juce::Time::getMillisecondCounter();
    }

    juce::String getDescription() const
    {
        const auto relTime = juce::RelativeTime::milliseconds (static_cast<int> (juce::Time::getMillisecondCounter() - startTime));
        return relTime.getDescription();
    }

private:
    juce::uint32 startTime;
};


//==============================================================================
/** Returns the set of automatable parameters excluding the bypass parameter. */
static inline juce::Array<juce::AudioProcessorParameter*> getNonBypassAutomatableParameters (juce::AudioPluginInstance& instance)
{
    juce::Array<juce::AudioProcessorParameter*> parameters;

    for (auto p : instance.getParameters())
        if (p->isAutomatable() && p != instance.getBypassParameter())
            parameters.add (p);

    return parameters;
}

//==============================================================================
template<typename UnaryFunction>
void iterateAudioBuffer (juce::AudioBuffer<float>& ab, UnaryFunction fn)
{
    float** sampleData = ab.getArrayOfWritePointers();

    for (int c = ab.getNumChannels(); --c >= 0;)
        for (int s = ab.getNumSamples(); --s >= 0;)
            fn (sampleData[c][s]);
}

static inline void fillNoise (juce::AudioBuffer<float>& ab) noexcept
{
    juce::Random r;
    juce::ScopedNoDenormals noDenormals;

    float** sampleData = ab.getArrayOfWritePointers();

    for (int c = ab.getNumChannels(); --c >= 0;)
        for (int s = ab.getNumSamples(); --s >= 0;)
            sampleData[c][s] = r.nextFloat() * 2.0f - 1.0f;
}

static inline int countNaNs (juce::AudioBuffer<float>& ab) noexcept
{
    int count = 0;
    iterateAudioBuffer (ab, [&count] (float s)
                            {
                                if (std::isnan (s))
                                    ++count;
                            });

    return count;
}

static inline int countInfs (juce::AudioBuffer<float>& ab) noexcept
{
    int count = 0;
    iterateAudioBuffer (ab, [&count] (float s)
    {
        if (std::isinf (s))
            ++count;
    });

    return count;
}

static inline int countSubnormals (juce::AudioBuffer<float>& ab) noexcept
{
    int count = 0;
    iterateAudioBuffer (ab, [&count] (float s)
    {
        if (s != 0.0f && std::fpclassify (s) == FP_SUBNORMAL)
            ++count;
    });

    return count;
}

static inline void addNoteOn (juce::MidiBuffer& mb, int channel, int noteNumber, int sample)
{
    mb.addEvent (juce::MidiMessage::noteOn (channel, noteNumber, 0.5f), sample);
}

static inline void addNoteOff (juce::MidiBuffer& mb, int channel, int noteNumber, int sample)
{
    mb.addEvent (juce::MidiMessage::noteOff (channel, noteNumber, 0.5f), sample);
}

static inline float getParametersSum (juce::AudioPluginInstance& instance)
{
    float value = 0.0f;

    for (auto parameter : getNonBypassAutomatableParameters (instance))
        value += parameter->getValue();

    return value;
}


//==============================================================================
//==============================================================================
static std::unique_ptr<juce::AudioProcessorEditor> createAndShowEditorOnMessageThread (juce::AudioPluginInstance& instance)
{
    std::unique_ptr<juce::AudioProcessorEditor> editor;

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        if (! instance.hasEditor())
            return {};

        jassert (instance.getActiveEditor() == nullptr);
        editor.reset (instance.createEditor());

        if (editor)
        {
            editor->addToDesktop (0);
            editor->setVisible (true);

            // Pump the message loop for a couple of seconds for the window to initialise itself
            // For some reason this blocks on Linux though so we'll avoid doing it
           #if ! JUCE_LINUX
             juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
           #endif
        }
    }
    else
    {
        juce::WaitableEvent waiter;
        juce::MessageManager::callAsync ([&]
                                   {
                                       editor = createAndShowEditorOnMessageThread (instance);
                                       waiter.signal();
                                   });
        waiter.wait();
    }

    return editor;
}

static void deleteEditorOnMessageThread (std::unique_ptr<juce::AudioProcessorEditor> editor)
{
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        editor.reset();
        return;
    }

    juce::WaitableEvent waiter;
    juce::MessageManager::callAsync ([&]
                               {
                                   editor.reset();
                                   waiter.signal();
                               });
    waiter.wait();
}

//==============================================================================
//==============================================================================
inline void callPrepareToPlayOnMessageThreadIfVST3 (juce::AudioPluginInstance& instance,
                                                    double sampleRate, int blockSize)
{
    if (instance.getPluginDescription().pluginFormatName != "VST3"
        || juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        instance.prepareToPlay (sampleRate, blockSize);
        return;
    }

    juce::WaitableEvent waiter;
    juce::MessageManager::callAsync ([&]
                               {
                                   instance.prepareToPlay (sampleRate, blockSize);
                                   waiter.signal();
                               });
    waiter.wait();
}

inline void callReleaseResourcesOnMessageThreadIfVST3 (juce::AudioPluginInstance& instance)
{
    if (instance.getPluginDescription().pluginFormatName != "VST3"
        || juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        instance.releaseResources();
        return;
    }

    juce::WaitableEvent waiter;
    juce::MessageManager::callAsync ([&]
                               {
                                   instance.releaseResources();
                                   waiter.signal();
                               });
    waiter.wait();
}

inline juce::MemoryBlock callGetStateInformationOnMessageThreadIfVST3 (juce::AudioPluginInstance& instance)
{
    juce::MemoryBlock state;

    if (instance.getPluginDescription().pluginFormatName != "VST3"
        || juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        instance.getStateInformation (state);
    }
    else
    {
        juce::WaitableEvent waiter;
        juce::MessageManager::callAsync ([&]
                                   {
                                       instance.getStateInformation (state);
                                       waiter.signal();
                                   });
        waiter.wait();
    }

    return state;
}

inline void callSetStateInformationOnMessageThreadIfVST3 (juce::AudioPluginInstance& instance, const juce::MemoryBlock& state)
{
    if (instance.getPluginDescription().pluginFormatName != "VST3"
        || juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        instance.setStateInformation (state.getData(), (int) state.getSize());
    }
    else
    {
        juce::WaitableEvent waiter;
        juce::MessageManager::callAsync ([&]
                                   {
                                       instance.setStateInformation (state.getData(), (int) state.getSize());
                                       waiter.signal();
                                   });
        waiter.wait();
    }
}

//==============================================================================
/** Creates an editor for the plugin on the message thread, shows it on screen and
    deletes it on the message thread upon destruction.
*/
struct ScopedEditorShower
{
    ScopedEditorShower (juce::AudioPluginInstance& instance)
        : editor (createAndShowEditorOnMessageThread (instance))
    {
    }

    ~ScopedEditorShower()
    {
        deleteEditorOnMessageThread (std::move (editor));
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor;
};


//==============================================================================
//==============================================================================
struct ScopedPluginDeinitialiser
{
    ScopedPluginDeinitialiser (juce::AudioPluginInstance& ap)
        : instance (ap), sampleRate (ap.getSampleRate()), blockSize (ap.getBlockSize())
    {
        callReleaseResourcesOnMessageThreadIfVST3 (instance);
    }

    ~ScopedPluginDeinitialiser()
    {
        if (blockSize != 0 && sampleRate != 0.0)
            callPrepareToPlayOnMessageThreadIfVST3 (instance, sampleRate, blockSize);
    }

    juce::AudioPluginInstance& instance;
    const double sampleRate;
    const int blockSize;
};


//==============================================================================
//==============================================================================
struct ScopedBusesLayout
{
    ScopedBusesLayout (juce::AudioProcessor& ap)
        : processor (ap), currentLayout (ap.getBusesLayout())
    {
    }

    ~ScopedBusesLayout()
    {
        processor.setBusesLayout (currentLayout);
    }

    juce::AudioProcessor& processor;
    juce::AudioProcessor::BusesLayout currentLayout;
};


//==============================================================================
/**
    Used to enable intercepting of allocations using new, new[], delete and
    delete[] on the calling thread.
*/
struct AllocatorInterceptor
{
    AllocatorInterceptor() = default;

    void disableAllocations()
    {
        allocationsAllowed.store (false);
    }

    void enableAllocations()
    {
        allocationsAllowed.store (true);
    }

    bool isAllowedToAllocate() const
    {
        return allocationsAllowed.load();
    }

    //==============================================================================
    void logAllocationViolation()
    {
        violationOccured.store (true);
        ++numAllocationViolations;
    }

    int getNumAllocationViolations() const noexcept
    {
        return numAllocationViolations;
    }

    bool getAndClearAllocationViolation() noexcept
    {
        return violationOccured.exchange (false);
    }

    int getAndClearNumAllocationViolations() noexcept
    {
        return numAllocationViolations.exchange (0);
    }

    //==============================================================================
    enum class ViolationBehaviour
    {
        none,
        logToCerr,
        throwException
    };

    static void setViolationBehaviour (ViolationBehaviour) noexcept;
    static ViolationBehaviour getViolationBehaviour() noexcept;

private:
    std::atomic<bool> allocationsAllowed { true };
    std::atomic<int> numAllocationViolations { 0 };
    std::atomic<bool> violationOccured { false };
    static std::atomic<ViolationBehaviour> violationBehaviour;
};

//==============================================================================
/** Returns an AllocatorInterceptor for the current thread. */
AllocatorInterceptor& getAllocatorInterceptor();

//==============================================================================
/**
    Helper class to log allocations on the current thread.
    Put one on stack at the point for which you want to detect allocations.
*/
struct ScopedAllocationDisabler
{
    /** Disables allocations on the current thread. */
    ScopedAllocationDisabler();

    /** Re-enables allocations on the current thread. */
    ~ScopedAllocationDisabler();
};
