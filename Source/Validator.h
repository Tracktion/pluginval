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
#include "PluginTests.h"

class ChildProcessValidator;
class AsyncValidator;
class MultiValidator;

//==============================================================================
//==============================================================================
/** Enum to determine the type of validation to run. */
enum class ValidationType
{
    inProcess,      /**< Runs the validation in the calling process. */
    childProcess    /**< Runs the validation in the a separate process. */
};

//==============================================================================
/**
    A single, asynchronous validation pass for a specific plugin.
*/
class ValidationPass
{
public:
    //==============================================================================
    /** Starts a validation process with a set of options and callbacks.
        The validation will be async so either use the validationEnded callback or
        poll hasFinished() to find out when the validation has completed.

        N.B. outputGenerated will be called from a background thread.
    */
    ValidationPass (const juce::String& fileOrIdToValidate, PluginTests::Options, ValidationType,
                    std::function<void (juce::String)> validationStarted,
                    std::function<void (juce::String, uint32_t /*exitCode*/)> validationEnded,
                    std::function<void(const juce::String&)> outputGenerated);

    /** Destructor. */
    ~ValidationPass();

    /** Returns true when the validation pass has ended. */
    [[nodiscard]] bool hasFinished() const;

private:
    //==============================================================================
    std::unique_ptr<ChildProcessValidator> childProcessValidator;
    std::unique_ptr<AsyncValidator> asyncValidator;
};


//==============================================================================
//==============================================================================
/**
    Manages validation calls via a separate process and provides a listener
    interface to find out the results of the validation.
*/
class Validator : public juce::ChangeBroadcaster,
                  private juce::AsyncUpdater
{
public:
    //==============================================================================
    /** Constructor. */
    Validator();

    /** Destructor. */
    ~Validator() override;

    /** Returns true if there is currently an open connection to a validator process. */
    bool isConnected() const;

    /** Validates an array of fileOrIDs. */
    bool validate (const juce::StringArray& fileOrIDsToValidate, PluginTests::Options);

    /** Validates an array of PluginDescriptions. */
    bool validate (const juce::Array<juce::PluginDescription>& pluginsToValidate, PluginTests::Options);

    /** Call this to make validation happen in the same process.
        This can be useful for debugging but should not generally be used as a crashing
        plugin will bring down the app.
    */
    void setValidateInProcess (bool useSameProcess);

    //==============================================================================
    struct Listener
    {
        virtual ~Listener() = default;

        virtual void validationStarted (const juce::String& idString) = 0;
        virtual void logMessage (const juce::String&) = 0;
        virtual void itemComplete (const juce::String& idString, uint32_t exitCode) = 0;
        virtual void allItemsComplete() = 0;
    };

    void addListener (Listener* l)          { listeners.add (l); }
    void removeListener (Listener* l)       { listeners.remove (l); }

private:
    //==============================================================================
    std::unique_ptr<MultiValidator> multiValidator;
    juce::ListenerList<Listener> listeners;
    bool launchInProcess = false;

    void logMessage (const juce::String&);

    void handleAsyncUpdate() override;
};
