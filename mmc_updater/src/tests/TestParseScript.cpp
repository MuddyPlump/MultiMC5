#include "TestParseScript.h"

#include "TestUtils.h"
#include "UpdateScript.h"

void TestParseScript::testParse()
{
	UpdateScript script;

	script.parse("file_list.xml");

	TEST_COMPARE(script.isValid(), true);
	TEST_COMPARE(script.filesToInstall(),
				 std::vector<UpdateScriptFile>({UpdateScriptFile{"SourceA", "DestA", 755},
												UpdateScriptFile{"SourceB", "DestB", 644},
												UpdateScriptFile{"SourceC", "DestC", 644}}));
	TEST_COMPARE(
		script.filesToUninstall(),
		std::vector<QString>({"file-to-uninstall.txt", "other-file-to-uninstall.txt"}));
}

int main(int, char **)
{
	TestList<TestParseScript> tests;
	tests.addTest(&TestParseScript::testParse);
	return TestUtils::runTest(tests);
}
