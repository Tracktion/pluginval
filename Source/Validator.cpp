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
#include <numeric>

#if JUCE_MAC
 #include <signal.h>
#endif

// Defined in Main.cpp, used to create the file logger as early as possible
extern void slaveInitialised();

#if LOG_PIPE_SLAVE_COMMUNICATION
 #define LOG_FROM_MASTER(textToLog) if (Logger::getCurrentLogger() != nullptr) Logger::writeToLog ("*** Recieved:\n" + textToLog);
 #define LOG_TO_MASTER(textToLog)   if (Logger::getCurrentLogger() != nullptr) Logger::writeToLog ("*** Sending:\n" + textToLog);
 #define LOG_SLAVE(textToLog)       if (Logger::getCurrentLogger() != nullptr) Logger::writeToLog ("*** Log:\n" + String (textToLog));
#else
 #define LOG_FROM_MASTER(textToLog)
 #define LOG_TO_MASTER(textToLog)
 #define LOG_SLAVE(textToLog)
#endif

//==============================================================================
struct PluginsUnitTestRunner    : public UnitTestRunner,
                                  private Thread
{
    PluginsUnitTestRunner (std::function<void (const String&)> logCallback, std::unique_ptr<FileOutputStream> logDestination, int64 timeoutInMs)
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

    ~PluginsUnitTestRunner()
    {
        stopThread (5000);
    }

    FileOutputStream* getOutputFileStream() const
    {
        return outputStream.get();
    }

    void resultsUpdated() override
    {
        resetTimeout();
    }

    void logMessage (const String& message) override
    {
        if (! canSendLogMessage)
            return;

        resetTimeout();

        if (message.isNotEmpty())
        {
            if (outputStream)
                *outputStream << message << "\n";

            callback (message);
        }
    }

private:
    std::function<void (const String&)> callback;
    std::unique_ptr<FileOutputStream> outputStream;
    const int64 timeoutMs = -1;
    std::atomic<int64> timoutTime { -1 };
    std::atomic<bool> canSendLogMessage { true };

    void resetTimeout()
    {
        timoutTime = (Time::getCurrentTime() + RelativeTime::milliseconds (timeoutMs)).toMilliseconds();
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            if (Time::getCurrentTime().toMilliseconds() > timoutTime)
            {
                logMessage ("*** FAILED: Timeout after " + RelativeTime::milliseconds (timeoutMs).getDescription());
                canSendLogMessage = false;
                outputStream.reset();

                // Give the log a second to flush the message before terminating
                Thread::sleep (1000);
                Process::terminate();
            }

            Thread::sleep (200);
        }
    }
};

//==============================================================================
//==============================================================================
String getFileNameFromDescription (PluginTests& test)
{
    auto getBaseName = [&]() -> String
    {
        if (auto pd = test.getDescriptions().getFirst())
            return pd->manufacturerName + " - " + pd->name + " " + pd->version + " - " + SystemStats::getOperatingSystemName() + " " + pd->pluginFormatName;

        const auto fileOrID = test.getFileOrID();

        if (fileOrID.isNotEmpty())
        {
            if (File::isAbsolutePath (fileOrID))
                return File (fileOrID).getFileName();
        }

        return "pluginval Log";
    };

    return getBaseName() + "_" + Time::getCurrentTime().toString (true, true).replace (":", ",") + ".txt";
}

File getDestinationFile (PluginTests& test)
{
    const auto dir = test.getOptions().outputDir;

    if (dir == File())
        return {};

    if (dir.existsAsFile() || ! dir.createDirectory())
    {
        jassertfalse;
        return {};
    }

    return dir.getChildFile (getFileNameFromDescription (test));
}

std::unique_ptr<FileOutputStream> createDestinationFileStream (PluginTests& test)
{
    auto file = getDestinationFile (test);

    if (file == File())
        return {};

    std::unique_ptr<FileOutputStream> fos (file.createOutputStream());

    if (fos && fos->openedOk())
        return fos;

    jassertfalse;
    return {};
}

/** Renames a file based upon its description.
    This is useful if a path or AU ID was passed in as the plugin info isn't known at that point.
*/
void updateFileNameIfPossible (PluginTests& test, PluginsUnitTestRunner& runner)
{
    if (auto os = runner.getOutputFileStream())
    {
        const auto sourceFile = os->getFile();
        const auto destName = getFileNameFromDescription (test);

        if (destName.isEmpty() || sourceFile.getFileName() == destName)
            return;

        const bool success = sourceFile.moveFileTo (sourceFile.getParentDirectory().getChildFile (destName));
        ignoreUnused (success);
        jassert (success);
    }
}


//==============================================================================
//==============================================================================
inline Array<UnitTestRunner::TestResult> runTests (PluginTests& test, std::function<void (const String&)> callback)
{
    const auto options = test.getOptions();
    Array<UnitTestRunner::TestResult> results;
    PluginsUnitTestRunner testRunner (std::move (callback), createDestinationFileStream (test), options.timeoutMs);
    testRunner.setAssertOnFailure (false);

    Array<UnitTest*> testsToRun;
    testsToRun.add (&test);
    testRunner.runTests (testsToRun, options.randomSeed);

    for (int i = 0; i < testRunner.getNumResults(); ++i)
        results.add (*testRunner.getResult (i));

    updateFileNameIfPossible (test, testRunner);

    return results;
}

inline Array<UnitTestRunner::TestResult> validate (const PluginDescription& pluginToValidate, PluginTests::Options options, std::function<void (const String&)> callback)
{
    PluginTests test (pluginToValidate, options);
    return runTests (test, std::move (callback));
}

inline Array<UnitTestRunner::TestResult> validate (const String& fileOrIDToValidate, PluginTests::Options options, std::function<void (const String&)> callback)
{
    PluginTests test (fileOrIDToValidate, options);
    return runTests (test, std::move (callback));
}

inline int getNumFailures (Array<UnitTestRunner::TestResult> results)
{
    return std::accumulate (results.begin(), results.end(), 0,
                            [] (int count, const UnitTestRunner::TestResult& r) { return count + r.failures; });
}

//==============================================================================
namespace IDs
{
    #define DECLARE_ID(name) const Identifier name (#name);

    DECLARE_ID(PLUGINS)
    DECLARE_ID(PLUGIN)
    DECLARE_ID(fileOrID)
    DECLARE_ID(pluginDescription)
    DECLARE_ID(strictnessLevel)
    DECLARE_ID(randomSeed)
    DECLARE_ID(timeoutMs)
    DECLARE_ID(verbose)
    DECLARE_ID(numRepeats)
    DECLARE_ID(randomiseTestOrder)
    DECLARE_ID(dataFile)
    DECLARE_ID(outputDir)
    DECLARE_ID(withGUI)
    DECLARE_ID(disabledTests)

    DECLARE_ID(MESSAGE)
    DECLARE_ID(type)
    DECLARE_ID(text)
    DECLARE_ID(log)
    DECLARE_ID(numFailures)

    #undef DECLARE_ID
}

//==============================================================================
// This is a token that's used at both ends of our parent-child processes, to
// act as a unique token in the command line arguments.
static const char* validatorCommandLineUID = "validatorUID";

// A few quick utility functions to convert between raw data and ValueTrees
static ValueTree memoryBlockToValueTree (const MemoryBlock& mb)
{
    return ValueTree::readFromData (mb.getData(), mb.getSize());
}

static MemoryBlock valueTreeToMemoryBlock (const ValueTree& v)
{
    MemoryOutputStream mo;
    v.writeToStream (mo);

    return mo.getMemoryBlock();
}

static String toXmlString (const ValueTree& v)
{
    if (auto xml = std::unique_ptr<XmlElement> (v.createXml()))
        return xml->toString (XmlElement::TextFormat().withoutHeader());

    return {};
}


//==============================================================================
/*  This class gets instantiated in the child process, and receives messages from
    the master process.
*/
class ValidatorSlaveProcess : public ChildProcessSlave,
                              private Thread,
                              private DeletedAtShutdown
{
public:
    ValidatorSlaveProcess()
        : Thread ("ValidatorSlaveProcess")
    {
        LOG_SLAVE("Constructing ValidatorSlaveProcess");

        // Initialise the crash handler to clear any previous crash logs
        initialiseCrashHandler();
        startThread (4);
    }

    ~ValidatorSlaveProcess()
    {
        LOG_SLAVE("Destructing ValidatorSlaveProcess");
        stopThread (5000);
    }

    void setConnected (bool isNowConnected) noexcept
    {
        isConnected = isNowConnected;

        if (isConnected)
            sendValueTreeToMaster ({ IDs::MESSAGE, {{ IDs::type, "connected" }} });
    }

    void setMasterProcess (ChildProcessMaster* newMaster)
    {
        master = newMaster;
        setConnected (master != nullptr);
    }

    void handleMessageFromMaster (const MemoryBlock& mb) override
    {
        LOG_FROM_MASTER(toXmlString (memoryBlockToValueTree (mb)));
        addRequest (mb);
    }

    void handleConnectionLost() override
    {
        // Force terminate to avoid any zombie processed that can't quit cleanly
        Process::terminate();
    }

private:
    struct LogMessagesSender    : public Thread
    {
        LogMessagesSender (ValidatorSlaveProcess& vsp)
            : Thread ("SlaveMessageSender"), owner (vsp)
        {
            startThread (1);
        }

        ~LogMessagesSender()
        {
            stopThread (2000);
            sendLogMessages();
        }

        void logMessage (const String& m)
        {
            if (! owner.isConnected)
                return;

            const ScopedLock sl (logMessagesLock);
            logMessages.add (m);
        }

        void run() override
        {
            while (! threadShouldExit())
            {
                sendLogMessages();
                Thread::sleep (200);
            }
        }

        void sendLogMessages()
        {
            StringArray messagesToSend;

            {
                const ScopedLock sl (logMessagesLock);
                messagesToSend.swapWith (logMessages);
            }

            if (owner.isConnected && ! messagesToSend.isEmpty())
                owner.sendValueTreeToMaster ({ IDs::MESSAGE, {{ IDs::type, "log" }, { IDs::text, messagesToSend.joinIntoString ("\n") }} }, false);
        }

        ValidatorSlaveProcess& owner;
        CriticalSection logMessagesLock;
        StringArray logMessages;
    };

    CriticalSection requestsLock;
    std::vector<MemoryBlock> requestsToProcess;
    std::atomic<bool> isConnected { false };
    LogMessagesSender logMessagesSender { *this };
    ChildProcessMaster* master = nullptr;

    void logMessage (const String& m)
    {
        logMessagesSender.logMessage (m);
    }

    void sendValueTreeToMaster (const ValueTree& v, bool flushLog = true)
    {
        if (flushLog)
            logMessagesSender.sendLogMessages();

        if (master != nullptr)
        {
            master->handleMessageFromSlave (valueTreeToMemoryBlock (v));
            return;
        }

        LOG_TO_MASTER(toXmlString (v));
        sendMessageToMaster (valueTreeToMemoryBlock (v));
    }

    void run() override
    {
        LOG_SLAVE("Starting slave thread");

        while (! threadShouldExit())
        {
            processRequests();

            const ScopedLock sl (requestsLock);

            if (requestsToProcess.empty())
                Thread::sleep (500);
        }

        LOG_SLAVE("Ended slave thread");
    }

    void addRequest (const MemoryBlock& mb)
    {
        {
            const ScopedLock sl (requestsLock);
            requestsToProcess.push_back (mb);
        }

        notify();
    }

    void processRequests()
    {
        std::vector<MemoryBlock> requests;

        {
            const ScopedLock sl (requestsLock);
            requests.swap (requestsToProcess);
        }

        LOG_SLAVE("processRequests:\n" + String ((int) requests.size()));

        for (const auto& r : requests)
            processRequest (r);
    }

    void processRequest (MemoryBlock mb)
    {
        const ValueTree v (memoryBlockToValueTree (mb));
        LOG_SLAVE("processRequest:\n" + toXmlString (v));

        if (v.hasType (IDs::PLUGINS))
        {
            PluginTests::Options options;
            options.strictnessLevel = v.getProperty (IDs::strictnessLevel, 5);
            options.randomSeed = v[IDs::randomSeed];
            options.timeoutMs = v.getProperty (IDs::timeoutMs, -1);
            options.verbose = v[IDs::verbose];
            options.numRepeats = v.getProperty (IDs::numRepeats, 1);
            options.randomiseTestOrder = v[IDs::randomiseTestOrder];
            options.dataFile = File (v[IDs::dataFile].toString());
            options.outputDir = File (v[IDs::outputDir].toString());
            options.withGUI = v.getProperty (IDs::withGUI, true);
            options.disabledTests = StringArray::fromTokens (v.getProperty (IDs::disabledTests).toString(), false);

            for (auto c : v)
            {
                String fileOrID;
                Array<UnitTestRunner::TestResult> results;
                LOG_SLAVE("processRequest - child:\n" + toXmlString (c));

                if (c.hasProperty (IDs::fileOrID))
                {
                    fileOrID = c[IDs::fileOrID].toString();
                    sendValueTreeToMaster ({
                        IDs::MESSAGE, {{ IDs::type, "started" }, { IDs::fileOrID, fileOrID }}
                    });

                    results = validate (c[IDs::fileOrID].toString(), options, [this] (const String& m) { logMessage (m); });
                }
                else if (c.hasProperty (IDs::pluginDescription))
                {
                    MemoryOutputStream ms;

                    if (Base64::convertFromBase64 (ms, c[IDs::pluginDescription].toString()))
                    {
                        if (auto xml = std::unique_ptr<XmlElement> (XmlDocument::parse (ms.toString())))
                        {
                            PluginDescription pd;

                            if (pd.loadFromXml (*xml))
                            {
                                fileOrID = pd.createIdentifierString();
                                sendValueTreeToMaster ({
                                    IDs::MESSAGE, {{ IDs::type, "started" }, { IDs::fileOrID, fileOrID }}
                                });

                                results = validate (pd, options, [this] (const String& m) { logMessage (m); });
                            }
                            else
                            {
                                LOG_SLAVE("processRequest - failed to load PluginDescription from XML:\n" + ms.toString());
                            }
                        }
                        else
                        {
                            LOG_SLAVE("processRequest - failed to parse pluginDescription:\n" + ms.toString());
                        }
                    }
                    else
                    {
                        LOG_SLAVE("processRequest - failed to convert pluginDescription from base64:\n" + ms.toString());
                    }
                }

                jassert (fileOrID.isNotEmpty());
                sendValueTreeToMaster ({
                    IDs::MESSAGE, {{ IDs::type, "result" }, { IDs::fileOrID, fileOrID }, { IDs::numFailures, getNumFailures (results) }}
                });
            }
        }

        sendValueTreeToMaster ({
            IDs::MESSAGE, {{ IDs::type, "complete" }}
        });
    }
};

//==============================================================================
class ValidatorMasterProcess    : public ChildProcessMaster
{
public:
    ValidatorMasterProcess() = default;

    // Callback which can be set to log any calls sent to the slave
    std::function<void (const String&)> logCallback;

    // Callback which can be set to be notified of a lost connection
    std::function<void()> connectionLostCallback;

    //==============================================================================
    // Callback which can be set to be informed when validation starts
    std::function<void (const String&)> validationStartedCallback;

    // Callback which can be set to be informed when a log message is posted
    std::function<void (const String&)> logMessageCallback;

    // Callback which can be set to be informed when a validation completes
    std::function<void (const String&, int)> validationCompleteCallback;

    // Callback which can be set to be informed when all validations have been completed
    std::function<void()> completeCallback;

    //==============================================================================
    Result launchInProcess()
    {
        validatorSlaveProcess = std::make_unique<ValidatorSlaveProcess>();
        validatorSlaveProcess->setMasterProcess (this);

        return Result::ok();
    }

    Result launch()
    {
        // Make sure we send 0 as the streamFlags args or the pipe can hang during DBG messages
        const bool ok = launchSlaveProcess (File::getSpecialLocation (File::currentExecutableFile),
                                            validatorCommandLineUID, 2000, 0);

        if (! connectionWaiter.wait (5000))
            return Result::fail ("Error: Slave took too long to launch");

        return ok ? Result::ok() : Result::fail ("Error: Slave failed to launch");
    }

    //==============================================================================
    void handleMessageFromSlave (const MemoryBlock& mb) override
    {
        auto v = memoryBlockToValueTree (mb);

        if (v.hasType (IDs::MESSAGE))
        {
            const auto type = v[IDs::type].toString();

            if (logMessageCallback && type == "log")
                logMessageCallback (v[IDs::text].toString());

            if (validationCompleteCallback && type == "result")
                validationCompleteCallback (v[IDs::fileOrID].toString(), v[IDs::numFailures]);

            if (validationStartedCallback && type == "started")
                validationStartedCallback (v[IDs::fileOrID].toString());

            if (completeCallback && type == "complete")
                completeCallback();

            if (type == "connected")
                connectionWaiter.signal();
        }

        logMessage ("Received: " + toXmlString (v));
    }

    // This gets called if the slave process dies.
    void handleConnectionLost() override
    {
        logMessage ("Connection lost to child process!");

        if (connectionLostCallback)
            connectionLostCallback();
    }

    //==============================================================================
    /** Triggers validation of a set of files or IDs. */
    void validate (const StringArray& fileOrIDsToValidate, PluginTests::Options options)
    {
        auto v = createPluginsTree (options);

        for (auto fileOrID : fileOrIDsToValidate)
        {
            jassert (fileOrID.isNotEmpty());
            v.appendChild ({ IDs::PLUGIN, {{ IDs::fileOrID, fileOrID.trimCharactersAtEnd ("\\/") }} }, nullptr);
        }

        sendValueTreeToSlave (v);
    }

    /** Triggers validation of a set of PluginDescriptions. */
    void validate (const Array<PluginDescription>& pluginsToValidate, PluginTests::Options options)
    {
        auto v = createPluginsTree (options);

        for (auto pd : pluginsToValidate)
            if (auto xml = std::unique_ptr<XmlElement> (pd.createXml()))
                v.appendChild ({ IDs::PLUGIN, {{ IDs::pluginDescription, Base64::toBase64 (xml->toString()) }} }, nullptr);

        sendValueTreeToSlave (v);
    }

private:
    WaitableEvent connectionWaiter;
    std::unique_ptr<ValidatorSlaveProcess> validatorSlaveProcess;

    static ValueTree createPluginsTree (PluginTests::Options options)
    {
        ValueTree v (IDs::PLUGINS);
        v.setProperty (IDs::strictnessLevel, options.strictnessLevel, nullptr);
        v.setProperty (IDs::randomSeed, options.randomSeed, nullptr);
        v.setProperty (IDs::timeoutMs, options.timeoutMs, nullptr);
        v.setProperty (IDs::verbose, options.verbose, nullptr);
        v.setProperty (IDs::numRepeats, options.numRepeats, nullptr);
        v.setProperty (IDs::randomiseTestOrder, options.randomiseTestOrder, nullptr);
        v.setProperty (IDs::dataFile, options.dataFile.getFullPathName(), nullptr);
        v.setProperty (IDs::outputDir, options.outputDir.getFullPathName(), nullptr);
        v.setProperty (IDs::withGUI, options.withGUI, nullptr);
        v.setProperty (IDs::disabledTests, options.disabledTests.joinIntoString ("\n"), nullptr);

        return v;
    }

    void sendValueTreeToSlave (const ValueTree& v)
    {
        if (validatorSlaveProcess)
        {
            validatorSlaveProcess->handleMessageFromMaster (valueTreeToMemoryBlock (v));
            return;
        }

        logMessage ("Sending: " + toXmlString (v));

        if (! sendMessageToSlave (valueTreeToMemoryBlock (v)))
            logMessage ("...failed");
    }

    void logMessage (const String& s)
    {
        if (logCallback)
            logCallback (s);
    }
};

//==============================================================================
Validator::Validator() {}
Validator::~Validator() {}

bool Validator::isConnected() const
{
    return masterProcess != nullptr;
}

bool Validator::validate (const StringArray& fileOrIDsToValidate, PluginTests::Options options)
{
    if (! ensureConnection())
        return false;

    masterProcess->validate (fileOrIDsToValidate, options);
    return true;
}

bool Validator::validate (const Array<PluginDescription>& pluginsToValidate, PluginTests::Options options)
{
    if (! ensureConnection())
        return false;

    masterProcess->validate (pluginsToValidate, options);
    return true;
}

void Validator::setValidateInProcess (bool useSameProcess)
{
    launchInProcess = useSameProcess;
}

//==============================================================================
void Validator::logMessage (const String& m)
{
    listeners.call (&Listener::logMessage, m);
}

bool Validator::ensureConnection()
{
    if (! masterProcess)
    {
        sendChangeMessage();
        masterProcess = std::make_unique<ValidatorMasterProcess>();

       #if LOG_PIPE_COMMUNICATION
        masterProcess->logCallback = [this] (const String& m) { logMessage (m); };
       #endif
        masterProcess->connectionLostCallback = [this]
            {
                listeners.call (&Listener::connectionLost);
                triggerAsyncUpdate();
            };

        masterProcess->validationStartedCallback    = [this] (const String& id) { listeners.call (&Listener::validationStarted, id); };
        masterProcess->logMessageCallback           = [this] (const String& m) { listeners.call (&Listener::logMessage, m); };
        masterProcess->validationCompleteCallback   = [this] (const String& id, int numFailures) { listeners.call (&Listener::itemComplete, id, numFailures); };
        masterProcess->completeCallback             = [this] { listeners.call (&Listener::allItemsComplete); triggerAsyncUpdate(); };

        const auto result = launchInProcess ? masterProcess->launchInProcess()
                                            : masterProcess->launch();

        if (result.failed())
        {
            logMessage (result.getErrorMessage());
            return false;
        }

        logMessage (String (ProjectInfo::projectName) + " v" + ProjectInfo::versionString
                    + " - " + SystemStats::getJUCEVersion());

        return true;
    }

    return true;
}

void Validator::handleAsyncUpdate()
{
    masterProcess.reset();
    sendChangeMessage();
}


//==============================================================================
#if JUCE_MAC
static void killWithoutMercy (int)
{
    kill (getpid(), SIGKILL);
}

static void setupSignalHandling()
{
    const int signals[] = { SIGFPE, SIGILL, SIGSEGV, SIGBUS, SIGABRT };

    for (int i = 0; i < numElementsInArray (signals); ++i)
    {
        ::signal (signals[i], killWithoutMercy);
        ::siginterrupt (signals[i], 1);
    }
}
#endif

//==============================================================================
bool invokeSlaveProcessValidator (const String& commandLine)
{
   #if JUCE_MAC
    setupSignalHandling();
   #endif

    auto slave = std::make_unique<ValidatorSlaveProcess>();

    if (slave->initialiseFromCommandLine (commandLine, validatorCommandLineUID))
    {
        slaveInitialised();
        slave->setConnected (true);
        slave.release(); // allow the slave object to stay alive - it'll handle its own deletion.
        return true;
    }

    return false;
}
