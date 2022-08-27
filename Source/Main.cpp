/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"
#include "Validator.h"
#include "CommandLine.h"

//==============================================================================
class PluginValidatorApplication  : public juce::JUCEApplication,
                                   private juce::AsyncUpdater
{
public:
    //==============================================================================
    PluginValidatorApplication() = default;

    juce::PropertiesFile& getAppPreferences()
    {
        jassert (propertiesFile); // Calling this from the child process?
        return *propertiesFile;
    }

    //==============================================================================
    const juce::String getApplicationName() override       { return "pluginval"; }
    const juce::String getApplicationVersion() override    { return VERSION; }
    bool moreThanOneInstanceAllowed() override       { return true; }

    //==============================================================================
    void initialise (const juce::String& commandLine) override
    {
        if (shouldPerformCommandLine (commandLine))
        {
            triggerAsyncUpdate();
            return;
        }

       #if JUCE_DEBUG
        juce::UnitTestRunner testRunner;
        testRunner.runTestsInCategory ("pluginval");
       #endif

        validator = std::make_unique<Validator>();
        propertiesFile.reset (getPropertiesFile());
        mainWindow = std::make_unique<MainWindow> (*validator, getApplicationName() + " v" + getApplicationVersion());
    }

    void shutdown() override
    {
        mainWindow.reset();
        validator.reset();
        juce::Logger::setCurrentLogger (nullptr);
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override
    {
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
    }

    //==============================================================================
    /*
        This class implements the desktop window that contains an instance of
        our MainComponent class.
    */
    class MainWindow    : public juce::DocumentWindow
    {
    public:
        MainWindow (Validator& v, juce::String name)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                .findColour (ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent (v), true);

            setResizable (true, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            // This is called when the user tries to close this window. Here, we'll just
            // ask the app to quit when this happens, but you can change this to do
            // whatever you need.
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

        /* Note: Be careful if you override any DocumentWindow methods - the base
           class uses a lot of them, so by overriding you might break its functionality.
           It's best to do all your work in your content component instead, but if
           you really have to override any DocumentWindow methods, make sure your
           subclass also calls the superclass's method.
        */

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<Validator> validator;
    std::unique_ptr<juce::PropertiesFile> propertiesFile;
    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<juce::FileLogger> fileLogger;
    std::unique_ptr<CommandLineValidator> commandLineValidator;

    static juce::PropertiesFile::Options getPropertiesFileOptions()
    {
        juce::PropertiesFile::Options opts;
        opts.millisecondsBeforeSaving = 2000;
        opts.storageFormat = juce::PropertiesFile::storeAsXML;

        opts.applicationName = juce::String ("pluginval");
        opts.filenameSuffix = ".xml";
        opts.folderName = opts.applicationName;
        opts.osxLibrarySubFolder = "Application Support";

        // Move old settings if possible
        if (! opts.getDefaultFile().exists())
        {
            const auto newFile = opts.getDefaultFile();
            auto oldOpts = opts;
            oldOpts.applicationName = oldOpts.folderName = "PluginValidator";
            const auto oldFile = oldOpts.getDefaultFile();

            if (oldFile.existsAsFile())
            {
                oldFile.getParentDirectory().copyDirectoryTo (newFile.getParentDirectory());
                newFile.getParentDirectory().getChildFile (oldFile.getFileName()).moveFileTo (newFile);
                oldFile.getParentDirectory().deleteRecursively();
            }
        }

        return opts;
    }

    static juce::PropertiesFile* getPropertiesFile()
    {
        auto opts = getPropertiesFileOptions();
        return new juce::PropertiesFile (opts.getDefaultFile(), opts);
    }

    void handleAsyncUpdate() override
    {
        commandLineValidator = std::make_unique<CommandLineValidator>();
        performCommandLine (*commandLineValidator, juce::JUCEApplication::getCommandLineParameters());
    }
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (PluginValidatorApplication)

juce::PropertiesFile& getAppPreferences()
{
    auto app = dynamic_cast<PluginValidatorApplication*> (PluginValidatorApplication::getInstance());
    return app->getAppPreferences();
}
