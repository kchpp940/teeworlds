/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>

#include <engine/preflight.h>

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

	for(int i = 1; i < argc; i++)
	{
		if(str_comp(argv[i], "--client") == 0)
			Mode = PREMODE_CLIENT;
		else if(str_comp(argv[i], "--server") == 0)
			Mode = PREMODE_SERVER;
		else if(str_comp(argv[i], "--tool") == 0)
			Mode = PREMODE_TOOL;
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

	int Errors = PreflightInitAndRun("Teeworlds", Mode, argc, argv);

	cmdline_free(argc, argv);

	if(Errors < 0)
	{
		dbg_msg("preflight", "preflight failed.");
		return 1;
	}

	dbg_msg("preflight", "all preflight checks passed.");
	return 0;
}
