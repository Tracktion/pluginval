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

class StrictnessInfoDialog;

juce::PropertiesFile& getAppPreferences();

//==============================================================================
struct StatusBar  : public juce::Component,
                    private juce::ChangeListener,
                    private Validator::Listener
{
    StatusBar (Validator& v)
        : validator (v)
    {
        validator.addListener (this);
        validator.addChangeListener (this);
    }

    ~StatusBar() override
    {
        validator.removeListener (this);
        validator.removeChangeListener (this);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        // Draw indicator dot
        auto dotSize = 10.0f;
        auto dotBounds = juce::Rectangle<float> (dotSize, dotSize)
                            .withCentre ({ dotSize, r.getCentreY() });

        g.setColour ([this] {
            switch (state)
            {
                case State::idle:          return juce::Colours::grey;
                case State::validating:    return juce::Colours::orange;
                case State::complete:      return juce::Colours::lightgreen;
                case State::failed:        return juce::Colours::red;
            }
            return juce::Colours::grey;
        }());
        g.fillEllipse (dotBounds);

        // Draw status text
        g.setColour (findColour (juce::Label::textColourId));
        g.setFont (juce::Font (juce::FontOptions (13.0f)));

        auto textBounds = r.withTrimmedLeft (dotSize * 2 + 4);
        {
            const juce::ScopedLock sl (statusTextLock);
            g.drawText (statusText, textBounds, juce::Justification::centredLeft);
        }
    }

private:
    enum class State
    {
        idle,
        validating,
        complete,
        failed
    };

    Validator& validator;
    std::atomic<State> state { State::idle };
    juce::CriticalSection statusTextLock;
    juce::String statusText { "Ready" };

    void updateState (State newState, const juce::String& text)
    {
        state = newState;
        {
            const juce::ScopedLock sl (statusTextLock);
            statusText = text;
        }
        juce::MessageManager::callAsync ([sp = SafePointer<Component> (this)] () mutable { if (sp != nullptr) sp->repaint(); });
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        if (! validator.isConnected() && state == State::validating)
            updateState (State::failed, "Validation crashed");
    }

    void validationStarted (const juce::String& id) override
    {
        updateState (State::validating, "Validating: " + id);
    }

    void logMessage (const juce::String&) override
    {
    }

    void itemComplete (const juce::String&, uint32_t exitCode) override
    {
        if (exitCode == 0)
            updateState (State::complete, "Passed");
        else
            updateState (State::failed, "Failed (exit code " + juce::String (exitCode) + ")");
    }

    void allItemsComplete() override
    {
        if (state != State::failed)
            updateState (State::complete, "All tests complete");
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
/**
    A simple filtered plugin table with search.
*/
class PluginTableComponent : public juce::Component,
                              private juce::ChangeListener,
                              private juce::TableListBoxModel
{
public:
    PluginTableComponent (juce::KnownPluginList& list)
        : knownPluginList (list)
    {
        knownPluginList.addChangeListener (this);

        searchBox.setTextToShowWhenEmpty ("Search plugins...", juce::Colours::grey);
        searchBox.onTextChange = [this] { updateFilter(); };
        addAndMakeVisible (searchBox);

        table.setModel (this);
        table.setHeader (std::make_unique<juce::TableHeaderComponent>());
        table.getHeader().addColumn ("Name", 1, 200, 100, 400);
        table.getHeader().addColumn ("Manufacturer", 2, 150, 80, 300);
        table.getHeader().addColumn ("Format", 3, 80, 60, 120);
        table.getHeader().addColumn ("Category", 4, 100, 60, 200);
        table.getHeader().addColumn ("File/Identifier", 5, 250, 100, 600);
        table.setMultipleSelectionEnabled (true);
        addAndMakeVisible (table);

        updateFilter();
    }

    ~PluginTableComponent() override { knownPluginList.removeChangeListener (this); }

    juce::TableListBox& getTableListBox() { return table; }

    juce::Array<juce::PluginDescription> getSelectedPlugins() const
    {
        juce::Array<juce::PluginDescription> result;
        auto types = knownPluginList.getTypes();

        for (int i = 0; i < table.getSelectedRows().size(); ++i)
        {
            auto idx = table.getSelectedRows()[i];
            if (idx >= 0 && idx < static_cast<int> (filteredIndices.size()))
                result.add (types.getReference (filteredIndices[static_cast<size_t> (idx)]));
        }
        return result;
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (5);
        searchBox.setBounds (r.removeFromTop (26).reduced (2, 0));
        r.removeFromTop (5);
        table.setBounds (r);
    }

private:
    juce::KnownPluginList& knownPluginList;
    juce::TextEditor searchBox;
    juce::TableListBox table;
    std::vector<int> filteredIndices;

    void updateFilter()
    {
        auto searchText = searchBox.getText().toLowerCase();
        auto types = knownPluginList.getTypes();
        filteredIndices.clear();

        for (int i = 0; i < types.size(); ++i)
        {
            auto& desc = types.getReference (i);
            if (searchText.isEmpty()
                || desc.name.toLowerCase().contains (searchText)
                || desc.manufacturerName.toLowerCase().contains (searchText))
                filteredIndices.push_back (i);
        }
        table.updateContent();
        table.repaint();
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { updateFilter(); }

    int getNumRows() override { return static_cast<int> (filteredIndices.size()); }

    void paintRowBackground (juce::Graphics& g, int, int, int, bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll (juce::Colour (0xff4a9eff));
    }

    void paintCell (juce::Graphics& g, int row, int col, int w, int h, bool selected) override
    {
        if (row < 0 || row >= static_cast<int> (filteredIndices.size()))
            return;

        auto trueIndex = filteredIndices[static_cast<size_t> (row)];

        if (trueIndex < 0 || trueIndex >= knownPluginList.getTypes().size())
            return;

        auto desc = knownPluginList.getTypes()[trueIndex];

        g.setColour (selected ? juce::Colours::white : findColour (juce::Label::textColourId));
        g.setFont (14.0f);

        juce::String text;
        if (col == 1) text = desc.name;
        else if (col == 2) text = desc.manufacturerName;
        else if (col == 3) text = desc.pluginFormatName;
        else if (col == 4) text = desc.category;
        else if (col == 5) text = desc.fileOrIdentifier;

        g.drawText (text, 4, 0, w - 8, h, juce::Justification::centredLeft);
    }

    void sortOrderChanged (int col, bool forwards) override
    {
        auto types = knownPluginList.getTypes();
        std::sort (filteredIndices.begin(), filteredIndices.end(),
                   [&types, col, forwards] (int a, int b)
        {
            auto& da = types.getReference (a);
            auto& db = types.getReference (b);
            int r = 0;
            if (col == 1) r = da.name.compareIgnoreCase (db.name);
            else if (col == 2) r = da.manufacturerName.compareIgnoreCase (db.manufacturerName);
            else if (col == 3) r = da.pluginFormatName.compareIgnoreCase (db.pluginFormatName);
            else if (col == 4) r = da.category.compareIgnoreCase (db.category);
            else if (col == 5) r = da.fileOrIdentifier.compareIgnoreCase (db.fileOrIdentifier);
            return forwards ? r < 0 : r > 0;
        });
        table.updateContent();
    }
};

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent   : public juce::Component,
                        public juce::MenuBarModel,
                        private juce::ChangeListener,
                        private Validator::Listener
{
public:
    //==============================================================================
    MainComponent (Validator&);
    ~MainComponent() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    //==============================================================================
    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int menuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

private:
    //==============================================================================
    Validator& validator;

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;

    juce::MenuBarComponent menuBar;
    juce::TabbedComponent tabbedComponent { juce::TabbedButtonBar::TabsAtTop };
    PluginTableComponent pluginTable { knownPluginList };
    juce::PluginListComponent pluginListComponent { formatManager, knownPluginList,
                                              getAppPreferences().getFile().getSiblingFile ("PluginsListDeadMansPedal"),
                                              &getAppPreferences() };
    ConsoleComponent console { validator };
    juce::TextButton testSelectedButton { "Test Selected" }, testAllButton { "Test All" }, testFileButton { "Test File..." },
               strictnessInfoButton { "Strictness" };
    StatusBar statusBar { validator };
    std::unique_ptr<StrictnessInfoDialog> strictnessDialog;

    void savePluginList();
    juce::PopupMenu createFileMenu();
    juce::PopupMenu createTestMenu();
    juce::PopupMenu createLogMenu();
    juce::PopupMenu createOptionsMenu();

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    // Validator::Listener
    void validationStarted (const juce::String& idString) override;
    void logMessage (const juce::String&) override {}
    void itemComplete (const juce::String&, uint32_t) override {}
    void allItemsComplete() override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
