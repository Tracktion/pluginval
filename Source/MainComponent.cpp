/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "MainComponent.h"
#include "PluginTests.h"

//==============================================================================
MainComponent::MainComponent (Validator& v)
    : validator (v)
{
    formatManager.addDefaultFormats();

    const auto tabCol = getLookAndFeel().findColour (ResizableWindow::backgroundColourId);
    addAndMakeVisible (tabbedComponent);
    tabbedComponent.addTab ("Plugin List", tabCol, &pluginListComponent, false);
    tabbedComponent.addTab ("Console", tabCol, &console, false);

    addAndMakeVisible (connectionStatus);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (testSelectedButton);
    addAndMakeVisible (testAllButton);

    testSelectedButton.onClick = [this]
        {
            auto rows = pluginListComponent.getTableListBox().getSelectedRows();
            Array<PluginDescription*> plugins;

            for (int i = 0; i < rows.size(); ++i)
                if (auto pd = knownPluginList.getType (rows[i]))
                    plugins.add (pd);

            validator.validate (plugins, 10);
        };

    testAllButton.onClick = [this]
        {
            Array<PluginDescription*> plugins;

            for (int i = 0; i < knownPluginList.getNumTypes(); ++i)
                if (auto pd = knownPluginList.getType (i))
                    plugins.add (pd);

            validator.validate (plugins, 10);
        };

    clearButton.onClick = [this]
        {
            console.clearLog();
        };

    saveButton.onClick = [this]
        {
            FileChooser fc (TRANS("Save Log File"),
                            getAppPreferences().getValue ("lastSaveLocation", File::getSpecialLocation (File::userDesktopDirectory).getFullPathName()),
                            "*.txt");

            if (fc.browseForFileToSave (true))
            {
                const auto f = fc.getResult();

                if (f.replaceWithText (console.getLog()))
                {
                    getAppPreferences().setValue ("lastSaveLocation", f.getFullPathName());
                }
                else
                {
                    AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, TRANS("Unable to Save"),
                                                      TRANS("Unable to save to the file at location: XYYX").replace ("XYYX", f.getFullPathName()));
                }
            }
        };

    if (auto xml = std::unique_ptr<XmlElement> (getAppPreferences().getXmlValue ("scannedPlugins")))
        knownPluginList.recreateFromXml (*xml);

    knownPluginList.addChangeListener (this);

    setSize (800, 600);
}

MainComponent::~MainComponent()
{
    savePluginList();
}

//==============================================================================
void MainComponent::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto r = getLocalBounds();

    auto bottomR = r.removeFromBottom (28);
    saveButton.setBounds (bottomR.removeFromRight (100).reduced (2));
    clearButton.setBounds (bottomR.removeFromRight (100).reduced (2));

    connectionStatus.setBounds (bottomR.removeFromLeft (bottomR.getHeight()).reduced (2));
    testSelectedButton.setBounds (bottomR.removeFromLeft (130).reduced (2));
    testAllButton.setBounds (bottomR.removeFromLeft (130).reduced (2));

    tabbedComponent.setBounds (r);
}

//==============================================================================
void MainComponent::savePluginList()
{
    if (auto xml = std::unique_ptr<XmlElement> (knownPluginList.createXml()))
        getAppPreferences().setValue ("scannedPlugins", xml.get());
}

void MainComponent::changeListenerCallback (ChangeBroadcaster*)
{
    savePluginList();
}
