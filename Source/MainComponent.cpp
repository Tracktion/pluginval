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

#include <magic_enum/magic_enum.hpp>

#include "PluginTests.h"
#include "StrictnessInfoPopup.h"

//==============================================================================
namespace
{
    void setStrictnessLevel (int newLevel)
    {
        getAppPreferences().setValue ("strictnessLevel", juce::jlimit (1, 10, newLevel));
    }

    int getStrictnessLevel()
    {
        return juce::jlimit (1, 10, getAppPreferences().getIntValue ("strictnessLevel", 5));
    }

    void setRandomSeed (juce::int64 newSeed)
    {
        getAppPreferences().setValue ("randomSeed", newSeed);
    }

    juce::int64 getRandomSeed()
    {
        return getAppPreferences().getIntValue ("randomSeed", 0);
    }

    void setValidateInProcess (bool shouldValidateInProcess)
    {
        getAppPreferences().setValue ("validateInProcess", shouldValidateInProcess);
    }

    bool getValidateInProcess()
    {
        return getAppPreferences().getBoolValue ("validateInProcess", false);
    }

    void setTimeoutMs (juce::int64 newTimeout)
    {
        getAppPreferences().setValue ("timeoutMs", newTimeout);
    }

    juce::int64 getTimeoutMs()
    {
        return getAppPreferences().getIntValue ("timeoutMs", 30000);
    }

    void setVerboseLogging (bool verbose)
    {
        getAppPreferences().setValue ("verbose", verbose);
    }

    bool getVerboseLogging()
    {
        return getAppPreferences().getBoolValue ("verbose", false);
    }

    void setNumRepeats (int numRepeats)
    {
        if (numRepeats >= 1)
            getAppPreferences().setValue ("numRepeats", numRepeats);
    }

    int getNumRepeats()
    {
        return juce::jmax (1, getAppPreferences().getIntValue ("numRepeats", 1));
    }

    void setRandomiseTests (bool shouldRandomiseTests)
    {
        getAppPreferences().setValue ("randomiseTests", shouldRandomiseTests);
    }

    bool getRandomiseTests()
    {
        return getAppPreferences().getBoolValue ("randomiseTests", false);
    }

    juce::File getOutputDir()
    {
        return getAppPreferences().getValue ("outputDir", juce::String());
    }

    std::vector<double> getSampleRates() // from UI no setting of sampleRates yet
    {
        return {44100., 48000., 96000. };
    }

    std::vector<int> getBlockSizes() // from UI no setting of block sizes yet
    {
        return { 64, 128, 256, 512, 1024 };
    }

    void setVST3Validator (juce::File f)
    {
        getAppPreferences().setValue ("vst3validator", f.getFullPathName());
    }

    juce::File getVST3Validator()
    {
        return getAppPreferences().getValue ("vst3validator", juce::String());
    }

    void setRealtimeCheckMode (RealtimeCheck rt)
    {
        getAppPreferences().setValue ("realtimeCheckMode", juce::String (std::string (magic_enum::enum_name (rt))));
    }

    RealtimeCheck getRealtimeCheckMode()
    {
        auto modeString = getAppPreferences().getValue ("realtimeCheckMode", juce::String());
        return magic_enum::enum_cast<RealtimeCheck> (modeString.toStdString())
                .value_or (RealtimeCheck::disabled);
    }

    void setPluginNameFilter (const juce::String& filter)
    {
        getAppPreferences().setValue ("pluginNameFilter", filter);
    }

    juce::String getPluginNameFilter()
    {
        return getAppPreferences().getValue ("pluginNameFilter", "");
    }

    juce::StringArray getPluginNameFilters()
    {
        auto filter = getPluginNameFilter();
        juce::StringArray filters;
        filters.addTokens (filter, ",", "");
        filters.trim();
        filters.removeEmptyStrings();
        return filters;
    }

    void showPluginFilterDialog()
    {
        const juce::String message = TRANS("Enter plugin names to scan for (comma-separated).\nOnly plugins containing these names will be scanned.");
        std::shared_ptr<juce::AlertWindow> aw (juce::LookAndFeel::getDefaultLookAndFeel().createAlertWindow (TRANS("Set Plugin Name Filter"), message,
                                                                                                 TRANS("OK"), TRANS("Clear"), TRANS("Cancel"),
                                                                                                 juce::AlertWindow::QuestionIcon, 3, nullptr));
        aw->addTextEditor ("filter", getPluginNameFilter());
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw] (int res)
                                                                  {
                                                                      if (res == 1)
                                                                      {
                                                                          if (auto te = aw->getTextEditor ("filter"))
                                                                              setPluginNameFilter (te->getText());
                                                                      }
                                                                      else if (res == 2)
                                                                      {
                                                                          setPluginNameFilter ({});
                                                                      }
                                                                  }));
    }

    juce::StringArray getFilteredPluginFiles (juce::AudioPluginFormat& format, const juce::StringArray& nameFilters)
    {
        juce::StringArray result;

        // Get all plugins - for AU this ignores the path and queries system registry
        juce::FileSearchPath searchPaths = format.getDefaultLocationsToSearch();
        auto allPlugins = format.searchPathsForPlugins (searchPaths, true, false);

        for (const auto& pluginId : allPlugins)
        {
            // Get the actual plugin name - for file-based formats this extracts from path,
            // for AU it gets the human-readable name from the identifier
            auto pluginName = format.getNameOfPluginFromIdentifier (pluginId);

            for (const auto& filter : nameFilters)
            {
                if (pluginName.containsIgnoreCase (filter) || pluginId.containsIgnoreCase (filter))
                {
                    result.add (pluginId);
                    break;
                }
            }
        }

        return result;
    }

    PluginTests::Options getTestOptions()
    {
        PluginTests::Options options;
        options.strictnessLevel = getStrictnessLevel();
        options.randomSeed = getRandomSeed();
        options.timeoutMs = getTimeoutMs();
        options.verbose = getVerboseLogging();
        options.numRepeats = getNumRepeats();
        options.randomiseTestOrder = getRandomiseTests();
        options.outputDir = getOutputDir();
        options.sampleRates = getSampleRates();
        options.blockSizes = getBlockSizes();
        options.vst3Validator = getVST3Validator();
        options.realtimeCheck = getRealtimeCheckMode();

        return options;
    }

    //==============================================================================
    void showRandomSeedDialog()
    {
        const juce::String message = TRANS("Set the random seed to use for the tests, useful for replicating issues");
        std::shared_ptr<juce::AlertWindow> aw (juce::LookAndFeel::getDefaultLookAndFeel().createAlertWindow (TRANS("Set Random Seed"), message,
                                                                                                 TRANS("OK"), TRANS("Cancel"), juce::String(),
                                                                                                 juce::AlertWindow::QuestionIcon, 2, nullptr));
        aw->addTextEditor ("randomSeed",juce::String (getRandomSeed()));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw] (int res)
                                                                  {
                                                                      if (res == 1)
                                                                      {
                                                                          if (auto te = aw->getTextEditor ("randomSeed"))
                                                                          {
                                                                              auto seedString = te->getText();
                                                                              setRandomSeed (seedString.startsWith ("0x") ? seedString.getHexValue64()
                                                                                                                          : seedString.getLargeIntValue());
                                                                          }
                                                                      }
                                                                  }));
    }

    void showTimeoutDialog()
    {
        const juce::String message = TRANS("Set the duration in milliseconds after which to kill the validation if there has been no output from it");
        std::shared_ptr<juce::AlertWindow> aw (juce::LookAndFeel::getDefaultLookAndFeel().createAlertWindow (TRANS("Set Timeout (ms)"), message,
                                                                                                 TRANS("OK"), TRANS("Cancel"), juce::String(),
                                                                                                 juce::AlertWindow::QuestionIcon, 2, nullptr));
        aw->addTextEditor ("timeoutMs",juce::String (getTimeoutMs()));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw] (int res)
                                                                  {
                                                                      if (res == 1)
                                                                          if (auto te = aw->getTextEditor ("timeoutMs"))
                                                                              setTimeoutMs (te->getText().getLargeIntValue());
                                                                  }));
    }

    void showNumRepeatsDialog()
    {
        const juce::String message = TRANS("Set the number of times the tests will be repeated");
        std::shared_ptr<juce::AlertWindow> aw (juce::LookAndFeel::getDefaultLookAndFeel().createAlertWindow (TRANS("Set Number of Repeats"), message,
                                                                                                 TRANS("OK"), TRANS("Cancel"), juce::String(),
                                                                                                 juce::AlertWindow::QuestionIcon, 2, nullptr));
        aw->addTextEditor ("repeats",juce::String (getNumRepeats()));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw] (int res)
                                                                  {
                                                                      if (res == 1)
                                                                          if (auto te = aw->getTextEditor ("repeats"))
                                                                              setNumRepeats (te->getText().getIntValue());
                                                                  }));
    }

    void showOutputDirDialog()
    {
        juce::String message = TRANS("Set a desintation directory to place log files");
        auto dir = getOutputDir();

        if (dir.getFullPathName().isNotEmpty())
            message << "\n\n" << dir.getFullPathName().quoted();
        else
            message << "\n\n" << "\"None set\"";

        std::shared_ptr<juce::AlertWindow> aw (juce::LookAndFeel::getDefaultLookAndFeel().createAlertWindow (TRANS("Set Log File Directory"), message,
                                                                                                 TRANS("Choose dir"), TRANS("Don't save logs"), TRANS("Cancel"),
                                                                                                 juce::AlertWindow::QuestionIcon, 3, nullptr));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw] (int res)
                                                                  {
                                                                      if (res == 1)
                                                                          getAppPreferences().setValue ("outputDir", juce::String());

                                                                      if (res == 2)
                                                                      {
                                                                          const auto defaultDir = juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("pluginval logs").getFullPathName();
                                                                          juce::FileChooser fc (TRANS("Directory to save log files"), defaultDir);

                                                                          if (fc.browseForDirectory())
                                                                              getAppPreferences().setValue ("outputDir", fc.getResult().getFullPathName());
                                                                      }
                                                                  }));
    }

    void showVST3ValidatorDialog()
    {
        juce::String message = TRANS("Set the location of the VST3 validator app");
        auto app = getVST3Validator();

        if (app.getFullPathName().isNotEmpty())
            message << "\n\n" << app.getFullPathName().quoted();
        else
            message << "\n\n" << "\"None set\"";

        std::shared_ptr<juce::AlertWindow> aw (juce::LookAndFeel::getDefaultLookAndFeel().createAlertWindow (TRANS("Set VST3 validator"), message,
                                                                                                 TRANS("Choose"), TRANS("Don't use VST3 validator"), TRANS("Cancel"),
                                                                                                 juce::AlertWindow::QuestionIcon, 3, nullptr));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw] (int res)
                                                                  {
                                                                      if (res == 1)
                                                                          setVST3Validator ({});

                                                                      if (res == 1)
                                                                      {
                                                                          juce::FileChooser fc (TRANS("Choose VST3 validator"), {});

                                                                          if (fc.browseForFileToOpen())
                                                                              setVST3Validator (fc.getResult().getFullPathName());
                                                                      }
                                                                  }));
    }
}

//==============================================================================
//==============================================================================
MainComponent::MainComponent (Validator& v)
    : validator (v)
{
    juce::addDefaultFormatsToManager (formatManager);

    menuBar.setModel (this);
    addAndMakeVisible (menuBar);

    const auto tabCol = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    addAndMakeVisible (tabbedComponent);
    tabbedComponent.addTab ("Plugin List", tabCol, &pluginTable, false);
    tabbedComponent.addTab ("Console", tabCol, &console, false);

    addAndMakeVisible (statusBar);
    addAndMakeVisible (testSelectedButton);
    addAndMakeVisible (testAllButton);
    addAndMakeVisible (testFileButton);
    addAndMakeVisible (strictnessInfoButton);

    testSelectedButton.onClick = [this]
        {
            auto plugins = pluginTable.getSelectedPlugins();
            validator.setValidateInProcess (getValidateInProcess());
            validator.validate (plugins, getTestOptions());
        };

    testAllButton.onClick = [this]
        {
            validator.setValidateInProcess (getValidateInProcess());
            validator.validate (knownPluginList.getTypes(), getTestOptions());
        };

    testFileButton.onClick = [this]
        {
            juce::FileChooser fc (TRANS("Browse for Plug-in File"),
                            getAppPreferences().getValue ("lastPluginLocation", juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getFullPathName()),
                            "*.vst;*.vst3;*.dll;*.component");

            if (fc.browseForFileToOpen())
            {
                const auto path = fc.getResult().getFullPathName();
                getAppPreferences().setValue ("lastPluginLocation", path);

                validator.setValidateInProcess (getValidateInProcess());
                validator.validate (path, getTestOptions());
            }
        };

    auto updateStrictnessButtonText = [this]
        {
            strictnessInfoButton.setButtonText ("Strictness: " + juce::String (getStrictnessLevel()));
        };

    updateStrictnessButtonText();

    strictnessInfoButton.onClick = [this, updateStrictnessButtonText]
        {
            strictnessDialog = std::make_unique<StrictnessInfoDialog> (
                getStrictnessLevel(),
                [this, updateStrictnessButtonText] (int newLevel)
                {
                    setStrictnessLevel (newLevel);
                    updateStrictnessButtonText();
                },
                [this] { strictnessDialog.reset(); });
        };

    if (auto xml = std::unique_ptr<juce::XmlElement> (getAppPreferences().getXmlValue ("scannedPlugins")))
        knownPluginList.recreateFromXml (*xml);

    knownPluginList.addChangeListener (this);
    validator.addListener (this);

    setSize (1000, 600);
}

MainComponent::~MainComponent()
{
    validator.removeListener (this);
    menuBar.setModel (nullptr);
    savePluginList();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto r = getLocalBounds();

    menuBar.setBounds (r.removeFromTop (24));
    r.removeFromTop (10);  // Spacing below menu bar / above tabs

    auto bottomR = r.removeFromBottom (48);
    bottomR.reduce (10, 10);  // Indent and add vertical padding

    testSelectedButton.setBounds (bottomR.removeFromLeft (110).reduced (2));
    testAllButton.setBounds (bottomR.removeFromLeft (80).reduced (2));
    testFileButton.setBounds (bottomR.removeFromLeft (90).reduced (2));
    strictnessInfoButton.setBounds (bottomR.removeFromLeft (110).reduced (2));

    statusBar.setBounds (bottomR.reduced (4, 0));

    tabbedComponent.setBounds (r);
}

//==============================================================================
void MainComponent::savePluginList()
{
    if (auto xml = std::unique_ptr<juce::XmlElement> (knownPluginList.createXml()))
        getAppPreferences().setValue ("scannedPlugins", xml.get());
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    savePluginList();
}

void MainComponent::validationStarted (const juce::String&)
{
    tabbedComponent.setCurrentTabIndex (1);  // Switch to Console tab
}

//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Plugins", "Test", "Log", "Options" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int menuIndex, const juce::String&)
{
    juce::PopupMenu menu;

    if (menuIndex == 0)
        menu = createFileMenu();
    else if (menuIndex == 1)
    {
        // Start with JUCE's standard plugin list options
        menu = pluginListComponent.createOptionsMenu();
        menu.addSeparator();

        // Add filter setting
        auto currentFilter = getPluginNameFilter();
        auto filterLabel = currentFilter.isEmpty() ? juce::String ("Set plugin filter...")
                                                   : "Set plugin filter (" + currentFilter + ")...";
        menu.addItem (filterLabel, [] { showPluginFilterDialog(); });

        menu.addSeparator();

        // Add filtered scanning options (disabled if no filter set)
        auto filters = getPluginNameFilters();
        bool hasFilter = ! filters.isEmpty();

        for (auto* format : formatManager.getFormats())
        {
            menu.addItem ("Scan " + format->getName() + " (filtered)", hasFilter, false, [this, format, filters]
            {
                auto files = getFilteredPluginFiles (*format, filters);
                if (files.isEmpty())
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                        "No Matches", "No plugins matching the filter were found.");
                }
                else
                {
                    pluginListComponent.scanFor (*format, files);
                }
            });
        }
    }
    else if (menuIndex == 2)
        menu = createTestMenu();
    else if (menuIndex == 3)
        menu = createLogMenu();
    else if (menuIndex == 4)
        menu = createOptionsMenu();

    return menu;
}

void MainComponent::menuItemSelected (int, int)
{
    // All items use lambdas
}

juce::PopupMenu MainComponent::createFileMenu()
{
    juce::PopupMenu m;
    m.addItem (TRANS("Exit"), [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); });
    return m;
}

juce::PopupMenu MainComponent::createTestMenu()
{
    juce::PopupMenu m;

    m.addItem (TRANS("Test Selected"), [this]
    {
        auto plugins = pluginTable.getSelectedPlugins();
        validator.setValidateInProcess (getValidateInProcess());
        validator.validate (plugins, getTestOptions());
    });

    m.addItem (TRANS("Test All"), [this]
    {
        validator.setValidateInProcess (getValidateInProcess());
        validator.validate (knownPluginList.getTypes(), getTestOptions());
    });

    m.addItem (TRANS("Test File..."), [this]
    {
        juce::FileChooser fc (TRANS("Browse for Plug-in File"),
                        getAppPreferences().getValue ("lastPluginLocation", juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getFullPathName()),
                        "*.vst;*.vst3;*.dll;*.component");

        if (fc.browseForFileToOpen())
        {
            const auto path = fc.getResult().getFullPathName();
            getAppPreferences().setValue ("lastPluginLocation", path);

            validator.setValidateInProcess (getValidateInProcess());
            validator.validate (path, getTestOptions());
        }
    });

    return m;
}

juce::PopupMenu MainComponent::createLogMenu()
{
    juce::PopupMenu m;

    m.addItem (TRANS("Clear Log"), [this] { console.clearLog(); });

    m.addItem (TRANS("Save Log..."), [this]
    {
        juce::FileChooser fc (TRANS("Save Log File"),
                        getAppPreferences().getValue ("lastSaveLocation", juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getFullPathName()),
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
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, TRANS("Unable to Save"),
                                                  TRANS("Unable to save to the file at location: XYYX").replace ("XYYX", f.getFullPathName()));
            }
        }
    });

    m.addSeparator();
    m.addItem (TRANS("Verbose logging"), true, getVerboseLogging(),
               [] { setVerboseLogging (! getVerboseLogging()); });

    m.addItem (TRANS("Choose log file directory..."),
               [] { showOutputDirDialog(); });

    return m;
}

juce::PopupMenu MainComponent::createOptionsMenu()
{
    juce::PopupMenu m;

    {
        juce::PopupMenu rtCheckMenu;

        for (auto currentMode = getRealtimeCheckMode();
             auto mode : magic_enum::enum_values<RealtimeCheck>())
        {
            rtCheckMenu.addItem (getDisplayString (mode), true, mode == currentMode,
                                 [newMode = mode] { setRealtimeCheckMode (newMode); });
        }

        m.addSubMenu (TRANS("Realtime check mode"), rtCheckMenu);
    }

    m.addItem (TRANS("Validate in process"), true, getValidateInProcess(),
               [this] { setValidateInProcess (! getValidateInProcess()); validator.setValidateInProcess (getValidateInProcess()); });

    m.addItem (TRANS("Set random seed (123)").replace ("123", "0x" + juce::String::toHexString (getRandomSeed()) + "/" + juce::String (getRandomSeed())),
               [] { showRandomSeedDialog(); });

    m.addItem (TRANS("Set timeout (123ms)").replace ("123", juce::String (getTimeoutMs())),
               [] { showTimeoutDialog(); });

    m.addItem (TRANS("Num repeats (123)").replace ("123", juce::String (getNumRepeats())),
               [] { showNumRepeatsDialog(); });

    m.addItem (TRANS("Randomise tests"), true, getRandomiseTests(),
               [] { setRandomiseTests (! getRandomiseTests()); });

    m.addItem (TRANS("Set VST3 validator location..."),
               [] { showVST3ValidatorDialog(); });

    m.addSeparator();

    m.addItem (TRANS("Show settings folder"),
               [] { getAppPreferences().getFile().revealToUser(); });

    return m;
}
