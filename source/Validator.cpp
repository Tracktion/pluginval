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
#include "ReportWriter.h"
#include <numeric>
#include <thread>
#include <chrono>
#include <mutex>

//==============================================================================
struct PluginsUnitTestRunner    : public juce::UnitTestRunner,
                                  private juce::Thread
{
    PluginsUnitTestRunner (std::function<void (const juce::String&)> logCallback, std::unique_ptr<juce::FileOutputStream> logDestination, juce::int64 timeoutInMs,
                           std::function<void()> timeoutCallback = {})
        : Thread ("TimoutThread"),
          callback (std::move (logCallback)),
          onTimeout (std::move (timeoutCallback)),
          outputStream (std::move (logDestination)),
          timeoutMs (timeoutInMs)
    {
        jassert (callback);
        resetTimeout();

        // Initialise the crash handler to clear any previous crash logs
        initialiseCrashHandler();

        if (timeoutInMs > 0)
            startThread (juce::Thread::Priority::low);
    }

    ~PluginsUnitTestRunner() override
    {
        stopThread (5000);
    }

    juce::FileOutputStream* getOutputFileStream() const
    {
        return outputStream.get();
    }

    /** Stops the timeout thread. Call once the tests have finished so a timeout
        can't fire (and write its report) while the results are being written.
        If the timeout has already fired, this blocks until the process ends.
    */
    void stopTimeoutThread()
    {
        stopThread (5000);
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
            {
                // The timeout thread can reset the stream at any point
                const std::scoped_lock sl (outputStreamLock);

                if (outputStream)
                    *outputStream << message << "\n";
            }

            callback (message + "\n");
        }
    }

private:
    std::function<void (const juce::String&)> callback;
    std::function<void()> onTimeout; // Called on the timeout thread just before the process is terminated
    std::unique_ptr<juce::FileOutputStream> outputStream;
    std::mutex outputStreamLock;
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

                {
                    const std::scoped_lock sl (outputStreamLock);
                    outputStream.reset();
                }

                // Give the log a second to flush the message before terminating
                juce::Thread::sleep (1000);

                if (onTimeout)
                    onTimeout();

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

    return getBaseName() + "_" + juce::Time::getCurrentTime().toISO8601 (false).replace (":", ",") + ".txt";
}

static juce::File getDestinationFile (PluginTests& test)
{
    const auto dir = test.getOptions().outputDir;
    const auto filename = test.getOptions().outputFilename;

    if (dir == juce::File())
    {
        if (juce::String() == filename)
            return {};
        return juce::File(filename);
    }

    if (dir.existsAsFile() || ! dir.createDirectory())
    {
        jassertfalse;
        return {};
    }

    if (juce::String() == filename)
        return dir.getChildFile (getFileNameFromDescription (test));
    return dir.getChildFile (filename);
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
        const auto filename = test.getOptions().outputFilename;
        const auto destName = (filename == juce::String())?getFileNameFromDescription (test) : filename;

        if (destName.isEmpty() || sourceFile.getFileName() == destName)
            return;

        const bool success = sourceFile.moveFileTo (sourceFile.getParentDirectory().getChildFile (destName));
        juce::ignoreUnused (success);
        jassert (success);
    }
}


//==============================================================================
/** Converts the results of a validation into a report for --junit-xml / --json-results. */
static report_writer::Report toReport (PluginTests& test, const juce::Array<juce::UnitTestRunner::TestResult>& results)
{
    const auto options = test.getOptions();

    report_writer::Report report;
    report.command = "validate";
    report.target = test.getFileOrID();

    if (! results.isEmpty())
        report.startTime = results.getReference (0).startTime;

    if (auto pd = test.getDescriptions().getFirst())
    {
        report.properties.push_back ({ "plugin_name", pd->name });
        report.properties.push_back ({ "plugin_version", pd->version });
        report.properties.push_back ({ "plugin_format", pd->pluginFormatName });
        report.properties.push_back ({ "plugin_manufacturer", pd->manufacturerName });
    }

    report.properties.push_back ({ "strictness_level", juce::String (options.strictnessLevel) });
    report.properties.push_back ({ "timeout_ms", juce::String (options.timeoutMs) });

    if (options.randomSeed != 0)
        report.properties.push_back ({ "random_seed", "0x" + juce::String::toHexString (options.randomSeed) });

    for (const auto& r : results)
    {
        report_writer::TestCase c;
        c.name = r.subcategoryName;
        c.status = r.failures > 0 ? report_writer::TestCase::Outcome::failed
                                  : report_writer::TestCase::Outcome::passed;
        c.messages = r.messages;
        c.durationSeconds = juce::jmax (0.0, (r.endTime - r.startTime).inSeconds());
        report.cases.push_back (c);
    }

    return report;
}

//==============================================================================
//==============================================================================
inline int getNumFailures (juce::Array<juce::UnitTestRunner::TestResult> results);

inline juce::Array<juce::UnitTestRunner::TestResult> runTests (PluginTests& test, std::function<void (const juce::String&)> callback)
{
    const auto options = test.getOptions();
    juce::Array<juce::UnitTestRunner::TestResult> results;
    const auto writeIncompleteReport = [options, fileOrID = test.getFileOrID()] (const juce::String& reason)
    {
        report_writer::write (report_writer::makeIncomplete ("validate", fileOrID, reason),
                              options.junitXmlFile, options.jsonResultsFile);
    };

    // Written by the timeout thread just before it ends the process. The thread
    // is stopped before the final report is written, so the two never overlap.
    PluginsUnitTestRunner testRunner (std::move (callback), createDestinationFileStream (test), options.timeoutMs,
                                      [options, writeIncompleteReport]
                                      {
                                          writeIncompleteReport ("Validation timed out after " + juce::RelativeTime::milliseconds (options.timeoutMs).getDescription());
                                      });
    testRunner.setAssertOnFailure (false);

    // Written up front so a crash can't leave a stale (or missing) report behind
    writeIncompleteReport ("Validation did not complete (crashed)");

    juce::Array<juce::UnitTest*> testsToRun;
    testsToRun.add (&test);
    testRunner.runTests (testsToRun, options.randomSeed);
    testRunner.stopTimeoutThread();

    for (int i = 0; i < testRunner.getNumResults(); ++i)
        results.add (*testRunner.getResult (i));

    updateFileNameIfPossible (test, testRunner);

    // A report that was asked for but couldn't be written is a failure, so CI doesn't silently lose it
    if (! report_writer::write (toReport (test, results), options.junitXmlFile, options.jsonResultsFile))
    {
        juce::UnitTestRunner::TestResult writeFailure ("pluginval", "Write report files");
        writeFailure.failures = 1;
        writeFailure.messages.add ("Unable to write the report file(s)");
        writeFailure.endTime = writeFailure.startTime;
        results.add (writeFailure);

        testRunner.logMessage ("*** FAILED: Unable to write the report file(s)");
    }

    const int failures = getNumFailures (results);

    if (! failures)
        testRunner.logMessage("SUCCESS");
    else
        testRunner.logMessage("FAILURE");

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
