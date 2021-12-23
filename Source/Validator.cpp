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
 #include <sys/types.h>
 #include <unistd.h>
#endif

// Defined in Main.cpp, used to create the file logger as early as possible
extern void childInitialised();

#if LOG_PIPE_CHILD_COMMUNICATION
 #define LOG_FROM_PARENT(textToLog) if (Logger::getCurrentLogger() != nullptr) Logger::writeToLog ("*** Recieved:\n" + textToLog);
 #define LOG_TO_PARENT(textToLog)   if (Logger::getCurrentLogger() != nullptr) Logger::writeToLog ("*** Sending:\n" + textToLog);
 #define LOG_CHILD(textToLog)       if (Logger::getCurrentLogger() != nullptr) Logger::writeToLog ("*** Log:\n" + String (textToLog));
#else
 #define LOG_FROM_PARENT(textToLog)
 #define LOG_TO_PARENT(textToLog)
 #define LOG_CHILD(textToLog)
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

    ~PluginsUnitTestRunner() override
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

            if (getNumResults() > 0)
            {
                auto testResult = getResult(getNumResults() - 1);
                output.getReference(testResult).add(message);
            }

            callback (message);
        }
    }

    StringArray getTestOutput(int testResultIndex) const
    {
        if (testResultIndex >= 0 && testResultIndex < getNumResults())
        {
            return output[getResult((testResultIndex))];
        }
        return {};
    }

private:
    std::function<void (const String&)> callback;
    std::unique_ptr<FileOutputStream> outputStream;
    const int64 timeoutMs = -1;
    std::atomic<int64> timoutTime { -1 };
    std::atomic<bool> canSendLogMessage { true };

    HashMap<const UnitTestRunner::TestResult*, StringArray> output;

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
inline PluginTestResultArray runTests (PluginTests& test, std::function<void (const String&)> callback)
{
    const auto options = test.getOptions();
    PluginTestResultArray results;
    PluginsUnitTestRunner testRunner (std::move (callback), createDestinationFileStream (test), options.timeoutMs);
    testRunner.setAssertOnFailure (false);

    Array<UnitTest*> testsToRun;
    testsToRun.add (&test);
    testRunner.runTests (testsToRun, options.randomSeed);

    for (int i = 0; i < testRunner.getNumResults(); ++i)
    {
        results.add({ *testRunner.getResult(i), testRunner.getTestOutput(i) });
    }

    updateFileNameIfPossible (test, testRunner);

    return results;
}

inline PluginTestResultArray validate (const PluginDescription& pluginToValidate, PluginTests::Options options, std::function<void (const String&)> callback)
{
    PluginTests test (pluginToValidate, options);
    return runTests (test, std::move (callback));
}

inline PluginTestResultArray validate (const String& fileOrIDToValidate, PluginTests::Options options, std::function<void (const String&)> callback)
{
    PluginTests test (fileOrIDToValidate, options);
    return runTests (test, std::move (callback));
}

inline int getNumFailures (const PluginTestResultArray& results)
{
    return std::accumulate (results.begin(), results.end(), 0,
                            [] (int count, const auto& r) { return count + r.result.failures; });
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
    DECLARE_ID(sampleRates)
    DECLARE_ID(blockSizes)

    DECLARE_ID(MESSAGE)
    DECLARE_ID(type)
    DECLARE_ID(text)
    DECLARE_ID(log)
    DECLARE_ID(numFailures)

    DECLARE_ID(testResultArray)
        DECLARE_ID(testResultItem)
            DECLARE_ID(testName)
            DECLARE_ID(testSubcategoryName)
            DECLARE_ID(testPassCount)
            DECLARE_ID(testFailureCount)
            DECLARE_ID(testFailureMessageArray)
                DECLARE_ID(testFailureMessageItem)
                    DECLARE_ID(testFailureMessageText)
            DECLARE_ID(testOutputMessageArray)
                DECLARE_ID(testOutputMessageItem)
                    DECLARE_ID(testOutputMessageText)
            DECLARE_ID(testStartTime)
            DECLARE_ID(testEndTime)

    #undef DECLARE_ID
}

namespace MessageTypes
{
    const String result = "result";
    const String log = "log";
    const String started = "started";
    const String complete = "complete";
    const String connected = "connected";
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
    the parent process.
*/
class ValidatorChildProcess : public ChildProcessSlave,
                              private Thread,
                              private DeletedAtShutdown
{
public:
    ValidatorChildProcess()
        : Thread ("ValidatorChildProcess")
    {
        LOG_CHILD("Constructing ValidatorChildProcess");

        // Initialise the crash handler to clear any previous crash logs
        initialiseCrashHandler();
        startThread (4);
    }

    ~ValidatorChildProcess() override
    {
        LOG_CHILD("Destructing ValidatorChildProcess");
        stopThread (5000);
    }

    void setConnected (bool isNowConnected) noexcept
    {
        isConnected = isNowConnected;

        if (isConnected)
            sendValueTreeToParent ({ IDs::MESSAGE, {{ IDs::type, MessageTypes::connected }} });
    }

    void setParentProcess (ChildProcessMaster* newParent)
    {
        parent = newParent;
        setConnected (parent != nullptr);
    }

    void handleMessageFromMaster (const MemoryBlock& mb) override
    {
        LOG_FROM_PARENT(toXmlString (memoryBlockToValueTree (mb)));
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
        LogMessagesSender (ValidatorChildProcess& vsp)
            : Thread ("ChildMessageSender"), owner (vsp)
        {
            startThread (1);
        }

        ~LogMessagesSender() override
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
                owner.sendValueTreeToParent ({ IDs::MESSAGE, {{ IDs::type, MessageTypes::log }, { IDs::text, messagesToSend.joinIntoString ("\n") }} }, false);
        }

        ValidatorChildProcess& owner;
        CriticalSection logMessagesLock;
        StringArray logMessages;
    };

    CriticalSection requestsLock;
    std::vector<MemoryBlock> requestsToProcess;
    std::atomic<bool> isConnected { false };
    LogMessagesSender logMessagesSender { *this };
    ChildProcessMaster* parent = nullptr;

    void logMessage (const String& m)
    {
        logMessagesSender.logMessage (m);
    }

    void sendValueTreeToParent (const ValueTree& v, bool flushLog = true)
    {
        if (flushLog)
            logMessagesSender.sendLogMessages();

        if (parent != nullptr)
        {
            parent->handleMessageFromSlave (valueTreeToMemoryBlock (v));
            return;
        }

        LOG_TO_PARENT(toXmlString (v));
        sendMessageToMaster (valueTreeToMemoryBlock (v));
    }

    void run() override
    {
        LOG_CHILD("Starting child thread");

        while (! threadShouldExit())
        {
            processRequests();

            auto noRequests = [&]
            {
                const ScopedLock sl (requestsLock);
                return requestsToProcess.empty();
            }();

            if (noRequests)
                Thread::sleep (500);
        }

        LOG_CHILD("Ended child thread");
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

        LOG_CHILD("processRequests:\n" + String ((int) requests.size()));

        for (const auto& r : requests)
            processRequest (r);
    }

    static ValueTree serializeTestResults(const PluginTestResultArray& results)
    {
        ValueTree testResultArray { IDs::testResultArray };
        for (const auto& r: results)
        {
            auto add_messages = [](const StringArray& src, const Identifier& itemId,
                    const Identifier& textId, ValueTree& dest)
            {
                for (const auto& m: src)
                {
                    dest.appendChild({ itemId, { { textId, m } } }, nullptr);
                }
            };

            ValueTree testFailureMessageArray { IDs::testFailureMessageArray };

            add_messages(r.result.messages, IDs::testFailureMessageItem, IDs::testFailureMessageText, testFailureMessageArray);

            ValueTree testOutputMessageArray { IDs::testOutputMessageArray };

            add_messages(r.output, IDs::testOutputMessageItem, IDs::testOutputMessageText, testOutputMessageArray);

            ValueTree testResultItem { IDs::testResultItem,
                {
                    { IDs::testName, r.result.unitTestName },
                    { IDs::testSubcategoryName, r.result.subcategoryName },
                    { IDs::testPassCount, r.result.passes },
                    { IDs::testFailureCount, r.result.failures },
                    { IDs::testStartTime, r.result.startTime.toMilliseconds() },
                    { IDs::testEndTime, r.result.endTime.toMilliseconds() },
                },
                { testFailureMessageArray, testOutputMessageArray }
            };

            testResultArray.appendChild( testResultItem, nullptr );
        }

        return testResultArray;
    }

    void processRequest (MemoryBlock mb)
    {
        const ValueTree v (memoryBlockToValueTree (mb));
        LOG_CHILD("processRequest:\n" + toXmlString (v));

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
            options.disabledTests = StringArray::fromLines (v.getProperty (IDs::disabledTests).toString());
            options.sampleRates = juce::VariantConverter<std::vector<double>>::fromVar (v.getProperty (IDs::sampleRates));
            options.blockSizes = juce::VariantConverter<std::vector<int>>::fromVar (v.getProperty (IDs::blockSizes));

            for (auto c : v)
            {
                String fileOrID;
                PluginTestResultArray results;
                LOG_CHILD("processRequest - child:\n" + toXmlString (c));

                if (c.hasProperty (IDs::fileOrID))
                {
                    fileOrID = c[IDs::fileOrID].toString();
                    sendValueTreeToParent ({
                        IDs::MESSAGE, {{ IDs::type, MessageTypes::started }, { IDs::fileOrID, fileOrID }}
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
                                sendValueTreeToParent ({
                                    IDs::MESSAGE, {{ IDs::type, MessageTypes::started }, { IDs::fileOrID, fileOrID }}
                                });

                                results = validate (pd, options, [this] (const String& m) { logMessage (m); });
                            }
                            else
                            {
                                LOG_CHILD("processRequest - failed to load PluginDescription from XML:\n" + ms.toString());
                            }
                        }
                        else
                        {
                            LOG_CHILD("processRequest - failed to parse pluginDescription:\n" + ms.toString());
                        }
                    }
                    else
                    {
                        LOG_CHILD("processRequest - failed to convert pluginDescription from base64:\n" + ms.toString());
                    }
                }

                jassert (fileOrID.isNotEmpty());

                ValueTree result {
                    IDs::MESSAGE,
                    {
                        { IDs::type, MessageTypes::result },
                        { IDs::fileOrID, fileOrID },
                        { IDs::numFailures, getNumFailures (results) }
                    },
                    { serializeTestResults( results ) }
                };
                sendValueTreeToParent ( result );
            }
        }

        sendValueTreeToParent ({
            IDs::MESSAGE, {{ IDs::type, MessageTypes::complete }}
        });
    }
};

//==============================================================================
class ValidatorParentProcess    : public ChildProcessMaster
{
public:
    ValidatorParentProcess() = default;

    // Callback which can be set to log any calls sent to the child
    std::function<void (const String&)> logCallback;

    // Callback which can be set to be notified of a lost connection
    std::function<void()> connectionLostCallback;

    //==============================================================================
    // Callback which can be set to be informed when validation starts
    std::function<void (const String&)> validationStartedCallback;

    // Callback which can be set to be informed when a log message is posted
    std::function<void (const String&)> logMessageCallback;

    // Callback which can be set to be informed when a validation completes
    std::function<void (const String&, int, const PluginTestResultArray&)> validationCompleteCallback;

    // Callback which can be set to be informed when all validations have been completed
    std::function<void()> completeCallback;

    //==============================================================================
    Result launchInProcess()
    {
        validatorChildProcess = std::make_unique<ValidatorChildProcess>();
        validatorChildProcess->setParentProcess (this);

        return Result::ok();
    }

    Result launch()
    {
        // Make sure we send 0 as the streamFlags args or the pipe can hang during DBG messages
        const bool ok = launchSlaveProcess (File::getSpecialLocation (File::currentExecutableFile),
                                            validatorCommandLineUID, 2000, 0);

        if (! connectionWaiter.wait (5000))
            return Result::fail ("Error: Child took too long to launch");

        return ok ? Result::ok() : Result::fail ("Error: Child failed to launch");
    }

    static PluginTestResultArray deserializeTestResults(const ValueTree& testResultArray)
    {
        if (!testResultArray.hasType(IDs::testResultArray))
        {
            return {};
        }
        PluginTestResultArray results;
        for (const auto& testResultItem: testResultArray)
        {
            if (!testResultItem.hasType(IDs::testResultItem) ||
                    !testResultItem.hasProperty(IDs::testName) ||
                    !testResultItem.hasProperty(IDs::testSubcategoryName) ||
                    !testResultItem.hasProperty(IDs::testPassCount) ||
                    !testResultItem.hasProperty(IDs::testFailureCount) ||
                    !testResultItem.hasProperty(IDs::testStartTime) ||
                    !testResultItem.hasProperty(IDs::testEndTime)
                )
            {
                continue;
            }
            UnitTestRunner::TestResult result;
            result.unitTestName = testResultItem.getProperty(IDs::testName);
            result.subcategoryName = testResultItem.getProperty(IDs::testSubcategoryName);
            result.passes = testResultItem.getProperty(IDs::testPassCount);
            result.failures = testResultItem.getProperty(IDs::testFailureCount);
            result.startTime = Time(testResultItem.getProperty(IDs::testStartTime));
            result.endTime = Time(testResultItem.getProperty(IDs::testEndTime));

            auto add_messages = [&testResultItem](const Identifier& arrayId, const Identifier& itemId,
                    const Identifier& textId, StringArray& dest)
            {
                auto array = testResultItem.getChildWithName(arrayId);
                if (array.hasType(arrayId))
                {
                    for (const auto& item: array)
                    {
                        if (!item.hasType(itemId) || !item.hasProperty(textId))
                        {
                            continue;
                        }
                        dest.add(item.getProperty(textId));
                    }
                }
            };

            add_messages(IDs::testFailureMessageArray, IDs::testFailureMessageItem,
                         IDs::testFailureMessageText, result.messages);

            StringArray output;
            add_messages(IDs::testOutputMessageArray, IDs::testOutputMessageItem,
                         IDs::testOutputMessageText, output);

            results.add( { result, output } );
        }
        return results;
    }

    //==============================================================================
    void handleMessageFromSlave (const MemoryBlock& mb) override
    {
        auto v = memoryBlockToValueTree (mb);

        if (v.hasType (IDs::MESSAGE))
        {
            const auto type = v[IDs::type].toString();

            if (logMessageCallback && type == MessageTypes::log)
            {
                logMessageCallback (v[IDs::text].toString());
            }

            if (validationCompleteCallback && type == MessageTypes::result)
            {
                auto results = deserializeTestResults(v.getChildWithName(IDs::testResultArray));
                validationCompleteCallback (v[IDs::fileOrID].toString(), v[IDs::numFailures], results);
            }

            if (validationStartedCallback && type == MessageTypes::started)
            {
                validationStartedCallback (v[IDs::fileOrID].toString());
            }

            if (completeCallback && type == MessageTypes::complete)
            {
                completeCallback();
            }

            if (type == MessageTypes::connected)
            {
                connectionWaiter.signal();
            }
        }

        logMessage ("Received: " + toXmlString (v));
    }

    // This gets called if the child process dies.
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

        sendValueTreeToChild (v);
    }

    /** Triggers validation of a set of PluginDescriptions. */
    void validate (const Array<PluginDescription>& pluginsToValidate, PluginTests::Options options)
    {
        auto v = createPluginsTree (options);

        for (auto pd : pluginsToValidate)
            if (auto xml = std::unique_ptr<XmlElement> (pd.createXml()))
                v.appendChild ({ IDs::PLUGIN, {{ IDs::pluginDescription, Base64::toBase64 (xml->toString()) }} }, nullptr);

        sendValueTreeToChild (v);
    }

private:
    WaitableEvent connectionWaiter;
    std::unique_ptr<ValidatorChildProcess> validatorChildProcess;

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
        v.setProperty (IDs::sampleRates, juce::VariantConverter<std::vector<double>>::toVar (options.sampleRates), nullptr);
        v.setProperty (IDs::blockSizes, juce::VariantConverter<std::vector<int>>::toVar (options.blockSizes), nullptr);

        return v;
    }

    void sendValueTreeToChild (const ValueTree& v)
    {
        if (validatorChildProcess)
        {
            validatorChildProcess->handleMessageFromMaster (valueTreeToMemoryBlock (v));
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
    return parentProcess != nullptr;
}

bool Validator::validate (const StringArray& fileOrIDsToValidate, PluginTests::Options options)
{
    if (! ensureConnection())
        return false;

    parentProcess->validate (fileOrIDsToValidate, options);
    return true;
}

bool Validator::validate (const Array<PluginDescription>& pluginsToValidate, PluginTests::Options options)
{
    if (! ensureConnection())
        return false;

    parentProcess->validate (pluginsToValidate, options);
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
    if (! parentProcess)
    {
        sendChangeMessage();
        parentProcess = std::make_unique<ValidatorParentProcess>();

       #if LOG_PIPE_COMMUNICATION
        parentProcess->logCallback = [this] (const String& m) { logMessage (m); };
       #endif
        parentProcess->connectionLostCallback = [this]
            {
                listeners.call (&Listener::connectionLost);
                triggerAsyncUpdate();
            };

        parentProcess->validationStartedCallback    = [this] (const String& id) { listeners.call (&Listener::validationStarted, id); };
        parentProcess->logMessageCallback           = [this] (const String& m) { listeners.call (&Listener::logMessage, m); };
        parentProcess->validationCompleteCallback   = [this] (const String& id, int numFailures, const PluginTestResultArray& results) { listeners.call (&Listener::itemComplete, id, numFailures, results); };
        parentProcess->completeCallback             = [this] { listeners.call (&Listener::allItemsComplete); triggerAsyncUpdate(); };

        const auto result = launchInProcess ? parentProcess->launchInProcess()
                                            : parentProcess->launch();

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
    parentProcess.reset();
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
bool invokeChildProcessValidator (const String& commandLine)
{
   #if JUCE_MAC
    setupSignalHandling();
   #endif

    auto child = std::make_unique<ValidatorChildProcess>();

    if (child->initialiseFromCommandLine (commandLine, validatorCommandLineUID))
    {
        childInitialised();
        child->setConnected (true);
        child.release(); // allow the child object to stay alive - it'll handle its own deletion.
        return true;
    }

    return false;
}
