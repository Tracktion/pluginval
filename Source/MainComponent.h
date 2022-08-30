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

#include "juce_gui_extra/juce_gui_extra.h"
#include "Validator.h"
#include "CrashHandler.h"

juce::PropertiesFile& getAppPreferences();

//==============================================================================
struct ConnectionState  : public juce::Component,
                          private juce::ChangeListener,
                          private Validator::Listener
{
    ConnectionState (Validator& v)
        : validator (v)
    {
        validator.addListener (this);
        validator.addChangeListener (this);
    }

    ~ConnectionState() override
    {
        validator.removeListener (this);
        validator.removeChangeListener (this);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        g.setColour ([this] {
            switch (state)
            {
                case State::disconnected:  return juce::Colours::darkred;
                case State::validating:    return juce::Colours::orange;
                case State::connected:
                case State::complete:      return juce::Colours::lightgreen;
            }

            return juce::Colours::darkred;
        }());
        g.fillEllipse (r);

        g.setColour (juce::Colours::darkgrey);
        g.drawEllipse (r.reduced (1.0f), 2.0f);
    }

private:
    enum class State
    {
        disconnected,
        connected,
        validating,
        complete
    };

    Validator& validator;
    std::atomic<State> state { State::disconnected };

    void setState (State newStatus)
    {
        state = newStatus;
        juce::MessageManager::callAsync ([sp = SafePointer<Component> (this)] () mutable { if (sp != nullptr) sp->repaint(); });
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        setState (validator.isConnected() ? State::connected : State::disconnected);
    }

    void validationStarted (const juce::String&) override
    {
        setState (State::validating);
    }

    void logMessage (const juce::String&) override
    {
    }

    void itemComplete (const juce::String&, uint32_t) override
    {
    }

    void allItemsComplete() override
    {
        setState (State::complete);
    }
};

//==============================================================================
struct ConsoleComponent : public juce::Component,
                          private juce::ChangeListener,
                          private juce::AsyncUpdater,
                          private Validator::Listener
{
    ConsoleComponent (Validator& v)
        : validator (v)
    {
        validator.addChangeListener (this);
        validator.addListener (this);

        addAndMakeVisible (editor);
        editor.setReadOnly (true);
        editor.setLineNumbersShown (false);
        editor.setScrollbarThickness (8);
    }

    ~ConsoleComponent() override
    {
        validator.removeChangeListener (this);
        validator.removeListener (this);
    }

    juce::String getLog() const
    {
        return codeDocument.getAllContent();
    }

    void clearLog()
    {
        codeDocument.replaceAllContent (juce::String());
    }

    void resized() override
    {
        auto r = getLocalBounds();
        editor.setBounds (r);
    }

private:
    Validator& validator;

    juce::CodeDocument codeDocument;
    juce::CodeEditorComponent editor { codeDocument, nullptr };
    juce::String currentID;

    juce::CriticalSection logMessagesLock;
    juce::StringArray pendingLogMessages;

    void handleAsyncUpdate() override
    {
        juce::StringArray logMessages;

        {
            const juce::ScopedLock sl (logMessagesLock);
            pendingLogMessages.swapWith (logMessages);
        }

        for (auto&& m : logMessages)
        {
            codeDocument.insertText (editor.getCaretPos(), m);
            editor.scrollToKeepCaretOnScreen();
        }
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        if (! validator.isConnected() && currentID.isNotEmpty())
        {
            logMessage ("\n*** FAILED: VALIDATION CRASHED\n");
            logMessage (getCrashLog());
            currentID = juce::String();
        }
    }

    void validationStarted (const juce::String& id) override
    {
        currentID = id;
        logMessage ("Started validating: " + id + "\n");
    }

    void logMessage (const juce::String& m) override
    {
        {
            const juce::ScopedLock sl (logMessagesLock);
            pendingLogMessages.add (m);
            triggerAsyncUpdate();
        }

        std::cout << m;
    }

    void itemComplete (const juce::String& id, uint32_t exitCode) override
    {
        logMessage ("\nFinished validating: " + id + "\n");

        if (exitCode == 0)
            logMessage ("ALL TESTS PASSED\n");
        else
            logMessage ("*** FAILED WITH EXIT CODE: " + juce::String (exitCode) + "\n");

        currentID = juce::String();
    }

    void allItemsComplete() override
    {
        logMessage ("\nFinished batch validation\n");
    }
};

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent   : public juce::Component,
                        private juce::ChangeListener
{
public:
    //==============================================================================
    MainComponent (Validator&);
    ~MainComponent() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==============================================================================
    Validator& validator;

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;

    juce::TabbedComponent tabbedComponent { juce::TabbedButtonBar::TabsAtTop };
    juce::PluginListComponent pluginListComponent { formatManager, knownPluginList,
                                              getAppPreferences().getFile().getSiblingFile ("PluginsListDeadMansPedal"),
                                              &getAppPreferences() };
    ConsoleComponent console { validator };
    juce::TextButton testSelectedButton { "Test Selected" }, testAllButton { "Test All" }, testFileButton { "Test File" },
               clearButton { "Clear Log" }, saveButton { "Save Log" }, optionsButton { "Options" };
    juce::Slider strictnessSlider;
    juce::Label strictnessLabel { {}, "Strictness Level" };
    ConnectionState connectionStatus { validator };

    void savePluginList();

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
