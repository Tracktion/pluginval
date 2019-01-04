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

#include <JuceHeader.h>

//==============================================================================
struct StopwatchTimer
{
    StopwatchTimer()
    {
        reset();
    }

    void reset()
    {
        startTime = Time::getMillisecondCounter();
    }

    String getDescription() const
    {
        const auto relTime = RelativeTime::milliseconds (static_cast<int> (Time::getMillisecondCounter() - startTime));
        return relTime.getDescription();
    }

private:
    uint32 startTime;
};


//==============================================================================
/** Returns the set of automatable parameters excluding the bypass parameter. */
static inline Array<AudioProcessorParameter*> getNonBypassAutomatableParameters (AudioPluginInstance& instance)
{
    Array<AudioProcessorParameter*> parameters;

    for (auto p : instance.getParameters())
        if (p->isAutomatable() && p != instance.getBypassParameter())
            parameters.add (p);

    return parameters;
}

//==============================================================================
template<typename UnaryFunction>
void iterateAudioBuffer (AudioBuffer<float>& ab, UnaryFunction fn)
{
    float** sampleData = ab.getArrayOfWritePointers();

    for (int c = ab.getNumChannels(); --c >= 0;)
        for (int s = ab.getNumSamples(); --s >= 0;)
            fn (sampleData[c][s]);
}

static inline void fillNoise (AudioBuffer<float>& ab) noexcept
{
    Random r;
    ScopedNoDenormals noDenormals;

    float** sampleData = ab.getArrayOfWritePointers();

    for (int c = ab.getNumChannels(); --c >= 0;)
        for (int s = ab.getNumSamples(); --s >= 0;)
            sampleData[c][s] = r.nextFloat() * 2.0f - 1.0f;
}

static inline int countNaNs (AudioBuffer<float>& ab) noexcept
{
    int count = 0;
    iterateAudioBuffer (ab, [&count] (float s)
                            {
                                if (std::isnan (s))
                                    ++count;
                            });

    return count;
}

static inline int countInfs (AudioBuffer<float>& ab) noexcept
{
    int count = 0;
    iterateAudioBuffer (ab, [&count] (float s)
    {
        if (std::isinf (s))
            ++count;
    });

    return count;
}

static inline int countSubnormals (AudioBuffer<float>& ab) noexcept
{
    int count = 0;
    iterateAudioBuffer (ab, [&count] (float s)
    {
        if (s != 0.0f && std::fpclassify (s) == FP_SUBNORMAL)
            ++count;
    });

    return count;
}

static inline void addNoteOn (MidiBuffer& mb, int channel, int noteNumber, int sample)
{
    mb.addEvent (juce::MidiMessage::noteOn (channel, noteNumber, 0.5f), sample);
}

static inline void addNoteOff (MidiBuffer& mb, int channel, int noteNumber, int sample)
{
    mb.addEvent (juce::MidiMessage::noteOff (channel, noteNumber, 0.5f), sample);
}

static inline float getParametersSum (AudioPluginInstance& instance)
{
    float value = 0.0f;

    for (auto parameter : getNonBypassAutomatableParameters (instance))
        value += parameter->getValue();

    return value;
}


//==============================================================================
//==============================================================================
static std::unique_ptr<AudioProcessorEditor> createAndShowEditorOnMessageThread (AudioPluginInstance& instance)
{
    std::unique_ptr<AudioProcessorEditor> editor;

    if (MessageManager::getInstance()->isThisTheMessageThread())
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
            MessageManager::getInstance()->runDispatchLoopUntil (200);
        }
    }
    else
    {
        WaitableEvent waiter;
        MessageManager::callAsync ([&]
                                   {
                                       editor = createAndShowEditorOnMessageThread (instance);
                                       waiter.signal();
                                   });
        waiter.wait();
    }

    return editor;
}

static void deleteEditorOnMessageThread (std::unique_ptr<AudioProcessorEditor> editor)
{
    if (MessageManager::getInstance()->isThisTheMessageThread())
    {
        editor.reset();
        return;
    }

    WaitableEvent waiter;
    MessageManager::callAsync ([&]
                               {
                                   editor.reset();
                                   waiter.signal();
                               });
    waiter.wait();
}

//==============================================================================
/** Creates an editor for the plugin on the message thread, shows it on screen and
    deletes it on the message thread upon destruction.
*/
struct ScopedEditorShower
{
    ScopedEditorShower (AudioPluginInstance& instance)
        : editor (createAndShowEditorOnMessageThread (instance))
    {
    }

    ~ScopedEditorShower()
    {
        deleteEditorOnMessageThread (std::move (editor));
    }

    std::unique_ptr<AudioProcessorEditor> editor;
};


//==============================================================================
//==============================================================================
struct ScopedPluginDeinitialiser
{
    ScopedPluginDeinitialiser (AudioProcessor& ap)
        : processor (ap), sampleRate (ap.getSampleRate()), blockSize (ap.getBlockSize())
    {
        processor.releaseResources();
    }

    ~ScopedPluginDeinitialiser()
    {
        if (blockSize != 0 && sampleRate != 0.0)
            processor.prepareToPlay (sampleRate, blockSize);
    }

    AudioProcessor& processor;
    const double sampleRate;
    const int blockSize;
};


//==============================================================================
//==============================================================================
struct ScopedBusesLayout
{
    ScopedBusesLayout (AudioProcessor& ap)
        : processor (ap), currentLayout (ap.getBusesLayout())
    {
    }

    ~ScopedBusesLayout()
    {
        processor.setBusesLayout (currentLayout);
    }

    AudioProcessor& processor;
    AudioProcessor::BusesLayout currentLayout;
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
