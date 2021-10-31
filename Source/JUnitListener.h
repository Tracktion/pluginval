#pragma once

#include <JuceHeader.h>
#include "CommonTypes.h"
#include "Validator.h"

class JUnitListener: public Validator::Listener
{
public:
    JUnitListener();
    void setReportFile(const File& newReportFile);
private:
    void validationStarted (const String& id) override;
    void logMessage (const String& m) override;
    void itemComplete (const String& id, int numItemFailures, const UnitTestResultsWithOutput& itemResults) override;
    void allItemsComplete() override;
    void connectionLost() override;

    File reportFile;
    HashMap<String, UnitTestResultsWithOutput> results;
};
