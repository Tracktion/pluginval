#include "JUnitListener.h"

#include "JUnitReport.h"

JUnitListener::JUnitListener()
{
}

void JUnitListener::setReportFile(const File &newReportFile)
{
    reportFile = newReportFile;
}

void JUnitListener::validationStarted(const String &)
{
}

void JUnitListener::logMessage(const String &)
{
}

void JUnitListener::itemComplete(const String &id, int, const PluginTestResultArray &itemResults)
{
    results.set(id, itemResults);
}

void JUnitListener::allItemsComplete()
{
    if (!reportFile.getFullPathName().isEmpty())
    {
        JUnitReport::write(results, reportFile);
    }
}

void JUnitListener::connectionLost()
{
    Validator::Listener::connectionLost();
    // TODO: create report when connection is lost
}
