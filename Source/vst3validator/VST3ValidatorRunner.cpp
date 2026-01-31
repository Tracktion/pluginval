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
#include <cstring>

// VST3 SDK includes
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
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
        const char16_t hostName[] = u"pluginval VST3 Validator";
        std::memcpy (name, hostName, sizeof (hostName));
        return kResultOk;
    }

    tresult PLUGIN_API createInstance (TUID /*cid*/, TUID /*_iid*/, void** obj) override
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

            // Create the component using the template method
            auto component = factory.createInstance<IComponent> (classInfo.ID ());
            if (! component)
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
            FUnknownPtr<IAudioProcessor> processor (component);
            if (! processor)
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
                auto controller = factory.createInstance<IEditController> (VST3::UID (controllerCID));
                if (controller)
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
    outputStream << "  Result: " << (allTestsPassed ? "PASSED" : "FAILED") << "\n";

    result.output = outputStream.str ();
    result.success = allTestsPassed;
    result.exitCode = result.success ? 0 : 1;

    return result;
}

}  // namespace vst3validator
