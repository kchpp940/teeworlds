/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_PREFLIGHT_H
#define ENGINE_SHARED_PREFLIGHT_H

#include <engine/preflight.h>

class CPreflight : public IPreflight
{
	enum
	{
		MAX_RESOURCE_PATHS_INTERNAL = 16,
	};

	struct SCustomCheck
	{
		FPreflightCustomCheck m_pfnCheck;
		void *m_pUser;
	};

	bool m_aCheckEnabled[PRECHECK_COUNT];
	bool m_ClientMode;
	bool m_ServerMode;
	int m_ServerPort;
	char m_aAppName[128];
	char m_aaResourcePaths[MAX_RESOURCE_PATHS_INTERNAL][IO_MAX_PATH_LENGTH];
	int m_NumResourcePaths;
	SCustomCheck m_aCustomChecks[MAX_CUSTOM_CHECKS];
	int m_NumCustomChecks;

	SPreflightResult m_aResults[MAX_RESULTS];
	int m_NumResults;

	const char *CheckName(EPreflightCheck Check) const;

	int CheckNetwork();
	int CheckResourcePaths(int argc, const char **argv);
	int CheckServerPort();
	int CheckDemoMapPermissions(int argc, const char **argv);
	int CheckConfigWrite(int argc, const char **argv);

	bool PathExistsAndReadable(const char *pPath);
	bool PathWritable(const char *pPath);
	bool CheckPortAvailable(int Port);

public:
	CPreflight();

	virtual void Reset();
	virtual void EnableCheck(EPreflightCheck Check);
	virtual void DisableCheck(EPreflightCheck Check);
	virtual void SetClientMode();
	virtual void SetServerMode();
	virtual void SetServerPort(int Port);
	virtual void SetAppName(const char *pAppName);
	virtual void AddResourcePath(const char *pPath);
	virtual void RegisterCustomCheck(FPreflightCustomCheck pfnCheck, void *pUser);
	virtual void AddResult(EPreflightCheck Check, EPreflightSeverity Severity, const char *pMessage, const char *pFixSuggestion);
	virtual int RunAllChecks(int argc, const char **argv);
	virtual int ResultCount() const { return m_NumResults; }
	virtual const SPreflightResult *GetResult(int Index) const;
	virtual bool HasErrors() const;
	virtual bool HasWarnings() const;
	virtual void PrintResults();
};

#endif
