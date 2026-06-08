/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>

#include <engine/preflight.h>

enum
{
	MAX_EXTRA_PATH_CHECKS = 16,
};

static void PrintUsage(const char *pProgName)
{
	dbg_msg("preflight", "usage: %s [--client|--server|--tool] [--check-map PATH] [--check-demo PATH] [--check-dir PATH] [--writable] [--no-preflight] [--help|-h]", pProgName);
	dbg_msg("preflight", "");
	dbg_msg("preflight", "Unified preflight diagnostic tool for Teeworlds.");
	dbg_msg("preflight", "Runs the same checks used by client/server/tools before startup.");
	dbg_msg("preflight", "");
	dbg_msg("preflight", "Modes:");
	dbg_msg("preflight", "  --client   Check client requirements (graphics, audio, resources)");
	dbg_msg("preflight", "  --server   Check server requirements (port binding, map permissions) [default]");
	dbg_msg("preflight", "  --tool     Check tool requirements (no graphics/audio/port checks)");
	dbg_msg("preflight", "");
	dbg_msg("preflight", "Path checks (for map/demo tools, may be repeated):");
	dbg_msg("preflight", "  --check-map PATH     Verify a .map file is accessible and readable");
	dbg_msg("preflight", "  --check-demo PATH    Verify a .demo file is accessible and readable");
	dbg_msg("preflight", "  --check-dir PATH     Verify a directory is accessible");
	dbg_msg("preflight", "  --writable           Require write permission for subsequent --check-* paths");
	dbg_msg("preflight", "");
	dbg_msg("preflight", "Exit code: 0 = all checks passed, 1 = errors found, 2 = usage error");
}

int main(int argc, const char **argv)
{
	cmdline_fix(&argc, &argv);

	EPreflightMode Mode = PREMODE_SERVER;
	SPreflightPathCheck aExtraPathChecks[MAX_EXTRA_PATH_CHECKS];
	int NumExtraPathChecks = 0;
	bool NextRequireWrite = false;

	for(int i = 1; i < argc; i++)
	{
		if(str_comp(argv[i], "--client") == 0)
		{
			Mode = PREMODE_CLIENT;
		}
		else if(str_comp(argv[i], "--server") == 0)
		{
			Mode = PREMODE_SERVER;
		}
		else if(str_comp(argv[i], "--tool") == 0)
		{
			Mode = PREMODE_TOOL;
		}
		else if(str_comp(argv[i], "--writable") == 0)
		{
			NextRequireWrite = true;
		}
		else if(str_comp(argv[i], "--check-map") == 0 || str_comp(argv[i], "--check-demo") == 0 || str_comp(argv[i], "--check-dir") == 0)
		{
			if(i + 1 >= argc)
			{
				dbg_msg("preflight", "error: %s requires a PATH argument", argv[i]);
				PrintUsage(argv[0]);
				cmdline_free(argc, argv);
				return 2;
			}
			if(NumExtraPathChecks >= MAX_EXTRA_PATH_CHECKS)
			{
				dbg_msg("preflight", "warning: too many path checks, ignoring '%s %s'", argv[i], argv[i + 1]);
				i++;
				continue;
			}

			bool IsDir = str_comp(argv[i], "--check-dir") == 0;
			const char *pDesc;
			if(str_comp(argv[i], "--check-map") == 0)
				pDesc = "map file";
			else if(str_comp(argv[i], "--check-demo") == 0)
				pDesc = "demo file";
			else
				pDesc = "directory";

			aExtraPathChecks[NumExtraPathChecks].m_pPath = argv[i + 1];
			aExtraPathChecks[NumExtraPathChecks].m_IsDir = IsDir;
			aExtraPathChecks[NumExtraPathChecks].m_RequireWrite = NextRequireWrite;
			aExtraPathChecks[NumExtraPathChecks].m_pDescription = pDesc;
			NumExtraPathChecks++;
			NextRequireWrite = false;
			i++;
		}
		else if(str_comp(argv[i], "--help") == 0 || str_comp(argv[i], "-h") == 0)
		{
			PrintUsage(argv[0]);
			cmdline_free(argc, argv);
			return 0;
		}
	}

	if(PreflightShouldSkip(argc, argv))
	{
		dbg_msg("preflight", "checks skipped (--no-preflight).");
		cmdline_free(argc, argv);
		return 0;
	}

	int Errors;
	if(Mode == PREMODE_CLIENT)
		Errors = PreflightRunForClient(argc, argv);
	else if(Mode == PREMODE_SERVER)
		Errors = PreflightRunForServer(argc, argv);
	else
		Errors = PreflightRunForTool(argc, argv, 0,
			NumExtraPathChecks > 0 ? aExtraPathChecks : 0, NumExtraPathChecks);

	cmdline_free(argc, argv);

	if(Errors < 0)
	{
		dbg_msg("preflight", "preflight failed.");
		return 1;
	}

	dbg_msg("preflight", "all preflight checks passed.");
	return 0;
}
