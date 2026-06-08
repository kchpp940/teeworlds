/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>

#include <engine/preflight.h>
#include <engine/storage.h>
#include <engine/shared/datafile.h>

static void PrintUsage(const char *pProgName)
{
	dbg_msg("map_version", "usage: %s <mapfile.map> [--no-preflight]", pProgName);
	dbg_msg("map_version", "");
	dbg_msg("map_version", "Prints version information and metadata for a Teeworlds .map file.");
	dbg_msg("map_version", "");
	dbg_msg("map_version", "Examples:");
	dbg_msg("map_version", "  %s dm1.map", pProgName);
	dbg_msg("map_version", "  %s /path/to/custom.map --no-preflight", pProgName);
}

int main(int argc, const char **argv)
{
	cmdline_fix(&argc, &argv);

	if(argc < 2)
	{
		PrintUsage(argv[0]);
		cmdline_free(argc, argv);
		return 2;
	}

	if(str_comp(argv[1], "--help") == 0 || str_comp(argv[1], "-h") == 0)
	{
		PrintUsage(argv[0]);
		cmdline_free(argc, argv);
		return 0;
	}

	const char *pMapPath = argv[1];

	if(!PreflightShouldSkip(argc, argv))
	{
		SPreflightPathCheck aChecks[2];
		aChecks[0].m_pPath = pMapPath;
		aChecks[0].m_IsDir = false;
		aChecks[0].m_RequireWrite = false;
		aChecks[0].m_pDescription = "input map file";

		SPreflightContext Ctx;
		int Errors = PreflightInitAndRun("Teeworlds", PREMODE_TOOL, argc, argv,
			&Ctx, 0, 0, 0, 0, aChecks, 1);

		if(Ctx.m_pPreflight && Ctx.m_pPreflight->HasErrors())
		{
			dbg_msg("map_version", "preflight checks failed with %d error(s). aborting.", Errors);
			dbg_msg("map_version", "use --no-preflight to skip checks (not recommended)");
			PreflightShutdown(&Ctx);
			cmdline_free(argc, argv);
			return 1;
		}
		PreflightShutdown(&Ctx);
	}

	dbg_msg("map_version", "loading map: %s", pMapPath);

	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_BASIC, argc, argv);
	if(!pStorage)
	{
		dbg_msg("map_version", "error: failed to create storage");
		cmdline_free(argc, argv);
		return 1;
	}

	CDataFileReader Reader;
	if(!Reader.Open(pStorage, pMapPath, IStorage::TYPE_ALL))
	{
		dbg_msg("map_version", "error: failed to open map file '%s'", pMapPath);
		delete pStorage;
		cmdline_free(argc, argv);
		return 1;
	}

	dbg_msg("map_version", "map info:");
	dbg_msg("map_version", "  items: %d", Reader.NumItems());
	dbg_msg("map_version", "  data blocks: %d", Reader.NumData());
	dbg_msg("map_version", "  crc: %u", Reader.Crc());

	char aSha256Str[SHA256_MAXSTRSIZE];
	sha256_str(Reader.Sha256(), aSha256Str, sizeof(aSha256Str));
	dbg_msg("map_version", "  sha256: %s", aSha256Str);

	int Start, Num;
	Reader.GetType(0, &Start, &Num);
	if(Num > 0)
	{
		int Type, ID;
		void *pItem = Reader.GetItem(Start, &Type, &ID);
		if(pItem)
		{
			dbg_msg("map_version", "  map version item found (type=0, count=%d, id=%d)", Num, ID);
		}
	}

	Reader.Close();
	delete pStorage;

	dbg_msg("map_version", "done.");
	cmdline_free(argc, argv);
	return 0;
}
