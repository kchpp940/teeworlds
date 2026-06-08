/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/preflight.h>

CTestInfo::CTestInfo()
{
	const ::testing::TestInfo *pTestInfo =
		::testing::UnitTest::GetInstance()->current_test_info();
	str_format(m_aFilenamePrefix, sizeof(m_aFilenamePrefix), "%s.%s-%d",
		pTestInfo->test_case_name(), pTestInfo->name(), pid());
	Filename(m_aFilename, sizeof(m_aFilename), ".tmp");
}

void CTestInfo::Filename(char *pBuffer, int BufferLength, const char *pSuffix)
{
	str_format(pBuffer, BufferLength, "%s%s", m_aFilenamePrefix, pSuffix);
}

int main(int argc, const char **argv)
{
	cmdline_fix(&argc, &argv);

	::testing::InitGoogleTest(&argc, const_cast<char **>(argv));

	if(!PreflightShouldSkip(argc, argv))
	{
		int Errors = PreflightRunForTool(argc, argv);
		if(Errors < 0)
		{
			dbg_msg("test", "preflight checks failed. aborting.");
			dbg_msg("test", "use --no-preflight to skip checks (not recommended)");
			return -1;
		}
	}

	int Result = RUN_ALL_TESTS();
	secure_random_uninit();
	cmdline_free(argc, argv);
	return Result;
}
