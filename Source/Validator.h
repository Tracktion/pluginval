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

#include <thread>
#include <JuceHeader.h>
#include "PluginTests.h"

class GroupValidator;

//==============================================================================
/**
    Manages validation calls via a separate process and provides a listener
    interface to find out the results of the validation.
*/
class Validator : public ChangeBroadcaster,
                  private AsyncUpdater
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
    bool validate (const StringArray& fileOrIDsToValidate, PluginTests::Options);

    /** Validates an array of PluginDescriptions. */
    bool validate (const Array<PluginDescription>& pluginsToValidate, PluginTests::Options);

    /** Call this to make validation happen in the same process.
        This can be useful for debugging but should not generally be used as a crashing
        plugin will bring down the app.
    */
    void setValidateInProcess (bool useSameProcess);

    //==============================================================================
    struct Listener
    {
        virtual ~Listener() = default;

        virtual void validationStarted (const String& idString) = 0;
        virtual void logMessage (const String&) = 0;
        virtual void itemComplete (const String& idString, uint32_t exitCode) = 0;
        virtual void allItemsComplete() = 0;
    };

    void addListener (Listener* l)          { listeners.add (l); }
    void removeListener (Listener* l)       { listeners.remove (l); }

private:
    //==============================================================================
    std::unique_ptr<GroupValidator> groupValidator;
    ListenerList<Listener> listeners;
    bool launchInProcess = false;

    void logMessage (const String&);

    void handleAsyncUpdate() override;
};


//==============================================================================
//==============================================================================
class ChildProcessValidator
{
public:
    ChildProcessValidator (const juce::String& fileOrIdToValidate, PluginTests::Options,
                           std::function<void (juce::String)> validationStarted,
                           std::function<void (juce::String, uint32_t /*exitCode*/)> validationEnded,
                           std::function<void(const String&)> outputGenerated);
    ~ChildProcessValidator();

    bool hasFinished() const;

private:
    const juce::String fileOrID;
    PluginTests::Options options;

    ChildProcess childProcess;
    std::thread thread;
    std::atomic<bool> isRunning { true };

    std::function<void (juce::String)> validationStarted;
    std::function<void (juce::String, uint32_t)> validationEnded;
    std::function<void(const String&)> outputGenerated;

    void run();
};

//==============================================================================
class AsyncValidator
{
public:
    AsyncValidator (const juce::String& fileOrIdToValidate, PluginTests::Options,
                    std::function<void (juce::String)> validationStarted,
                    std::function<void (juce::String, uint32_t /*exitCode*/)> validationEnded,
                    std::function<void (const String&)> outputGenerated);
    ~AsyncValidator();

    bool hasFinished() const;

private:
    const juce::String fileOrID;
    PluginTests::Options options;

    std::thread thread;
    std::atomic<bool> isRunning { true };

    std::function<void (juce::String)> validationStarted;
    std::function<void (juce::String, uint32_t)> validationEnded;
    std::function<void (const String&)> outputGenerated;

    void run();
};
