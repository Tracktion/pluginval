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

#include <exception>

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

/** Returns a random normalised value to set on a parameter.

    Below strictness level 8, two-step (on/off) parameters are only sent 0 or 1.
    Any value is valid, but some plugins keep the values in between rather than
    mapping them to a step, which the state restoration tests then pick up.
*/
static inline float getRandomParameterValue (juce::Random& r, const juce::AudioProcessorParameter& parameter, int strictnessLevel)
{
    // N.B. hosted VST3 parameters don't report isBoolean so also check the number of steps
    const bool isTwoStep = parameter.isBoolean() || parameter.getNumSteps() == 2;

    if (isTwoStep && strictnessLevel < 8)
        return r.nextBool() ? 1.0f : 0.0f;

    return r.nextFloat();
}

//==============================================================================
template<typename UnaryFunction>
void iterateAudioBuffer (juce::AudioBuffer<float>& ab, UnaryFunction fn)
{
    auto sampleData = ab.getArrayOfWritePointers();

    for (int c = ab.getNumChannels(); --c >= 0;)
        for (int s = ab.getNumSamples(); --s >= 0;)
            fn (sampleData[c][s]);
}

static inline void fillNoise (juce::AudioBuffer<float>& ab) noexcept
{
    juce::Random r;
    juce::ScopedNoDenormals noDenormals;

    auto sampleData = ab.getArrayOfWritePointers();

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
/** Turns a child process's output, read in arbitrary chunks, into text. A UTF-8
    character split across two reads is held back until the rest arrives, rather
    than producing an invalid string.
*/
struct ChildProcessOutputDecoder
{
    /** Adds the bytes read and returns the complete text so far. */
    std::string decode (const char* data, size_t numBytes)
    {
        pending.append (data, numBytes);

        const auto numComplete = getNumCompleteBytes (pending);
        auto text = pending.substr (0, numComplete);
        pending.erase (0, numComplete);
        return text;
    }

    /** The number of leading bytes that don't end in a partial UTF-8 character. */
    static size_t getNumCompleteBytes (const std::string& bytes)
    {
        // A character is at most 4 bytes, so only its lead byte in the last 4 matters
        for (size_t i = bytes.size(), numChecked = 0; i > 0 && numChecked < 4; --i, ++numChecked)
        {
            const auto byte = (unsigned char) bytes[i - 1];

            if ((byte & 0xc0) == 0x80)
                continue; // a continuation byte

            const size_t length = byte < 0x80           ? 1
                                : (byte & 0xe0) == 0xc0 ? 2
                                : (byte & 0xf0) == 0xe0 ? 3
                                : (byte & 0xf8) == 0xf0 ? 4
                                                        : 1; // invalid, so pass it on

            return i - 1 + length <= bytes.size() ? bytes.size() : i - 1;
        }

        return bytes.size();
    }

private:
    std::string pending;
};

//==============================================================================
/** Runs fn on the message thread (directly if already on it) and waits for it to
    finish. An exception thrown by fn is rethrown here, so the caller is always
    released rather than left waiting forever.
*/
template <typename Fn>
void callOnMessageThreadAndWait (Fn&& fn)
{
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        fn();
        return;
    }

    juce::WaitableEvent waiter;
    std::exception_ptr error;

    juce::MessageManager::callAsync ([&]
                                     {
                                         try
                                         {
                                             fn();
                                         }
                                         catch (...)
                                         {
                                             error = std::current_exception();
                                         }

                                         waiter.signal();
                                     });
    waiter.wait();

    if (error)
        std::rethrow_exception (error);
}

/** Calls fn on the message thread if the plugin is a VST3, otherwise on this thread. */
template <typename Fn>
void callOnMessageThreadIfVST3 (juce::AudioPluginInstance& instance, Fn&& fn)
{
    if (instance.getPluginDescription().pluginFormatName != "VST3")
        fn();
    else
        callOnMessageThreadAndWait (fn);
}

//==============================================================================
static std::unique_ptr<juce::AudioProcessorEditor> createAndShowEditorOnMessageThread (juce::AudioPluginInstance& instance)
{
    std::unique_ptr<juce::AudioProcessorEditor> editor;

    callOnMessageThreadAndWait ([&]
    {
        if (! instance.hasEditor())
            return;

        jassert (instance.getActiveEditor() == nullptr);
        editor.reset (instance.createEditorAndMakeActive());

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
    });

    return editor;
}

static void deleteEditorOnMessageThread (std::unique_ptr<juce::AudioProcessorEditor> editor)
{
    callOnMessageThreadAndWait ([&]
    {
        if (editor != nullptr)
            editor->processor.editorBeingDeleted (editor.get());

        editor.reset();
    });
}

//==============================================================================
//==============================================================================
inline void callPrepareToPlayOnMessageThreadIfVST3 (juce::AudioPluginInstance& instance,
                                                    double sampleRate, int blockSize)
{
    callOnMessageThreadIfVST3 (instance, [&] { instance.prepareToPlay (sampleRate, blockSize); });
}

inline void callReleaseResourcesOnMessageThreadIfVST3 (juce::AudioPluginInstance& instance)
{
    callOnMessageThreadIfVST3 (instance, [&] { instance.releaseResources(); });
}

inline juce::MemoryBlock callGetStateInformationOnMessageThreadIfVST3 (juce::AudioPluginInstance& instance)
{
    juce::MemoryBlock state;
    callOnMessageThreadIfVST3 (instance, [&] { instance.getStateInformation (state); });
    return state;
}

inline void callSetStateInformationOnMessageThreadIfVST3 (juce::AudioPluginInstance& instance, const juce::MemoryBlock& state)
{
    callOnMessageThreadIfVST3 (instance, [&] { instance.setStateInformation (state.getData(), (int) state.getSize()); });
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
