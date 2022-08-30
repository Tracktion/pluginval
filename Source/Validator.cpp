/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "Validator.h"
#include "PluginTests.h"
#include "CrashHandler.h"
#include "CommandLine.h"
#include <numeric>
#include <thread>

//==============================================================================
struct PluginsUnitTestRunner    : public juce::UnitTestRunner,
                               private juce::Thread
{
    PluginsUnitTestRunner (std::function<void (const juce::String&)> logCallback, std::unique_ptr<juce::FileOutputStream> logDestination, juce::int64 timeoutInMs)
        : Thread ("TimoutThread"),
          callback (std::move (logCallback)),
          outputStream (std::move (logDestination)),
          timeoutMs (timeoutInMs)
    {
        jassert (callback);
        resetTimeout();

        if (timeoutInMs > 0)
            startThread (1);
    }

    ~PluginsUnitTestRunner() override
    {
        stopThread (5000);
    }

    juce::FileOutputStream* getOutputFileStream() const
    {
        return outputStream.get();
    }

    void resultsUpdated() override
    {
        resetTimeout();
    }

    void logMessage (const juce::String& message) override
    {
        if (! canSendLogMessage)
            return;

        resetTimeout();

        if (message.isNotEmpty())
        {
            if (outputStream)
                *outputStream << message << "\n";

            callback (message + "\n");
        }
    }

private:
    std::function<void (const juce::String&)> callback;
    std::unique_ptr<juce::FileOutputStream> outputStream;
    const juce::int64 timeoutMs = -1;
    std::atomic<juce::int64> timoutTime { -1 };
    std::atomic<bool> canSendLogMessage { true };

    void resetTimeout()
    {
        timoutTime = (juce::Time::getCurrentTime() + juce::RelativeTime::milliseconds (timeoutMs)).toMilliseconds();
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            if (juce::Time::getCurrentTime().toMilliseconds() > timoutTime)
            {
                logMessage ("*** FAILED: Timeout after " + juce::RelativeTime::milliseconds (timeoutMs).getDescription());
                canSendLogMessage = false;
                outputStream.reset();

                // Give the log a second to flush the message before terminating
                juce::Thread::sleep (1000);
                juce::Process::terminate();
            }

            juce::Thread::sleep (200);
        }
    }
};

//==============================================================================
//==============================================================================
static juce::String getFileNameFromDescription (PluginTests& test)
{
    auto getBaseName = [&]() -> juce::String
    {
        if (auto pd = test.getDescriptions().getFirst())
            return pd->manufacturerName + " - " + pd->name + " " + pd->version + " - " + juce::SystemStats::getOperatingSystemName() + " " + pd->pluginFormatName;

        const auto fileOrID = test.getFileOrID();

        if (fileOrID.isNotEmpty())
        {
            if (juce::File::isAbsolutePath (fileOrID))
                return juce::File (fileOrID).getFileName();
        }

        return "pluginval Log";
    };

    return getBaseName() + "_" + juce::Time::getCurrentTime().toString (true, true).replace (":", ",") + ".txt";
}

static juce::File getDestinationFile (PluginTests& test)
{
    const auto dir = test.getOptions().outputDir;

    if (dir == juce::File())
        return {};

    if (dir.existsAsFile() || ! dir.createDirectory())
    {
        jassertfalse;
        return {};
    }

    return dir.getChildFile (getFileNameFromDescription (test));
}

static std::unique_ptr<juce::FileOutputStream> createDestinationFileStream (PluginTests& test)
{
    auto file = getDestinationFile (test);

    if (file == juce::File())
        return {};

    std::unique_ptr<juce::FileOutputStream> fos (file.createOutputStream());

    if (fos && fos->openedOk())
        return fos;

    jassertfalse;
    return {};
}

/** Renames a file based upon its description.
    This is useful if a path or AU ID was passed in as the plugin info isn't known at that point.
*/
static void updateFileNameIfPossible (PluginTests& test, PluginsUnitTestRunner& runner)
{
    if (auto os = runner.getOutputFileStream())
    {
        const auto sourceFile = os->getFile();
        const auto destName = getFileNameFromDescription (test);

        if (destName.isEmpty() || sourceFile.getFileName() == destName)
            return;

        const bool success = sourceFile.moveFileTo (sourceFile.getParentDirectory().getChildFile (destName));
        juce::ignoreUnused (success);
        jassert (success);
    }
}


//==============================================================================
//==============================================================================
inline juce::Array<juce::UnitTestRunner::TestResult> runTests (PluginTests& test, std::function<void (const juce::String&)> callback)
{
    const auto options = test.getOptions();
    juce::Array<juce::UnitTestRunner::TestResult> results;
    PluginsUnitTestRunner testRunner (std::move (callback), createDestinationFileStream (test), options.timeoutMs);
    testRunner.setAssertOnFailure (false);

    juce::Array<juce::UnitTest*> testsToRun;
    testsToRun.add (&test);
    testRunner.runTests (testsToRun, options.randomSeed);

    for (int i = 0; i < testRunner.getNumResults(); ++i)
        results.add (*testRunner.getResult (i));

    updateFileNameIfPossible (test, testRunner);

    return results;
}

inline juce::Array<juce::UnitTestRunner::TestResult> validate (const juce::String& fileOrIDToValidate, PluginTests::Options options, std::function<void (const juce::String&)> callback)
{
    PluginTests test (fileOrIDToValidate, options);
    return runTests (test, std::move (callback));
}

inline int getNumFailures (juce::Array<juce::UnitTestRunner::TestResult> results)
{
    return std::accumulate (results.begin(), results.end(), 0,
                            [] (int count, const juce::UnitTestRunner::TestResult& r) { return count + r.failures; });
}


//==============================================================================
//==============================================================================
class ChildProcessValidator
{
public:
    //==============================================================================
    ChildProcessValidator (const juce::String& fileOrID_, PluginTests::Options options_,
                           std::function<void (juce::String)> validationStarted_,
                           std::function<void (juce::String, uint32_t /*exitCode*/)> validationEnded_,
                           std::function<void (const juce::String&)> outputGenerated_)
        : fileOrID (fileOrID_),
          options (options_),
          validationStarted (std::move (validationStarted_)),
          validationEnded (std::move (validationEnded_)),
          outputGenerated (std::move (outputGenerated_))
    {
        thread = std::thread ([this] { run(); });
    }

    ~ChildProcessValidator()
    {
        thread.join();
    }

    bool hasFinished() const
    {
        return ! isRunning;
    }

private:
    //==============================================================================
    JUCE_DECLARE_WEAK_REFERENCEABLE (ChildProcessValidator)

    const juce::String fileOrID;
    PluginTests::Options options;

    juce::ChildProcess childProcess;
    std::thread thread;
    std::atomic<bool> isRunning { true };

    std::function<void (juce::String)> validationStarted;
    std::function<void (juce::String, uint32_t)> validationEnded;
    std::function<void(const juce::String&)> outputGenerated;

    //==============================================================================
    void run()
    {
        isRunning = childProcess.start (createCommandLine (fileOrID, options));

        if (! isRunning)
            return;

        juce::MessageManager::callAsync ([this, wr = juce::WeakReference<ChildProcessValidator> (this)]
                                         {
                                             if (wr != nullptr && validationStarted)
                                                 validationStarted (fileOrID);
                                         });


        // Flush the output from the process
        for (;;)
        {
            for (;;)
            {
                char buffer[2048];

                if (const auto numBytesRead = childProcess.readProcessOutput (buffer, sizeof (buffer));
                    numBytesRead > 0)
                {
                    std::string msg (buffer, (size_t) numBytesRead);

                    if (outputGenerated)
                        outputGenerated (msg);
                }
                else
                {
                    break;
                }
            }

            if (! childProcess.isRunning())
                break;

            using namespace std::literals;
            std::this_thread::sleep_for (100ms);
        }

        juce::MessageManager::callAsync ([this, wr = juce::WeakReference<ChildProcessValidator> (this), exitCode = childProcess.getExitCode()]
                                         {
                                             if (wr != nullptr)
                                             {
                                                 if (validationEnded)
                                                     validationEnded (fileOrID, exitCode);

                                                 isRunning = false;
                                             }
                                         });
    }
};


//==============================================================================
//==============================================================================
class AsyncValidator
{
public:
    //==============================================================================
    AsyncValidator (const juce::String& fileOrID_, PluginTests::Options options_,
                    std::function<void (juce::String)> validationStarted_,
                    std::function<void (juce::String, uint32_t /*exitCode*/)> validationEnded_,
                    std::function<void (const juce::String&)> outputGenerated_)
        : fileOrID (fileOrID_),
          options (options_),
          validationStarted (std::move (validationStarted_)),
          validationEnded (std::move (validationEnded_)),
          outputGenerated (std::move (outputGenerated_))
    {
        thread = std::thread ([this] { run(); });
    }

    ~AsyncValidator()
    {
        thread.join();
    }

    bool hasFinished() const
    {
        return ! isRunning;
    }

private:
    //==============================================================================
    JUCE_DECLARE_WEAK_REFERENCEABLE (AsyncValidator)

    const juce::String fileOrID;
    PluginTests::Options options;

    std::thread thread;
    std::atomic<bool> isRunning { true };

    std::function<void (juce::String)> validationStarted;
    std::function<void (juce::String, uint32_t)> validationEnded;
    std::function<void (const juce::String&)> outputGenerated;

    //==============================================================================
    void run()
    {
        juce::MessageManager::callAsync ([this, wr = juce::WeakReference<AsyncValidator> (this)]
                                         {
                                             if (wr != nullptr && validationStarted)
                                                 validationStarted (fileOrID);
                                         });

        const auto numFailues = getNumFailures (validate (fileOrID, options, outputGenerated));

        juce::MessageManager::callAsync ([this, wr = juce::WeakReference<AsyncValidator> (this), numFailues]
                                         {
                                             if (wr != nullptr)
                                             {
                                                 if (validationEnded)
                                                     validationEnded (fileOrID, numFailues > 0 ? 1 : 0);

                                                 isRunning = false;
                                             }
                                         });
    }
};


//==============================================================================
//==============================================================================
ValidationPass::ValidationPass (const juce::String& fileOrIdToValidate, PluginTests::Options opts, ValidationType vt,
                                std::function<void (juce::String)> validationStarted,
                                std::function<void (juce::String, uint32_t)> validationEnded,
                                std::function<void(const juce::String&)> outputGenerated)
{
    if (vt == ValidationType::inProcess)
    {
        asyncValidator = std::make_unique<AsyncValidator> (fileOrIdToValidate, opts,
                                                           std::move (validationStarted),
                                                           std::move (validationEnded),
                                                           std::move (outputGenerated));
    }
    else if (vt == ValidationType::childProcess)
    {
        childProcessValidator = std::make_unique<ChildProcessValidator> (fileOrIdToValidate, opts,
                                                                         std::move (validationStarted),
                                                                         std::move (validationEnded),
                                                                         std::move (outputGenerated));
    }
}

ValidationPass::~ValidationPass()
{
}

bool ValidationPass::hasFinished() const
{
    return asyncValidator ? asyncValidator->hasFinished()
                          : childProcessValidator->hasFinished();
}


//==============================================================================
//==============================================================================
class MultiValidator    : public juce::Timer
{
public:
    MultiValidator (juce::StringArray fileOrIDsToValidate,
                    PluginTests::Options options_,
                    ValidationType validationType_,
                    std::function<void (juce::String)> validationStarted_,
                    std::function<void (juce::String, uint32_t /*exitCode*/)> validationEnded_,
                    std::function<void(const juce::String&)> outputGenerated_,
                    std::function<void()> allCompleteCallback_)
        : pluginsToValidate (std::move (fileOrIDsToValidate)),
          options (std::move (options_)),
          validationType (validationType_),
          validationStarted (std::move (validationStarted_)),
          validationEnded (std::move (validationEnded_)),
          outputGenerated (std::move (outputGenerated_)),
          completeCallback (std::move (allCompleteCallback_))
    {
        startTimerHz (20);
        timerCallback();
    }

private:
    juce::StringArray pluginsToValidate;
    const PluginTests::Options options;
    const ValidationType validationType;

    std::unique_ptr<ValidationPass> validationPass;

    std::function<void (juce::String)> validationStarted;
    std::function<void (juce::String, uint32_t /*exitCode*/)> validationEnded;
    std::function<void(const juce::String&)> outputGenerated;
    std::function<void()> completeCallback;

    //==============================================================================
    bool isRunning() const
    {
        return validationPass ? ! validationPass->hasFinished()
                              : false;
    }

    //==============================================================================
    void timerCallback() override
    {
        if (pluginsToValidate.isEmpty())
        {
            if (isRunning())
                return;

            validationPass.reset();

            if (completeCallback)
                completeCallback();

            stopTimer();
            return;
        }

        if (isRunning())
            return;

        validationPass = std::make_unique<ValidationPass> (pluginsToValidate[0], options, validationType,
                                                           validationStarted,
                                                           validationEnded,
                                                           outputGenerated);
        pluginsToValidate.remove (0);
    }
};

//==============================================================================
Validator::Validator() {}
Validator::~Validator() {}

bool Validator::isConnected() const
{
    return multiValidator != nullptr;
}

bool Validator::validate (const juce::StringArray& fileOrIDsToValidate, PluginTests::Options options)
{
    sendChangeMessage();
    multiValidator = std::make_unique<MultiValidator> (fileOrIDsToValidate, options, launchInProcess ? ValidationType::inProcess : ValidationType::childProcess,
                                                       [this] (juce::String id) { listeners.call (&Listener::validationStarted, id); },
                                                       [this] (juce::String id, uint32_t exitCode) { listeners.call (&Listener::itemComplete, id, exitCode); },
                                                       [this] (const juce::String& m) { listeners.call (&Listener::logMessage, m); },
                                                       [this] { listeners.call (&Listener::allItemsComplete); triggerAsyncUpdate(); });
    return true;
}

bool Validator::validate (const juce::Array<juce::PluginDescription>& pluginsToValidate, PluginTests::Options options)
{
    juce::StringArray fileOrIDsToValidate;

    for (auto pd : pluginsToValidate)
        fileOrIDsToValidate.add (pd.fileOrIdentifier);

    return validate (fileOrIDsToValidate, options);
}

void Validator::setValidateInProcess (bool useSameProcess)
{
    launchInProcess = useSameProcess;
}

//==============================================================================
void Validator::handleAsyncUpdate()
{
    multiValidator.reset();
    sendChangeMessage();
}
