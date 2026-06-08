/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/kernel.h>
#include <engine/preflight.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <game/version.h>

static void PrintUsage(const char *pProgName)
{
	dbg_msg("preflight", "usage: %s [--client|--server|--tool] [--no-preflight] [--help|-h]", pProgName);
	dbg_msg("preflight", "");
	dbg_msg("preflight", "Unified preflight diagnostic tool for Teeworlds.");
	dbg_msg("preflight", "Runs the same checks used by client/server/tools before startup.");
	dbg_msg("preflight", "");
	dbg_msg("preflight", "Modes:");
	dbg_msg("preflight", "  --client   Check client requirements (graphics, audio, resources)");
	dbg_msg("preflight", "  --server   Check server requirements (port binding, map permissions) [default]");
	dbg_msg("preflight", "  --tool     Check tool requirements (no graphics/audio/port checks)");
	dbg_msg("preflight", "");
	dbg_msg("preflight", "Exit code: 0 = all checks passed, 1 = errors found, 2 = usage error");
}

int main(int argc, const char **argv)
{
	cmdline_fix(&argc, &argv);

	EPreflightMode Mode = PREMODE_SERVER;
	bool SkipPreflight = false;

	for(int i = 1; i < argc; i++)
	{
		if(str_comp(argv[i], "--client") == 0)
			Mode = PREMODE_CLIENT;
		else if(str_comp(argv[i], "--server") == 0)
			Mode = PREMODE_SERVER;
		else if(str_comp(argv[i], "--tool") == 0)
			Mode = PREMODE_TOOL;
		else if(str_comp(argv[i], "--no-preflight") == 0)
			SkipPreflight = true;
		else if(str_comp(argv[i], "--help") == 0 || str_comp(argv[i], "-h") == 0)
		{
			PrintUsage(argv[0]);
			return 0;
		}
	}

	if(SkipPreflight)
	{
		dbg_msg("preflight", "checks skipped (--no-preflight).");
		return 0;
	}

	int FlagMask = CFGFLAG_CLIENT | CFGFLAG_SERVER;
	if(Mode == PREMODE_CLIENT)
		FlagMask = CFGFLAG_CLIENT;
	else if(Mode == PREMODE_SERVER)
		FlagMask = CFGFLAG_SERVER;

	IEngine *pEngine = CreateEngine("Teeworlds");
	IStorage *pStorage = CreateStorage("Teeworlds",
		(Mode == PREMODE_CLIENT) ? IStorage::STORAGETYPE_CLIENT :
		(Mode == PREMODE_SERVER) ? IStorage::STORAGETYPE_SERVER :
		IStorage::STORAGETYPE_BASIC,
		argc, argv);
	IConsole *pConsole = CreateConsole(FlagMask);
	IConfigManager *pConfigManager = CreateConfigManager();

	IKernel *pKernel = IKernel::Create();
	bool RegisterFail = false;
	RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngine);
	RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConsole);
	RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConfigManager);
	RegisterFail = RegisterFail || !pKernel->RegisterInterface(pStorage);

	if(RegisterFail)
	{
		dbg_msg("preflight", "failed to register core interfaces. aborting.");
		return 2;
	}

	pEngine->Init();
	pConfigManager->Init(FlagMask);
	pConsole->Init();

	if(Mode != PREMODE_TOOL)
	{
		pConsole->ExecuteFile(SETTINGS_FILENAME ".cfg");
		pConsole->ExecuteFile("settings.cfg");
		pConsole->ExecuteFile("autoexec.cfg");
		if(argc > 1)
		{
			const char **apFilteredArgs = (const char **)mem_alloc(sizeof(const char *) * argc);
			int FilteredArgc = 0;
			for(int i = 1; i < argc; i++)
			{
				if(str_comp(argv[i], "--client") == 0
					|| str_comp(argv[i], "--server") == 0
					|| str_comp(argv[i], "--tool") == 0
					|| str_comp(argv[i], "--no-preflight") == 0
					|| str_comp(argv[i], "--help") == 0
					|| str_comp(argv[i], "-h") == 0)
					continue;
				apFilteredArgs[FilteredArgc++] = argv[i];
			}
			if(FilteredArgc > 0)
				pConsole->ParseArguments(FilteredArgc, apFilteredArgs);
			mem_free(apFilteredArgs);
		}
		pConfigManager->RestoreStrings();
	}

	IPreflight *pPreflight = CreatePreflight();
	pPreflight->SetMode(Mode);
	pPreflight->SetAppName("Teeworlds");
	pPreflight->SetStorage(pStorage);
	pPreflight->SetConfig(pConfigManager->Values());

	if(Mode == PREMODE_TOOL)
	{
		pPreflight->DisableCheck(PRECHECK_GRAPHICS);
		pPreflight->DisableCheck(PRECHECK_AUDIO);
		pPreflight->DisableCheck(PRECHECK_SERVER_PORT);
	}
	else if(Mode == PREMODE_CLIENT)
	{
		pPreflight->DisableCheck(PRECHECK_SERVER_PORT);
	}
	else if(Mode == PREMODE_SERVER)
	{
		pPreflight->DisableCheck(PRECHECK_GRAPHICS);
		pPreflight->DisableCheck(PRECHECK_AUDIO);
	}

	int Errors = pPreflight->RunAllChecks();
	bool HasErrors = pPreflight->HasErrors();

	delete pPreflight;
	delete pKernel;
	delete pEngine;
	delete pStorage;
	delete pConsole;
	delete pConfigManager;

	cmdline_free(argc, argv);

	if(HasErrors)
	{
		dbg_msg("preflight", "preflight failed with %d error(s).", Errors);
		return 1;
	}

	dbg_msg("preflight", "all preflight checks passed.");
	return 0;
}
