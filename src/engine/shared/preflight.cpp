/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <base/math.h>
#include <base/system.h>

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
	m_ClientMode = false;
	m_ServerMode = false;
	m_ServerPort = 8303;
	m_aAppName[0] = 0;
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

void CPreflight::SetClientMode()
{
	m_ClientMode = true;
	m_ServerMode = false;
}

void CPreflight::SetServerMode()
{
	m_ClientMode = false;
	m_ServerMode = true;
}

void CPreflight::SetServerPort(int Port)
{
	m_ServerPort = clamp(Port, 1, 65535);
}

void CPreflight::SetAppName(const char *pAppName)
{
	str_copy(m_aAppName, pAppName, sizeof(m_aAppName));
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

bool CPreflight::PathWritable(const char *pPath)
{
	if(!pPath || !pPath[0])
		return false;

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

int CPreflight::CheckNetwork()
{
	dbg_msg("preflight", "running network initialization check...");

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

	AddResult(PRECHECK_NETWORK, PRESEVERITY_INFO,
		"Network subsystem initialized successfully.", 0);
	return 0;
}

int CPreflight::CheckResourcePaths(int argc, const char **argv)
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

	char aAppDir[IO_MAX_PATH_LENGTH] = {0};
	char aDataDir[IO_MAX_PATH_LENGTH] = {0};

	if(argc > 0 && argv && argv[0])
	{
		str_copy(aAppDir, argv[0], sizeof(aAppDir));
		fs_parent_dir(aAppDir);
	}

	if(aAppDir[0])
	{
		str_format(aDataDir, sizeof(aDataDir), "%s/data", aAppDir);
	}
	else
	{
		str_copy(aDataDir, "data", sizeof(aDataDir));
	}

	if(!PathExistsAndReadable(aDataDir))
	{
		char aCwdData[IO_MAX_PATH_LENGTH];
		fs_getcwd(aCwdData, sizeof(aCwdData));
		str_append(aCwdData, "/data", sizeof(aCwdData));

		if(!PathExistsAndReadable(aCwdData))
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "Data directory not found at '%s' or '%s'.", aDataDir, aCwdData);
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
			str_copy(aDataDir, aCwdData, sizeof(aDataDir));
		}
	}

	if(NumFailures == 0 && aDataDir[0])
	{
		for(unsigned i = 0; i < sizeof(apDefaultSubdirs)/sizeof(apDefaultSubdirs[0]); ++i)
		{
			char aSubPath[IO_MAX_PATH_LENGTH];
			str_format(aSubPath, sizeof(aSubPath), "%s/%s", aDataDir, apDefaultSubdirs[i]);
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
	if(!m_ServerMode)
		return 0;

	dbg_msg("preflight", "running server port check...");

	if(m_ServerPort <= 0 || m_ServerPort > 65535)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Invalid port number: %d. Port must be between 1 and 65535.", m_ServerPort);
		AddResult(PRECHECK_SERVER_PORT, PRESEVERITY_ERROR, aBuf,
			"Set sv_port to a valid value between 1 and 65535 (default: 8303)."
		);
		return -1;
	}

	if(m_ServerPort < 1024)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Port %d is a privileged port (< 1024).", m_ServerPort);
		AddResult(PRECHECK_SERVER_PORT, PRESEVERITY_WARNING, aBuf,
			"On Unix systems, privileged ports require root/sudo access. Consider using a port >= 1024 (e.g., 8303)."
		);
	}

	if(!CheckPortAvailable(m_ServerPort))
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Port %d is already in use or cannot be bound.", m_ServerPort);
		AddResult(PRECHECK_SERVER_PORT, PRESEVERITY_ERROR, aBuf,
			"1. Check if another Teeworlds server is already running on this port\n"
			"2. Use 'lsof -i :PORT' or 'netstat -tulpn | grep PORT' to find the process\n"
			"   macOS:   lsof -i :8303\n"
			"   Linux:   sudo netstat -tulpn | grep 8303\n"
			"3. Change sv_port to a different, unused port"
		);
		return -1;
	}

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Server port %d is available.", m_ServerPort);
	AddResult(PRECHECK_SERVER_PORT, PRESEVERITY_INFO, aBuf, 0);
	return 0;
}

int CPreflight::CheckDemoMapPermissions(int argc, const char **argv)
{
	dbg_msg("preflight", "running demo/map file permissions check...");

	int NumFailures = 0;

	char aUserDir[IO_MAX_PATH_LENGTH] = {0};
	if(m_aAppName[0])
	{
		fs_storage_path(m_aAppName, aUserDir, sizeof(aUserDir));
	}

	if(aUserDir[0])
	{
		char aDemosDir[IO_MAX_PATH_LENGTH];
		char aMapsDir[IO_MAX_PATH_LENGTH];
		str_format(aDemosDir, sizeof(aDemosDir), "%s/demos", aUserDir);
		str_format(aMapsDir, sizeof(aMapsDir), "%s/maps", aUserDir);

		if(fs_is_dir(aDemosDir))
		{
			if(!PathWritable(aDemosDir))
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "Demos directory is not writable: '%s'", aDemosDir);
				AddResult(PRECHECK_DEMO_MAP_PERMISSIONS, PRESEVERITY_ERROR, aBuf,
					"Fix directory permissions:\n"
					"  chmod u+w /path/to/demos\n"
					"Or check the ownership of the directory with: ls -la"
				);
				NumFailures++;
			}
		}

		if(fs_is_dir(aMapsDir))
		{
			if(!PathWritable(aMapsDir))
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "Downloaded maps directory is not writable: '%s'", aMapsDir);
				AddResult(PRECHECK_DEMO_MAP_PERMISSIONS, PRESEVERITY_ERROR, aBuf,
					"Fix directory permissions:\n"
					"  chmod u+w /path/to/maps\n"
					"Or check the ownership of the directory with: ls -la"
				);
				NumFailures++;
			}
		}

		char aAutoDemosDir[IO_MAX_PATH_LENGTH];
		str_format(aAutoDemosDir, sizeof(aAutoDemosDir), "%s/demos/auto", aUserDir);
		if(fs_is_dir(aAutoDemosDir) && !PathWritable(aAutoDemosDir))
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "Auto demos directory is not writable: '%s'", aAutoDemosDir);
			AddResult(PRECHECK_DEMO_MAP_PERMISSIONS, PRESEVERITY_WARNING, aBuf,
				"Fix directory permissions: chmod u+w /path/to/demos/auto"
			);
			NumFailures++;
		}
	}

	char aAppDir[IO_MAX_PATH_LENGTH] = {0};
	if(argc > 0 && argv && argv[0])
	{
		str_copy(aAppDir, argv[0], sizeof(aAppDir));
		fs_parent_dir(aAppDir);
	}

	if(aAppDir[0])
	{
		char aBuiltinMaps[IO_MAX_PATH_LENGTH];
		str_format(aBuiltinMaps, sizeof(aBuiltinMaps), "%s/data/maps", aAppDir);
		if(!PathExistsAndReadable(aBuiltinMaps))
		{
			char aCwdMaps[IO_MAX_PATH_LENGTH];
			fs_getcwd(aCwdMaps, sizeof(aCwdMaps));
			str_append(aCwdMaps, "/data/maps", sizeof(aCwdMaps));

			if(!PathExistsAndReadable(aCwdMaps))
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "Builtin maps directory not found or unreadable at '%s' or '%s'", aBuiltinMaps, aCwdMaps);
				AddResult(PRECHECK_DEMO_MAP_PERMISSIONS, PRESEVERITY_WARNING, aBuf,
					"Ensure data/maps directory exists with the default .map files (dm1, ctf1, etc.)."
				);
				NumFailures++;
			}
		}
		else
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

	if(NumFailures == 0)
	{
		AddResult(PRECHECK_DEMO_MAP_PERMISSIONS, PRESEVERITY_INFO,
			"Demo and map file permissions are correct.", 0);
	}

	return NumFailures == 0 ? 0 : -1;
}

int CPreflight::CheckConfigWrite(int argc, const char **argv)
{
	dbg_msg("preflight", "running config directory write check...");

	int NumFailures = 0;

	char aUserDir[IO_MAX_PATH_LENGTH] = {0};
	if(m_aAppName[0])
	{
		fs_storage_path(m_aAppName, aUserDir, sizeof(aUserDir));
	}

	if(!aUserDir[0])
	{
		AddResult(PRECHECK_CONFIG_WRITE, PRESEVERITY_ERROR,
			"Unable to determine user configuration directory path.",
			"Check that fs_storage_path() works correctly on your system and HOME/APPDATA is set."
		);
		return -1;
	}

	if(!fs_is_dir(aUserDir))
	{
		int Ret = fs_makedir_recursive(aUserDir);
		if(Ret != 0)
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "Failed to create configuration directory: '%s'", aUserDir);
			AddResult(PRECHECK_CONFIG_WRITE, PRESEVERITY_ERROR, aBuf,
				"1. Check the parent directory permissions\n"
				"2. Ensure your home directory is writable\n"
				"3. Create the directory manually:\n"
				"   mkdir -p /path/to/config/dir"
			);
			return -1;
		}
	}

	if(!PathWritable(aUserDir))
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Configuration directory is not writable: '%s'", aUserDir);
		AddResult(PRECHECK_CONFIG_WRITE, PRESEVERITY_ERROR, aBuf,
			"Fix directory permissions:\n"
			"  chmod u+w /path/to/config/dir\n"
			"  chown -R $(whoami) /path/to/config/dir"
		);
		NumFailures++;
	}

	char aConfigsDir[IO_MAX_PATH_LENGTH];
	str_format(aConfigsDir, sizeof(aConfigsDir), "%s/configs", aUserDir);
	if(fs_is_dir(aConfigsDir) && !PathWritable(aConfigsDir))
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Settings subdirectory is not writable: '%s'", aConfigsDir);
		AddResult(PRECHECK_CONFIG_WRITE, PRESEVERITY_ERROR, aBuf,
			"Fix directory permissions: chmod u+w /path/to/configs"
		);
		NumFailures++;
	}

	char aDumpsDir[IO_MAX_PATH_LENGTH];
	str_format(aDumpsDir, sizeof(aDumpsDir), "%s/dumps", aUserDir);
	if(fs_is_dir(aDumpsDir) && !PathWritable(aDumpsDir))
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Dumps directory is not writable: '%s'", aDumpsDir);
		AddResult(PRECHECK_CONFIG_WRITE, PRESEVERITY_WARNING, aBuf,
			"Fix directory permissions: chmod u+w /path/to/dumps"
		);
		NumFailures++;
	}

	char aTestFile[IO_MAX_PATH_LENGTH];
	str_format(aTestFile, sizeof(aTestFile), "%s/.preflight_write_test", aUserDir);
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
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Failed to write test file in config directory: '%s'", aUserDir);
		AddResult(PRECHECK_CONFIG_WRITE, PRESEVERITY_ERROR, aBuf,
			"Check that your disk is not full and the directory has write permissions."
		);
		NumFailures++;
	}

	if(NumFailures == 0)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Configuration directory is writable: '%s'", aUserDir);
		AddResult(PRECHECK_CONFIG_WRITE, PRESEVERITY_INFO, aBuf, 0);
	}

	return NumFailures == 0 ? 0 : -1;
}

int CPreflight::RunAllChecks(int argc, const char **argv)
{
	int TotalErrors = 0;

	dbg_msg("preflight", "=== starting preflight checks ===");
	dbg_msg("preflight", "mode: %s", m_ClientMode ? "client" : (m_ServerMode ? "server" : "tool"));

	if(m_aCheckEnabled[PRECHECK_NETWORK])
	{
		if(CheckNetwork() != 0)
			TotalErrors++;
	}

	if(m_aCheckEnabled[PRECHECK_RESOURCE_PATHS])
	{
		if(CheckResourcePaths(argc, argv) != 0)
			TotalErrors++;
	}

	if(m_aCheckEnabled[PRECHECK_SERVER_PORT])
	{
		if(CheckServerPort() != 0)
			TotalErrors++;
	}

	if(m_aCheckEnabled[PRECHECK_DEMO_MAP_PERMISSIONS])
	{
		if(CheckDemoMapPermissions(argc, argv) != 0)
			TotalErrors++;
	}

	if(m_aCheckEnabled[PRECHECK_CONFIG_WRITE])
	{
		if(CheckConfigWrite(argc, argv) != 0)
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
