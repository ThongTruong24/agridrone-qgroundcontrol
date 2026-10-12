#pragma once

#include "UnitTest.h"

class CompanionParamServiceTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testConfirmedFacts();
    void _testInitialTemplateLoads();
    void _testStageAndReset();
    void _testParamExtValueDecode();
    void _testParamExtAckHandling();
    void _testExportAndImport();
    void _testRejectsWrongComponentId();
    void _testExact16ByteParamId();
    void _testParamAckInProgressRemainsPending();
    void _testUnavailableValuesNotReplacedByDefault();
    void _testNamedLinksReplaceLegacyLinks();
    void _testReadOnlyAndBootOnlyCannotBeStaged();
    void _testUnknownParamsNotWritable();
    void _testFactsAreDeveloperCategoryAndEditsAreStaged();
    void _testEditorFactOrderAndStagedSubsystems();
};
