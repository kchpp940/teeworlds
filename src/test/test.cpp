/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/kernel.h>
#include <engine/preflight.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

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

	bool SkipPreflight = false;
	for(int i = 1; i < argc; i++)
	{
		if(str_comp("--no-preflight", argv[i]) == 0)
		{
			SkipPreflight = true;
			break;
		}
	}

	::testing::InitGoogleTest(&argc, const_cast<char **>(argv));
	net_init();

	if(!SkipPreflight)
	{
		IEngine *pEngine = CreateEngine("Teeworlds");
		IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_BASIC, argc, argv);
		IConsole *pConsole = CreateConsole(CFGFLAG_CLIENT | CFGFLAG_SERVER);
		IConfigManager *pConfigManager = CreateConfigManager();

		IKernel *pKernel = IKernel::Create();
		bool RegisterFail = false;
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngine);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConsole);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConfigManager);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pStorage);

		if(!RegisterFail)
		{
			pEngine->Init();
			pConfigManager->Init(CFGFLAG_CLIENT | CFGFLAG_SERVER);
			pConsole->Init();
		}

		IPreflight *pPreflight = CreatePreflight();
		pPreflight->SetToolMode();
		pPreflight->SetAppName("Teeworlds");
		if(!RegisterFail)
		{
			pPreflight->SetStorage(pStorage);
			pPreflight->SetConfig(pConfigManager->Values());
		}
		pPreflight->SetNetworkAlreadyInitialized();
		pPreflight->DisableCheck(PRECHECK_GRAPHICS);
		pPreflight->DisableCheck(PRECHECK_AUDIO);
		pPreflight->DisableCheck(PRECHECK_SERVER_PORT);
		int PreflightErrors = pPreflight->RunAllChecks();
		bool PreflightHasErrors = pPreflight->HasErrors();
		delete pPreflight;

		if(!RegisterFail)
		{
			delete pKernel;
			delete pEngine;
			delete pStorage;
			delete pConsole;
			delete pConfigManager;
		}

		if(PreflightHasErrors)
		{
			dbg_msg("test", "preflight checks failed with %d error(s). aborting.", PreflightErrors);
			dbg_msg("test", "use --no-preflight to skip checks (not recommended)");
			return -1;
		}
	}

	int Result = RUN_ALL_TESTS();
	secure_random_uninit();
	cmdline_free(argc, argv);
	return Result;
}
