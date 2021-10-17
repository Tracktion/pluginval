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

void JUnitListener::itemComplete(const String &id, int, const Array<UnitTestRunner::TestResult> &itemResults)
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