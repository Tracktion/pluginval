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
                                                                                                 TRANS("Choose dir"), TRANS("Cancel"), TRANS("Don't save logs"),
                                                                                                 juce::AlertWindow::QuestionIcon, 3, nullptr));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw] (int res)
                                                                  {
                                                                      if (res == 3)
                                                                          getAppPreferences().setValue ("outputDir", juce::String());

                                                                      if (res == 1)
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
                                                                                                 TRANS("Choose"), TRANS("Cancel"), TRANS("Don't use VST3 validator"),
                                                                                                 juce::AlertWindow::QuestionIcon, 3, nullptr));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw] (int res)
                                                                  {
                                                                      if (res == 3)
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
    formatManager.addDefaultFormats();

    const auto tabCol = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    addAndMakeVisible (tabbedComponent);
    tabbedComponent.addTab ("Plugin List", tabCol, &pluginListComponent, false);
    tabbedComponent.addTab ("Console", tabCol, &console, false);

    addAndMakeVisible (connectionStatus);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (optionsButton);
    addAndMakeVisible (testSelectedButton);
    addAndMakeVisible (testAllButton);
    addAndMakeVisible (testFileButton);
    addAndMakeVisible (strictnessLabel);
    addAndMakeVisible (strictnessSlider);

    testSelectedButton.onClick = [this]
        {
            auto rows = pluginListComponent.getTableListBox().getSelectedRows();
            juce::Array<juce::PluginDescription> plugins;

            for (int i = 0; i < rows.size(); ++i)
                plugins.add (knownPluginList.getTypes()[rows[i]]);

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

    clearButton.onClick = [this]
        {
            console.clearLog();
        };

    saveButton.onClick = [this]
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
        };

    optionsButton.onClick = [this, sp = SafePointer<MainComponent> (this)]
        {
            enum MenuItem
            {
                validateInProcess = 1,
                showRandomSeed,
                showTimeout,
                verboseLogging,
                numRepeats,
                randomise,
                chooseOutputDir,
                showVST3Validator,
                showSettingsDir
            };

            juce::PopupMenu m;
            m.addItem (validateInProcess, TRANS("Validate in process"), true, getValidateInProcess());
            m.addItem (showRandomSeed, TRANS("Set random seed (123)").replace ("123", "0x" + juce::String::toHexString (getRandomSeed()) + "/" + juce::String (getRandomSeed())));
            m.addItem (showTimeout, TRANS("Set timeout (123ms)").replace ("123",juce::String (getTimeoutMs())));
            m.addItem (verboseLogging, TRANS("Verbose logging"), true, getVerboseLogging());
            m.addItem (numRepeats, TRANS("Num repeats (123)").replace ("123",juce::String (getNumRepeats())));
            m.addItem (randomise, TRANS("Randomise tests"), true, getRandomiseTests());
            m.addItem (chooseOutputDir, TRANS("Choose a location for log files"));
            m.addItem (showVST3Validator, TRANS("Set the location of the VST3 validator"));
            m.addSeparator();
            m.addItem (showSettingsDir, TRANS("Show settings folder"));
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&optionsButton),
                             [sp] (int res) mutable
                             {
                                 if (res == validateInProcess)
                                 {
                                     setValidateInProcess (! getValidateInProcess());
                                     sp->validator.setValidateInProcess (getValidateInProcess());
                                 }
                                 else if (res == showRandomSeed)
                                 {
                                     showRandomSeedDialog();
                                 }
                                 else if (res == showTimeout)
                                 {
                                     showTimeoutDialog();
                                 }
                                 else if (res == verboseLogging)
                                 {
                                     setVerboseLogging (! getVerboseLogging());
                                 }
                                 else if (res == numRepeats)
                                 {
                                     showNumRepeatsDialog();
                                 }
                                 else if (res == randomise)
                                 {
                                     setRandomiseTests (! getRandomiseTests());
                                 }
                                 else if (res == chooseOutputDir)
                                 {
                                     showOutputDirDialog();
                                 }
                                 else if (res == showVST3Validator)
                                 {
                                     showVST3ValidatorDialog();
                                 }
                                 else if (res == showSettingsDir)
                                 {
                                     getAppPreferences().getFile().revealToUser();
                                 }
                             });
        };

    strictnessSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 35, 24);
    strictnessSlider.setSliderStyle (juce::Slider::IncDecButtons);
    strictnessSlider.setRange ({ 1.0, 10.0 }, 1.0);
    strictnessSlider.setNumDecimalPlacesToDisplay (0);
    strictnessSlider.setValue (getStrictnessLevel());
    strictnessSlider.onValueChange = [this]
        {
            setStrictnessLevel (juce::roundToInt (strictnessSlider.getValue()));
        };

    if (auto xml = std::unique_ptr<juce::XmlElement> (getAppPreferences().getXmlValue ("scannedPlugins")))
        knownPluginList.recreateFromXml (*xml);

    knownPluginList.addChangeListener (this);

    setSize (800, 600);
}

MainComponent::~MainComponent()
{
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

    auto bottomR = r.removeFromBottom (28);
    saveButton.setBounds (bottomR.removeFromRight (80).reduced (2));
    clearButton.setBounds (bottomR.removeFromRight (80).reduced (2));
    optionsButton.setBounds (bottomR.removeFromRight (70).reduced (2));

    connectionStatus.setBounds (bottomR.removeFromLeft (bottomR.getHeight()).reduced (2));
    testSelectedButton.setBounds (bottomR.removeFromLeft (110).reduced (2));
    testAllButton.setBounds (bottomR.removeFromLeft (110).reduced (2));
    testFileButton.setBounds (bottomR.removeFromLeft (110).reduced (2));

    strictnessLabel.setBounds (bottomR.removeFromLeft (110).reduced (2));
    strictnessSlider.setBounds (bottomR.removeFromLeft (100).reduced (2));

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
