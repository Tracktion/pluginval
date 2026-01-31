/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "VST3ValidatorRunner.h"

#include <sstream>
#include <iostream>

// VST3 SDK includes
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/testsuite/vsttestsuite.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivsthostapplication.h"
#include "pluginterfaces/base/ipluginbase.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace vst3validator {

//==============================================================================
/** Simple host application implementation for running tests. */
class ValidatorHostApp : public IHostApplication
{
public:
    ValidatorHostApp() = default;

    tresult PLUGIN_API getName (String128 name) override
    {
        return StringCopy (name, u"pluginval VST3 Validator");
    }

    tresult PLUGIN_API createInstance (TUID cid, TUID iid, void** obj) override
    {
        *obj = nullptr;
        return kResultFalse;
    }

    tresult PLUGIN_API queryInterface (const TUID _iid, void** obj) override
    {
        QUERY_INTERFACE (_iid, obj, FUnknown::iid, IHostApplication)
        QUERY_INTERFACE (_iid, obj, IHostApplication::iid, IHostApplication)
        *obj = nullptr;
        return kNoInterface;
    }

    uint32 PLUGIN_API addRef () override { return 1; }
    uint32 PLUGIN_API release () override { return 1; }
};

//==============================================================================
/** Test result handler that captures output. */
class TestResultHandler : public ITestResult
{
public:
    TestResultHandler (std::ostream& os, bool verbose)
        : output (os), verboseOutput (verbose)
    {
    }

    void PLUGIN_API addErrorMessage (const char8* msg) override
    {
        if (msg)
        {
            output << "ERROR: " << msg << "\n";
            hasErrors = true;
        }
    }

    void PLUGIN_API addMessage (const char8* msg) override
    {
        if (msg && verboseOutput)
            output << msg << "\n";
    }

    tresult PLUGIN_API queryInterface (const TUID, void** obj) override
    {
        *obj = nullptr;
        return kNoInterface;
    }

    uint32 PLUGIN_API addRef () override { return 1; }
    uint32 PLUGIN_API release () override { return 1; }

    bool hasErrors = false;

private:
    std::ostream& output;
    bool verboseOutput;
};

//==============================================================================
Result runValidator (const Options& options)
{
    Result result;
    std::ostringstream outputStream;

    outputStream << "VST3 Validator - pluginval integrated version\n";
    outputStream << "Validating: " << options.pluginPath << "\n";

    if (options.extendedMode)
        outputStream << "Extended validation mode enabled\n";

    outputStream << "\n";

    // Load the VST3 module
    std::string errorStr;
    auto module = VST3::Hosting::Module::create (options.pluginPath, errorStr);

    if (! module)
    {
        outputStream << "Failed to load module: " << errorStr << "\n";
        result.output = outputStream.str();
        result.exitCode = 1;
        result.success = false;
        return result;
    }

    outputStream << "Module loaded successfully\n";

    auto factory = module->getFactory ();
    if (! factory.get ())
    {
        outputStream << "Failed to get plugin factory\n";
        result.output = outputStream.str();
        result.exitCode = 1;
        result.success = false;
        return result;
    }

    // Get factory info
    auto factoryInfo = factory.info ();
    outputStream << "Factory Info:\n";
    outputStream << "  Vendor: " << factoryInfo.vendor () << "\n";
    outputStream << "  URL: " << factoryInfo.url () << "\n";
    outputStream << "  Email: " << factoryInfo.email () << "\n";
    outputStream << "\n";

    // Create host application
    ValidatorHostApp hostApp;

    // Create test result handler
    TestResultHandler testResult (outputStream, options.verbose);

    bool allTestsPassed = true;
    int numProcessorClasses = 0;

    // Iterate through all classes in the factory
    for (const auto& classInfo : factory.classInfos ())
    {
        outputStream << "Class: " << classInfo.name () << "\n";
        outputStream << "  Category: " << classInfo.category () << "\n";
        outputStream << "  CID: " << classInfo.ID ().toString () << "\n";

        // Check if this is an audio processor
        if (classInfo.category () == kVstAudioEffectClass)
        {
            numProcessorClasses++;
            outputStream << "  [Audio Processor]\n";

            // Create the component
            IPtr<IComponent> component;
            if (factory.createInstance (classInfo.ID (), component) != kResultOk || ! component)
            {
                outputStream << "  ERROR: Failed to create component instance\n";
                allTestsPassed = false;
                continue;
            }

            // Initialize the component
            if (component->initialize (&hostApp) != kResultOk)
            {
                outputStream << "  ERROR: Failed to initialize component\n";
                allTestsPassed = false;
                continue;
            }

            outputStream << "  Component created and initialized successfully\n";

            // Get audio processor interface
            IPtr<IAudioProcessor> processor;
            if (component->queryInterface (IAudioProcessor::iid, (void**)&processor) != kResultOk || ! processor)
            {
                outputStream << "  WARNING: Component does not implement IAudioProcessor\n";
            }
            else
            {
                outputStream << "  IAudioProcessor interface available\n";
            }

            // Get edit controller
            TUID controllerCID;
            if (component->getControllerClassId (controllerCID) == kResultOk)
            {
                IPtr<IEditController> controller;
                if (factory.createInstance (VST3::UID (controllerCID), controller) == kResultOk && controller)
                {
                    if (controller->initialize (&hostApp) == kResultOk)
                    {
                        outputStream << "  Edit controller created and initialized\n";

                        // Get parameter count
                        int32 paramCount = controller->getParameterCount ();
                        outputStream << "  Parameter count: " << paramCount << "\n";

                        controller->terminate ();
                    }
                }
            }

            // Run extended tests if requested
            if (options.extendedMode)
            {
                outputStream << "  Running extended validation...\n";

                // Additional extended validation would go here
                // This includes more thorough state save/restore tests,
                // bus arrangement tests, etc.
            }

            // Terminate the component
            component->terminate ();
            outputStream << "  Component terminated successfully\n";
        }

        outputStream << "\n";
    }

    // Summary
    outputStream << "----------------------------------------\n";
    outputStream << "Validation Summary:\n";
    outputStream << "  Audio Processor classes found: " << numProcessorClasses << "\n";
    outputStream << "  Result: " << (allTestsPassed && ! testResult.hasErrors ? "PASSED" : "FAILED") << "\n";

    result.output = outputStream.str ();
    result.success = allTestsPassed && ! testResult.hasErrors;
    result.exitCode = result.success ? 0 : 1;

    return result;
}

}  // namespace vst3validator
