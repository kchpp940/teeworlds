/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <base/math.h>
#include <base/system.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/kernel.h>
#include <engine/preflight.h>
#include <engine/storage.h>

#include "preflight.h"

#if defined(CONF_FAMILY_UNIX)
#include <unistd.h>
#include <sys/stat.h>
#elif defined(CONF_FAMILY_WINDOWS)
#include <windows.h>
#endif

CPreflight::CPreflight()
{
	Reset();
}

void CPreflight::Reset()
{
	for(int i = 0; i < PRECHECK_COUNT; ++i)
		m_aCheckEnabled[i] = true;
	m_Mode = PREMODE_TOOL;
	m_NetworkAlreadyInitialized = false;
	m_aAppName[0] = 0;
	m_pStorage = 0;
	m_pConfig = 0;
	mem_zero(m_aaResourcePaths, sizeof(m_aaResourcePaths));
	m_NumResourcePaths = 0;
	mem_zero(m_aCustomChecks, sizeof(m_aCustomChecks));
	m_NumCustomChecks = 0;
	mem_zero(m_aResults, sizeof(m_aResults));
	m_NumResults = 0;
}

void CPreflight::EnableCheck(EPreflightCheck Check)
{
	if(Check >= 0 && Check < PRECHECK_COUNT)
		m_aCheckEnabled[Check] = true;
}

void CPreflight::DisableCheck(EPreflightCheck Check)
{
	if(Check >= 0 && Check < PRECHECK_COUNT)
		m_aCheckEnabled[Check] = false;
}

void CPreflight::SetMode(EPreflightMode Mode)
{
	m_Mode = Mode;
}

void CPreflight::SetClientMode()
{
	m_Mode = PREMODE_CLIENT;
}

void CPreflight::SetServerMode()
{
	m_Mode = PREMODE_SERVER;
}

void CPreflight::SetToolMode()
{
	m_Mode = PREMODE_TOOL;
}

void CPreflight::SetAppName(const char *pAppName)
{
	str_copy(m_aAppName, pAppName, sizeof(m_aAppName));
}

void CPreflight::SetStorage(IStorage *pStorage)
{
	m_pStorage = pStorage;
}

void CPreflight::SetConfig(CConfig *pConfig)
{
	m_pConfig = pConfig;
}

void CPreflight::SetNetworkAlreadyInitialized()
{
	m_NetworkAlreadyInitialized = true;
}

void CPreflight::AddResourcePath(const char *pPath)
{
	if(m_NumResourcePaths >= MAX_RESOURCE_PATHS_INTERNAL || !pPath || !pPath[0])
		return;
	str_copy(m_aaResourcePaths[m_NumResourcePaths], pPath, sizeof(m_aaResourcePaths[m_NumResourcePaths]));
	m_NumResourcePaths++;
}

void CPreflight::RegisterCustomCheck(FPreflightCustomCheck pfnCheck, void *pUser)
{
	if(!pfnCheck || m_NumCustomChecks >= MAX_CUSTOM_CHECKS)
		return;
	m_aCustomChecks[m_NumCustomChecks].m_pfnCheck = pfnCheck;
	m_aCustomChecks[m_NumCustomChecks].m_pUser = pUser;
	m_NumCustomChecks++;
}

void CPreflight::AddResult(EPreflightCheck Check, EPreflightSeverity Severity, const char *pMessage, const char *pFixSuggestion)
{
	if(m_NumResults >= MAX_RESULTS)
		return;
	SPreflightResult *pResult = &m_aResults[m_NumResults];
	pResult->m_Check = Check;
	pResult->m_Severity = Severity;
	str_copy(pResult->m_aMessage, pMessage, sizeof(pResult->m_aMessage));
	str_copy(pResult->m_aFixSuggestion, pFixSuggestion ? pFixSuggestion : "", sizeof(pResult->m_aFixSuggestion));
	m_NumResults++;
}

const char *CPreflight::CheckName(EPreflightCheck Check) const
{
	switch(Check)
	{
	case PRECHECK_NETWORK: return "network";
	case PRECHECK_GRAPHICS: return "graphics";
	case PRECHECK_AUDIO: return "audio";
	case PRECHECK_RESOURCE_PATHS: return "resource_paths";
	case PRECHECK_SERVER_PORT: return "server_port";
	case PRECHECK_DEMO_MAP_PERMISSIONS: return "demo_map_permissions";
	case PRECHECK_CONFIG_WRITE: return "config_write";
	default: return "unknown";
	}
}

const char *CPreflight::ModeName() const
{
	switch(m_Mode)
	{
	case PREMODE_CLIENT: return "client";
	case PREMODE_SERVER: return "server";
	case PREMODE_TOOL:
	default: return "tool";
	}
}

const SPreflightResult *CPreflight::GetResult(int Index) const
{
	if(Index < 0 || Index >= m_NumResults)
		return 0;
	return &m_aResults[Index];
}

bool CPreflight::HasErrors() const
{
	for(int i = 0; i < m_NumResults; ++i)
		if(m_aResults[i].m_Severity == PRESEVERITY_ERROR)
			return true;
	return false;
}

bool CPreflight::HasWarnings() const
{
	for(int i = 0; i < m_NumResults; ++i)
		if(m_aResults[i].m_Severity == PRESEVERITY_WARNING)
			return true;
	return false;
}

void CPreflight::PrintResults()
{
	if(m_NumResults == 0)
	{
		dbg_msg("preflight", "All checks passed.");
		return;
	}

	for(int i = 0; i < m_NumResults; ++i)
	{
		const SPreflightResult *pR = &m_aResults[i];
		const char *pSeverityStr = "INFO";
		if(pR->m_Severity == PRESEVERITY_WARNING)
			pSeverityStr = "WARNING";
		else if(pR->m_Severity == PRESEVERITY_ERROR)
			pSeverityStr = "ERROR";

		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "[%s] %s: %s", pSeverityStr, CheckName(pR->m_Check), pR->m_aMessage);
		dbg_msg("preflight", "%s", aBuf);

		if(pR->m_aFixSuggestion[0])
		{
			str_format(aBuf, sizeof(aBuf), "  fix: %s", pR->m_aFixSuggestion);
			dbg_msg("preflight", "%s", aBuf);
		}
	}
}

bool CPreflight::PathExistsAndReadable(const char *pPath)
{
	if(!pPath || !pPath[0])
		return false;

#if defined(CONF_FAMILY_UNIX)
	struct stat st;
	if(stat(pPath, &st) != 0)
		return false;
	if(access(pPath, R_OK) != 0)
		return false;
	return true;
#elif defined(CONF_FAMILY_WINDOWS)
	DWORD Attr = GetFileAttributesA(pPath);
	if(Attr == INVALID_FILE_ATTRIBUTES)
		return false;
	return true;
#else
	IOHANDLE f = io_open(pPath, IOFLAG_READ);
	if(f)
	{
		io_close(f);
		return true;
	}
	return fs_is_dir(pPath) != 0;
#endif
}

static bool CanCreateDirectory(const char *pPath)
{
	if(!pPath || !pPath[0])
		return false;

	char aParent[IO_MAX_PATH_LENGTH];
	str_copy(aParent, pPath, sizeof(aParent));
	if(!fs_parent_dir(aParent))
		return false;

#if defined(CONF_FAMILY_UNIX)
	return access(aParent, W_OK) == 0;
#elif defined(CONF_FAMILY_WINDOWS)
	DWORD Attr = GetFileAttributesA(aParent);
	if(Attr == INVALID_FILE_ATTRIBUTES)
		return false;
	if(Attr & FILE_ATTRIBUTE_READONLY)
		return false;
	return true;
#else
	char aTestDir[IO_MAX_PATH_LENGTH];
	str_format(aTestDir, sizeof(aTestDir), "%s/.preflight_create_test_%d", aParent, (int)pid());
	int Ret = fs_makedir(aTestDir);
	if(Ret == 0)
	{
		fs_remove(aTestDir);
		return true;
	}
	return false;
#endif
}

bool CPreflight::PathWritable(const char *pPath)
{
	if(!pPath || !pPath[0])
		return false;

	if(!fs_is_dir(pPath))
	{
		if(!PathExistsAndReadable(pPath))
		{
			return CanCreateDirectory(pPath);
		}
	}

#if defined(CONF_FAMILY_UNIX)
	return access(pPath, W_OK) == 0;
#elif defined(CONF_FAMILY_WINDOWS)
	DWORD Attr = GetFileAttributesA(pPath);
	if(Attr == INVALID_FILE_ATTRIBUTES)
		return false;
	if(Attr & FILE_ATTRIBUTE_READONLY)
		return false;
	return true;
#else
	char aTestFile[IO_MAX_PATH_LENGTH];
	str_format(aTestFile, sizeof(aTestFile), "%s/.preflight_write_test_%d", pPath, (int)pid());
	IOHANDLE f = io_open(aTestFile, IOFLAG_WRITE);
	if(f)
	{
		io_close(f);
		fs_remove(aTestFile);
		return true;
	}
	return false;
#endif
}

bool CPreflight::CheckPortAvailable(int Port)
{
	NETADDR Addr;
	mem_zero(&Addr, sizeof(Addr));
	Addr.type = NETTYPE_ALL;
	Addr.port = Port;

	NETSOCKET Sock = net_udp_create(Addr, 0);
	if(Sock.type == NETTYPE_INVALID)
		return false;

	net_udp_close(Sock);
	return true;
}

const char *CPreflight::GetSaveDir()
{
	static char s_aSaveDir[IO_MAX_PATH_LENGTH];
	s_aSaveDir[0] = 0;

	if(m_pStorage)
	{
		m_pStorage->GetCompletePath(IStorage::TYPE_SAVE, "", s_aSaveDir, sizeof(s_aSaveDir));
	}
	else if(m_aAppName[0])
	{
		fs_storage_path(m_aAppName, s_aSaveDir, sizeof(s_aSaveDir));
	}

	int Len = str_length(s_aSaveDir);
	while(Len > 1 && s_aSaveDir[Len - 1] == '/')
	{
		s_aSaveDir[Len - 1] = 0;
		Len--;
	}
	return s_aSaveDir;
}

const char *CPreflight::GetDataDir()
{
	static char s_aDataDir[IO_MAX_PATH_LENGTH];
	s_aDataDir[0] = 0;

	if(m_pStorage)
	{
		char aMarker[IO_MAX_PATH_LENGTH];
		if(m_pStorage->FindFile("maps/dm1.map", "data", IStorage::TYPE_ALL, aMarker, sizeof(aMarker)))
		{
			str_copy(s_aDataDir, aMarker, sizeof(s_aDataDir));
			for(int i = 0; i < 2; i++)
				fs_parent_dir(s_aDataDir);
			return s_aDataDir;
		}
		m_pStorage->GetCompletePath(IStorage::TYPE_ALL, "data", s_aDataDir, sizeof(s_aDataDir));
	}
	else
	{
		str_copy(s_aDataDir, "data", sizeof(s_aDataDir));
	}

	int Len = str_length(s_aDataDir);
	while(Len > 1 && s_aDataDir[Len - 1] == '/')
	{
		s_aDataDir[Len - 1] = 0;
		Len--;
	}
	return s_aDataDir;
}

int CPreflight::CheckNetwork()
{
	dbg_msg("preflight", "running network initialization check...");

	if(m_NetworkAlreadyInitialized)
	{
		AddResult(PRECHECK_NETWORK, PRESEVERITY_INFO,
			"Network subsystem already initialized (skipped re-init).", 0);
		return 0;
	}

	int Ret = net_init();
	if(Ret != 0)
	{
		AddResult(PRECHECK_NETWORK, PRESEVERITY_ERROR,
			"Network subsystem initialization failed.",
#if defined(CONF_FAMILY_WINDOWS)
			"Check that Winsock is properly installed. Try restarting your computer or reinstalling network drivers."
#else
			"Check your system's network configuration. Ensure socket creation is allowed and no firewall is blocking initialization."
#endif
		);
		return -1;
	}

	m_NetworkAlreadyInitialized = true;
	AddResult(PRECHECK_NETWORK, PRESEVERITY_INFO,
		"Network subsystem initialized successfully.", 0);
	return 0;
}

int CPreflight::CheckResourcePaths()
{
	dbg_msg("preflight", "running resource paths check...");

	int NumFailures = 0;
	const char *apDefaultSubdirs[] = {
		"audio",
		"maps",
		"mapres",
		"skins",
		"fonts",
		"languages",
		"ui",
	};

	if(m_pStorage)
	{
		for(unsigned i = 0; i < sizeof(apDefaultSubdirs)/sizeof(apDefaultSubdirs[0]); ++i)
		{
			char aMarker[IO_MAX_PATH_LENGTH];
			char aPath[IO_MAX_PATH_LENGTH];
			str_format(aPath, sizeof(aPath), "data/%s", apDefaultSubdirs[i]);
			bool Found = false;
			if(str_comp(apDefaultSubdirs[i], "maps") == 0)
				Found = m_pStorage->FindFile("dm1.map", aPath, IStorage::TYPE_ALL, aMarker, sizeof(aMarker));
			else if(str_comp(apDefaultSubdirs[i], "mapres") == 0)
				Found = m_pStorage->FindFile("grass_main.png", aPath, IStorage::TYPE_ALL, aMarker, sizeof(aMarker));
			else if(str_comp(apDefaultSubdirs[i], "skins") == 0)
				Found = m_pStorage->FindFile("default.png", aPath, IStorage::TYPE_ALL, aMarker, sizeof(aMarker));
			else if(str_comp(apDefaultSubdirs[i], "fonts") == 0)
				Found = m_pStorage->FindFile("DejaVuSans.ttf", aPath, IStorage::TYPE_ALL, aMarker, sizeof(aMarker));
			else if(str_comp(apDefaultSubdirs[i], "audio") == 0)
				Found = m_pStorage->FindFile("chat_msg.wav", aPath, IStorage::TYPE_ALL, aMarker, sizeof(aMarker));
			else if(str_comp(apDefaultSubdirs[i], "languages") == 0)
				Found = m_pStorage->FindFile("index.txt", aPath, IStorage::TYPE_ALL, aMarker, sizeof(aMarker));
			else if(str_comp(apDefaultSubdirs[i], "ui") == 0)
				Found = m_pStorage->FindFile("background.png", aPath, IStorage::TYPE_ALL, aMarker, sizeof(aMarker));
			else
			{
				m_pStorage->GetCompletePath(IStorage::TYPE_ALL, aPath, aMarker, sizeof(aMarker));
				Found = PathExistsAndReadable(aMarker);
			}

			if(!Found)
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "Missing or unreadable resource directory: data/%s (searched in storage paths)", apDefaultSubdirs[i]);
				AddResult(PRECHECK_RESOURCE_PATHS, PRESEVERITY_WARNING, aBuf,
					"Verify the data/ directory is complete and present in one of the storage search paths (application dir, user dir, current dir). "
					"Re-download the game data if files are missing."
				);
				NumFailures++;
			}
		}
	}
	else
	{
		const char *pDataDir = GetDataDir();
		if(!PathExistsAndReadable(pDataDir))
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "Data directory not found at '%s'.", pDataDir);
			AddResult(PRECHECK_RESOURCE_PATHS, PRESEVERITY_ERROR, aBuf,
				"1. Verify the 'data' directory exists in the same directory as the executable\n"
				"2. If running from build directory, make sure to copy or symlink the data directory:\n"
				"   ln -s /path/to/teeworlds/data data\n"
				"3. Check that the data directory has read permissions"
			);
			NumFailures++;
		}
		else
		{
			for(unsigned i = 0; i < sizeof(apDefaultSubdirs)/sizeof(apDefaultSubdirs[0]); ++i)
			{
				char aSubPath[IO_MAX_PATH_LENGTH];
				str_format(aSubPath, sizeof(aSubPath), "%s/%s", pDataDir, apDefaultSubdirs[i]);
				if(!PathExistsAndReadable(aSubPath))
				{
					char aBuf[512];
					str_format(aBuf, sizeof(aBuf), "Missing or unreadable resource subdirectory: '%s'", aSubPath);
					AddResult(PRECHECK_RESOURCE_PATHS, PRESEVERITY_WARNING, aBuf,
						"Verify the data directory is complete. Re-download the game data if files are missing."
					);
					NumFailures++;
				}
			}
		}
	}

	for(int i = 0; i < m_NumResourcePaths; ++i)
	{
		if(!PathExistsAndReadable(m_aaResourcePaths[i]))
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "Custom resource path not found or unreadable: '%s'", m_aaResourcePaths[i]);
			AddResult(PRECHECK_RESOURCE_PATHS, PRESEVERITY_ERROR, aBuf,
				"Check that the specified path exists and has read permissions."
			);
			NumFailures++;
		}
	}

	if(NumFailures == 0)
	{
		AddResult(PRECHECK_RESOURCE_PATHS, PRESEVERITY_INFO,
			"All resource paths are accessible.", 0);
	}

	return NumFailures == 0 ? 0 : -1;
}

int CPreflight::CheckServerPort()
{
	if(m_Mode != PREMODE_SERVER)
		return 0;

	dbg_msg("preflight", "running server port check...");

	int ServerPort = 8303;
	if(m_pConfig)
	{
		ServerPort = m_pConfig->m_SvPort;
	}

	if(ServerPort <= 0 || ServerPort > 65535)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Invalid port number: %d (from sv_port config). Port must be between 1 and 65535.", ServerPort);
		AddResult(PRECHECK_SERVER_PORT, PRESEVERITY_ERROR, aBuf,
			"Set sv_port to a valid value between 1 and 65535 in settings_ddnet.cfg (default: 8303)."
		);
		return -1;
	}

	if(ServerPort < 1024)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Port %d is a privileged port (< 1024).", ServerPort);
		AddResult(PRECHECK_SERVER_PORT, PRESEVERITY_WARNING, aBuf,
			"On Unix systems, privileged ports require root/sudo access. Consider setting sv_port >= 1024 (e.g., 8303)."
		);
	}

	if(!CheckPortAvailable(ServerPort))
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Port %d (sv_port) is already in use or cannot be bound.", ServerPort);
		AddResult(PRECHECK_SERVER_PORT, PRESEVERITY_ERROR, aBuf,
			"1. Check if another Teeworlds server is already running on this port\n"
			"2. Use 'lsof -i :PORT' or 'netstat -tulpn | grep PORT' to find the process\n"
			"   macOS:   lsof -i :8303\n"
			"   Linux:   sudo netstat -tulpn | grep 8303\n"
			"3. Change sv_port in settings_ddnet.cfg to a different, unused port"
		);
		return -1;
	}

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Server port %d (from sv_port config) is available.", ServerPort);
	AddResult(PRECHECK_SERVER_PORT, PRESEVERITY_INFO, aBuf, 0);
	return 0;
}

int CPreflight::CheckDemoMapPermissions()
{
	dbg_msg("preflight", "running demo/map file permissions check...");

	int NumFailures = 0;
	const char *pSaveDir = GetSaveDir();

	if(pSaveDir && pSaveDir[0])
	{
		struct
		{
			const char *m_pSubDir;
			bool m_IsError;
		} aDirs[] = {
			{"demos", true},
			{"demos/auto", false},
			{"maps", true},
			{"downloadedmaps", false},
		};

		for(unsigned i = 0; i < sizeof(aDirs)/sizeof(aDirs[0]); ++i)
		{
			char aFullPath[IO_MAX_PATH_LENGTH];
			str_format(aFullPath, sizeof(aFullPath), "%s/%s", pSaveDir, aDirs[i].m_pSubDir);

			bool Exists = fs_is_dir(aFullPath);
			if(Exists)
			{
				if(!PathWritable(aFullPath))
				{
					char aBuf[512];
					str_format(aBuf, sizeof(aBuf), "'%s' directory exists but is not writable: '%s'", aDirs[i].m_pSubDir, aFullPath);
					AddResult(PRECHECK_DEMO_MAP_PERMISSIONS,
						aDirs[i].m_IsError ? PRESEVERITY_ERROR : PRESEVERITY_WARNING, aBuf,
						"Fix directory permissions:\n"
						"  chmod u+w /path/to/dir\n"
						"Or check the ownership of the directory with: ls -la"
					);
					NumFailures++;
				}
			}
			else
			{
				if(!PathWritable(aFullPath))
				{
					char aBuf[512];
					str_format(aBuf, sizeof(aBuf), "'%s' directory does not exist and cannot be created at: '%s'", aDirs[i].m_pSubDir, aFullPath);
					AddResult(PRECHECK_DEMO_MAP_PERMISSIONS,
						aDirs[i].m_IsError ? PRESEVERITY_ERROR : PRESEVERITY_WARNING, aBuf,
						"Check parent directory permissions. The game should be able to create this subdirectory automatically:\n"
						"  chmod u+w /path/to/parent/dir"
					);
					NumFailures++;
				}
			}
		}
	}

	if(m_pStorage)
	{
		char aMapPath[IO_MAX_PATH_LENGTH];
		bool HasDm1 = m_pStorage->FindFile("dm1.map", "data/maps", IStorage::TYPE_ALL, aMapPath, sizeof(aMapPath));
		bool HasCtf1 = m_pStorage->FindFile("ctf1.map", "data/maps", IStorage::TYPE_ALL, aMapPath, sizeof(aMapPath));
		if(!HasDm1 || !HasCtf1)
		{
			AddResult(PRECHECK_DEMO_MAP_PERMISSIONS, PRESEVERITY_WARNING,
				"Standard map files (dm1.map, ctf1.map) not found in storage search paths.",
				"Ensure data/maps directory exists with the default .map files from the original Teeworlds distribution."
			);
			NumFailures++;
		}
	}
	else
	{
		const char *pDataDir = GetDataDir();
		if(pDataDir && pDataDir[0])
		{
			char aBuiltinMaps[IO_MAX_PATH_LENGTH];
			str_format(aBuiltinMaps, sizeof(aBuiltinMaps), "%s/maps", pDataDir);
			if(PathExistsAndReadable(aBuiltinMaps))
			{
				const char *apTestMaps[] = {"dm1.map", "ctf1.map"};
				for(unsigned i = 0; i < sizeof(apTestMaps)/sizeof(apTestMaps[0]); ++i)
				{
					char aMapPath[IO_MAX_PATH_LENGTH];
					str_format(aMapPath, sizeof(aMapPath), "%s/%s", aBuiltinMaps, apTestMaps[i]);
					if(!PathExistsAndReadable(aMapPath))
					{
						char aBuf[512];
						str_format(aBuf, sizeof(aBuf), "Standard map file missing or unreadable: '%s'", aMapPath);
						AddResult(PRECHECK_DEMO_MAP_PERMISSIONS, PRESEVERITY_WARNING, aBuf,
							"Reinstall or restore the missing map files from the original Teeworlds distribution."
						);
						NumFailures++;
					}
				}
			}
		}
	}

	if(NumFailures == 0)
	{
		AddResult(PRECHECK_DEMO_MAP_PERMISSIONS, PRESEVERITY_INFO,
			"Demo and map file permissions are correct.", 0);
	}

	return NumFailures == 0 ? 0 : -1;
}

int CPreflight::CheckConfigWrite()
{
	dbg_msg("preflight", "running config directory write check...");

	int NumFailures = 0;
	const char *pSaveDir = GetSaveDir();

	if(!pSaveDir || !pSaveDir[0])
	{
		AddResult(PRECHECK_CONFIG_WRITE, PRESEVERITY_ERROR,
			"Unable to determine user configuration directory path.",
			"Check that fs_storage_path() works correctly on your system and HOME/APPDATA is set."
		);
		return -1;
	}

	if(!fs_is_dir(pSaveDir))
	{
		if(!PathWritable(pSaveDir))
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "Configuration directory does not exist and cannot be created: '%s'", pSaveDir);
			AddResult(PRECHECK_CONFIG_WRITE, PRESEVERITY_ERROR, aBuf,
				"1. Check the parent directory permissions\n"
				"2. Ensure your home directory is writable\n"
				"3. Create the directory manually:\n"
				"   mkdir -p /path/to/config/dir"
			);
			return -1;
		}
	}
	else
	{
		if(!PathWritable(pSaveDir))
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "Configuration directory is not writable: '%s'", pSaveDir);
			AddResult(PRECHECK_CONFIG_WRITE, PRESEVERITY_ERROR, aBuf,
				"Fix directory permissions:\n"
				"  chmod u+w /path/to/config/dir\n"
				"  chown -R $(whoami) /path/to/config/dir"
			);
			NumFailures++;
		}
	}

	const char *apSubDirs[] = {"configs", "dumps"};
	for(unsigned i = 0; i < sizeof(apSubDirs)/sizeof(apSubDirs[0]); ++i)
	{
		char aFullPath[IO_MAX_PATH_LENGTH];
		str_format(aFullPath, sizeof(aFullPath), "%s/%s", pSaveDir, apSubDirs[i]);

		if(fs_is_dir(aFullPath))
		{
			if(!PathWritable(aFullPath))
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "'%s' subdirectory is not writable: '%s'", apSubDirs[i], aFullPath);
				AddResult(PRECHECK_CONFIG_WRITE, PRESEVERITY_ERROR, aBuf,
					"Fix directory permissions: chmod u+w /path/to/dir"
				);
				NumFailures++;
			}
		}
		else
		{
			if(!PathWritable(aFullPath))
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "'%s' subdirectory does not exist and cannot be created at: '%s'", apSubDirs[i], aFullPath);
				AddResult(PRECHECK_CONFIG_WRITE, PRESEVERITY_WARNING, aBuf,
					"Check parent directory permissions: chmod u+w /path/to/parent/dir"
				);
				NumFailures++;
			}
		}
	}

	char aTestFile[IO_MAX_PATH_LENGTH];
	str_format(aTestFile, sizeof(aTestFile), "%s/.preflight_write_test", pSaveDir);
	IOHANDLE f = io_open(aTestFile, IOFLAG_WRITE);
	if(f)
	{
		const char *pTest = "preflight test";
		io_write(f, pTest, str_length(pTest));
		io_close(f);
		fs_remove(aTestFile);
	}
	else
	{
		if(fs_is_dir(pSaveDir))
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "Failed to write test file in existing config directory: '%s'", pSaveDir);
			AddResult(PRECHECK_CONFIG_WRITE, PRESEVERITY_ERROR, aBuf,
				"Check that your disk is not full and the directory has write permissions."
			);
			NumFailures++;
		}
	}

	if(NumFailures == 0)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Configuration directory is writable: '%s'", pSaveDir);
		AddResult(PRECHECK_CONFIG_WRITE, PRESEVERITY_INFO, aBuf, 0);
	}

	return NumFailures == 0 ? 0 : -1;
}

int CPreflight::RunAllChecks()
{
	int TotalErrors = 0;

	dbg_msg("preflight", "=== starting preflight checks ===");
	dbg_msg("preflight", "mode: %s", ModeName());
	dbg_msg("preflight", "using %s storage, %s config",
		m_pStorage ? "initialized" : "fallback",
		m_pConfig ? "loaded" : "default"
	);

	if(m_aCheckEnabled[PRECHECK_NETWORK])
	{
		if(CheckNetwork() != 0)
			TotalErrors++;
	}

	if(m_aCheckEnabled[PRECHECK_RESOURCE_PATHS])
	{
		if(CheckResourcePaths() != 0)
			TotalErrors++;
	}

	if(m_aCheckEnabled[PRECHECK_SERVER_PORT])
	{
		if(CheckServerPort() != 0)
			TotalErrors++;
	}

	if(m_aCheckEnabled[PRECHECK_DEMO_MAP_PERMISSIONS])
	{
		if(CheckDemoMapPermissions() != 0)
			TotalErrors++;
	}

	if(m_aCheckEnabled[PRECHECK_CONFIG_WRITE])
	{
		if(CheckConfigWrite() != 0)
			TotalErrors++;
	}

	for(int i = 0; i < m_NumCustomChecks; ++i)
	{
		if(m_aCustomChecks[i].m_pfnCheck)
		{
			if(m_aCustomChecks[i].m_pfnCheck(this, m_aCustomChecks[i].m_pUser) != 0)
				TotalErrors++;
		}
	}

	dbg_msg("preflight", "=== preflight complete: %d error(s), %d warning(s) ===",
		HasErrors() ? 1 : 0,
		HasWarnings() ? 1 : 0);

	PrintResults();

	return TotalErrors;
}

IPreflight *CreatePreflight()
{
	return new CPreflight;
}

bool PreflightShouldSkip(int argc, const char **argv)
{
	for(int i = 1; i < argc; i++)
	{
		if(str_comp(argv[i], "--no-preflight") == 0)
			return true;
	}
	return false;
}

int PreflightInitAndRun(const char *pAppName, EPreflightMode Mode, int argc, const char **argv,
	SPreflightContext *pOutContext,
	FPreflightCustomCheck pfnCustomCheckA, void *pUserA,
	FPreflightCustomCheck pfnCustomCheckB, void *pUserB)
{
	int FlagMask = CFGFLAG_CLIENT | CFGFLAG_SERVER;
	int StorageType = IStorage::STORAGETYPE_BASIC;
	switch(Mode)
	{
	case PREMODE_CLIENT:
		FlagMask = CFGFLAG_CLIENT;
		StorageType = IStorage::STORAGETYPE_CLIENT;
		break;
	case PREMODE_SERVER:
		FlagMask = CFGFLAG_SERVER;
		StorageType = IStorage::STORAGETYPE_SERVER;
		break;
	case PREMODE_TOOL:
	default:
		break;
	}

	IEngine *pEngine = CreateEngine(pAppName);
	IStorage *pStorage = CreateStorage(pAppName, StorageType, argc, argv);
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
		dbg_msg("preflight", "failed to register core interfaces.");
		delete pKernel;
		delete pEngine;
		delete pStorage;
		delete pConsole;
		delete pConfigManager;
		return -1;
	}

	pEngine->Init();
	pConfigManager->Init(FlagMask);
	pConsole->Init();

	IPreflight *pPreflight = CreatePreflight();
	pPreflight->SetMode(Mode);
	pPreflight->SetAppName(pAppName);
	pPreflight->SetStorage(pStorage);
	pPreflight->SetConfig(pConfigManager->Values());
	pPreflight->SetNetworkAlreadyInitialized();

	switch(Mode)
	{
	case PREMODE_CLIENT:
		pPreflight->DisableCheck(PRECHECK_SERVER_PORT);
		break;
	case PREMODE_SERVER:
		pPreflight->DisableCheck(PRECHECK_GRAPHICS);
		pPreflight->DisableCheck(PRECHECK_AUDIO);
		break;
	case PREMODE_TOOL:
	default:
		pPreflight->DisableCheck(PRECHECK_GRAPHICS);
		pPreflight->DisableCheck(PRECHECK_AUDIO);
		pPreflight->DisableCheck(PRECHECK_SERVER_PORT);
		break;
	}

	if(pfnCustomCheckA)
		pPreflight->RegisterCustomCheck(pfnCustomCheckA, pUserA);
	if(pfnCustomCheckB)
		pPreflight->RegisterCustomCheck(pfnCustomCheckB, pUserB);

	int NumErrors = pPreflight->RunAllChecks();
	bool HasErrors = pPreflight->HasErrors();

	if(pOutContext)
	{
		pOutContext->m_pKernel = pKernel;
		pOutContext->m_pEngine = pEngine;
		pOutContext->m_pStorage = pStorage;
		pOutContext->m_pConsole = pConsole;
		pOutContext->m_pConfigManager = pConfigManager;
		pOutContext->m_pPreflight = pPreflight;
		pOutContext->m_OwnsInstances = true;
	}
	else
	{
		delete pPreflight;
		delete pKernel;
		delete pEngine;
		delete pStorage;
		delete pConsole;
		delete pConfigManager;
	}

	return HasErrors ? -1 : NumErrors;
}

void PreflightShutdown(SPreflightContext *pContext)
{
	if(!pContext || !pContext->m_OwnsInstances)
		return;

	delete pContext->m_pPreflight;
	delete pContext->m_pKernel;
	delete pContext->m_pEngine;
	delete pContext->m_pStorage;
	delete pContext->m_pConsole;
	delete pContext->m_pConfigManager;

	pContext->m_pPreflight = 0;
	pContext->m_pKernel = 0;
	pContext->m_pEngine = 0;
	pContext->m_pStorage = 0;
	pContext->m_pConsole = 0;
	pContext->m_pConfigManager = 0;
	pContext->m_OwnsInstances = false;
}
