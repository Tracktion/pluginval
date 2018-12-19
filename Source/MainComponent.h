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

#include "../JuceLibraryCode/JuceHeader.h"
#include "Validator.h"
#include "CrashHandler.h"

PropertiesFile& getAppPreferences();

//==============================================================================
struct ConnectionStatus : public Component,
                          private ChangeListener,
                          private Validator::Listener
{
    ConnectionStatus (Validator& v)
        : validator (v)
    {
        validator.addListener (this);
        validator.addChangeListener (this);
    }

    ~ConnectionStatus()
    {
        validator.removeListener (this);
        validator.removeChangeListener (this);
    }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        g.setColour ([this] {
            switch (status)
            {
                case Status::disconnected:  return Colours::darkred;
                case Status::validating:    return Colours::orange;
                case Status::connected:
                case Status::complete:      return Colours::lightgreen;
            }

			return Colours::darkred;
        }());
        g.fillEllipse (r);

        g.setColour (Colours::darkgrey);
        g.drawEllipse (r.reduced (1.0f), 2.0f);
    }

private:
    enum class Status
    {
        disconnected,
        connected,
        validating,
        complete
    };

    Validator& validator;
    std::atomic<Status> status { Status::disconnected };

    void setStatus (Status newStatus)
    {
        status = newStatus;
        MessageManager::callAsync ([sp = SafePointer<Component> (this)] () mutable { if (sp != nullptr) sp->repaint(); });
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        setStatus (validator.isConnected() ? Status::connected : Status::disconnected);
    }

    void validationStarted (const String&) override
    {
        setStatus (Status::validating);
    }

    void logMessage (const String&) override
    {
    }

    void itemComplete (const String&, int) override
    {
    }

    void allItemsComplete() override
    {
        setStatus (Status::complete);
    }
};

//==============================================================================
struct ConsoleComponent : public Component,
                          private ChangeListener,
                          private AsyncUpdater,
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

    ~ConsoleComponent()
    {
        validator.removeChangeListener (this);
        validator.removeListener (this);
    }

    String getLog() const
    {
        return codeDocument.getAllContent();
    }

    void clearLog()
    {
        codeDocument.replaceAllContent (String());
    }

    void resized() override
    {
        auto r = getLocalBounds();
        editor.setBounds (r);
    }

private:
    Validator& validator;

    CodeDocument codeDocument;
    CodeEditorComponent editor { codeDocument, nullptr };
    String currentID;

    CriticalSection logMessagesLock;
    StringArray pendingLogMessages;

    void handleAsyncUpdate() override
    {
        StringArray logMessages;

        {
            const ScopedLock sl (logMessagesLock);
            pendingLogMessages.swapWith (logMessages);
        }

        for (auto&& m : logMessages)
        {
            codeDocument.insertText (editor.getCaretPos(), m + "\n");
            editor.scrollToKeepCaretOnScreen();
        }
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        if (! validator.isConnected() && currentID.isNotEmpty())
        {
            logMessage ("\n*** FAILED: VALIDATION CRASHED");
            logMessage (getCrashLog());
            currentID = String();
        }
    }

    void validationStarted (const String& id) override
    {
        currentID = id;
        logMessage ("Started validating: " + id);
    }

    void logMessage (const String& m) override
    {
        {
            const ScopedLock sl (logMessagesLock);
            pendingLogMessages.add (m);
            triggerAsyncUpdate();
        }

        std::cout << m << "\n";
    }

    void itemComplete (const String& id, int numFailures) override
    {
        logMessage ("\nFinished validating: " + id);

        if (numFailures == 0)
            logMessage ("ALL TESTS PASSED");
        else
            logMessage ("*** FAILED: " + String (numFailures) + " TESTS");

        currentID = String();
    }

    void allItemsComplete() override
    {
    }
};

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent   : public Component,
                        private ChangeListener
{
public:
    //==============================================================================
    MainComponent (Validator&);
    ~MainComponent();

    //==============================================================================
    void paint (Graphics&) override;
    void resized() override;

private:
    //==============================================================================
    Validator& validator;

    AudioPluginFormatManager formatManager;
    KnownPluginList knownPluginList;

    TabbedComponent tabbedComponent { TabbedButtonBar::TabsAtTop };
    PluginListComponent pluginListComponent { formatManager, knownPluginList,
                                              getAppPreferences().getFile().getSiblingFile ("PluginsListDeadMansPedal"),
                                              &getAppPreferences() };
    ConsoleComponent console { validator };
    TextButton testSelectedButton { "Test Selected" }, testAllButton { "Test All" }, testFileButton { "Test File" },
               clearButton { "Clear Log" }, saveButton { "Save Log" }, optionsButton { "Options" };
    Slider strictnessSlider;
    Label strictnessLabel { {}, "Strictness Level" };
    ConnectionStatus connectionStatus { validator };

    void savePluginList();

    void changeListenerCallback (ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
