#pragma once

#include "UnitTest.h"

class CompanionParamServiceTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testInitialTemplateLoads();
    void _testStageAndReset();
    void _testParamExtValueDecode();
    void _testParamExtAckHandling();
    void _testExportAndImport();
    void _testRejectsWrongComponentId();
};
