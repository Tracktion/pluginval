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

#include <JuceHeader.h>
#include "PluginTestResult.h"
#include "Validator.h"
#include "JUnitListener.h"

struct CommandLineValidator : private ChangeListener,
                              private Validator::Listener
{
    CommandLineValidator();
    ~CommandLineValidator() override;

    void validate (const StringArray& fileOrIDs, PluginTests::Options, bool validateInProcess);

private:
    Validator validator;
    JUnitListener junitListener;
    String currentID;
    std::atomic<int> numFailures { 0 };

    void changeListenerCallback (ChangeBroadcaster*) override;

    void validationStarted (const String&) override;
    void logMessage (const String& m) override;
    void itemComplete (const String&, int numItemFailures, const PluginTestResultArray&) override;
    void allItemsComplete() override;
    void connectionLost() override;
};

//==============================================================================
void performCommandLine (CommandLineValidator&, const String& commandLine);
bool shouldPerformCommandLine (const String& commandLine);

enum { commandLineNotPerformed = 0x72346231 };
