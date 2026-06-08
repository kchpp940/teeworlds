/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_PREFLIGHT_H
#define ENGINE_PREFLIGHT_H

#include "kernel.h"

class IStorage;
class IEngine;
class IConsole;
class IConfigManager;
class CConfig;

enum EPreflightCheck
{
	PRECHECK_NETWORK = 0,
	PRECHECK_GRAPHICS,
	PRECHECK_AUDIO,
	PRECHECK_RESOURCE_PATHS,
	PRECHECK_SERVER_PORT,
	PRECHECK_DEMO_MAP_PERMISSIONS,
	PRECHECK_CONFIG_WRITE,
	PRECHECK_COUNT,
};

enum EPreflightSeverity
{
	PRESEVERITY_INFO = 0,
	PRESEVERITY_WARNING,
	PRESEVERITY_ERROR,
};

enum EPreflightMode
{
	PREMODE_TOOL = 0,
	PREMODE_CLIENT,
	PREMODE_SERVER,
};

struct SPreflightResult
{
	EPreflightCheck m_Check;
	EPreflightSeverity m_Severity;
	char m_aMessage[512];
	char m_aFixSuggestion[512];
};

typedef int (*FPreflightCustomCheck)(class IPreflight *pPreflight, void *pUser);

struct SPreflightPathCheck
{
	const char *m_pPath;
	bool m_IsDir;
	bool m_RequireWrite;
	const char *m_pDescription;
};

struct SPreflightContext
{
	class IKernel *m_pKernel;
	class IEngine *m_pEngine;
	class IStorage *m_pStorage;
	class IConsole *m_pConsole;
	class IConfigManager *m_pConfigManager;
	class IPreflight *m_pPreflight;
	bool m_OwnsInstances;
};

class IPreflight : public IInterface
{
	MACRO_INTERFACE("preflight", 0)
public:
	enum
	{
		MAX_RESULTS = 32,
		MAX_RESOURCE_PATHS = 8,
		MAX_CUSTOM_CHECKS = 4,
		MAX_PATH_CHECKS = 16,
	};

	virtual void Reset() = 0;
	virtual void EnableCheck(EPreflightCheck Check) = 0;
	virtual void DisableCheck(EPreflightCheck Check) = 0;
	virtual void SetMode(EPreflightMode Mode) = 0;
	virtual void SetClientMode() = 0;
	virtual void SetServerMode() = 0;
	virtual void SetToolMode() = 0;
	virtual void SetAppName(const char *pAppName) = 0;
	virtual void SetStorage(IStorage *pStorage) = 0;
	virtual void SetConfig(CConfig *pConfig) = 0;
	virtual void SetNetworkAlreadyInitialized() = 0;
	virtual void AddResourcePath(const char *pPath) = 0;
	virtual void AddPathCheck(const char *pPath, bool IsDir, bool RequireWrite, const char *pDescription) = 0;
	virtual void RegisterCustomCheck(FPreflightCustomCheck pfnCheck, void *pUser) = 0;
	virtual void AddResult(EPreflightCheck Check, EPreflightSeverity Severity, const char *pMessage, const char *pFixSuggestion) = 0;
	virtual int RunAllChecks() = 0;
	virtual int ResultCount() const = 0;
	virtual const SPreflightResult *GetResult(int Index) const = 0;
	virtual bool HasErrors() const = 0;
	virtual bool HasWarnings() const = 0;
	virtual void PrintResults() = 0;
};

extern IPreflight *CreatePreflight();

extern FPreflightCustomCheck g_pfnPreflightSDLGraphicsCheck;
extern FPreflightCustomCheck g_pfnPreflightSDLAudioCheck;

bool PreflightShouldSkip(int argc, const char **argv);

void PreflightConfigure(IPreflight *pPreflight, EPreflightMode Mode, const char *pAppName,
	IStorage *pStorage, CConfig *pConfig, bool NetworkAlreadyInitialized);

int PreflightInitAndRun(const char *pAppName, EPreflightMode Mode, int argc, const char **argv,
	SPreflightContext *pOutContext = 0,
	FPreflightCustomCheck pfnCustomCheckA = 0, void *pUserA = 0,
	FPreflightCustomCheck pfnCustomCheckB = 0, void *pUserB = 0,
	const SPreflightPathCheck *pExtraPathChecks = 0, int NumExtraPathChecks = 0);

void PreflightShutdown(SPreflightContext *pContext);

int PreflightRunForClient(int argc, const char **argv,
	SPreflightContext *pOutContext = 0,
	FPreflightCustomCheck pfnGraphicsCheck = 0,
	FPreflightCustomCheck pfnAudioCheck = 0);

int PreflightRunForServer(int argc, const char **argv,
	SPreflightContext *pOutContext = 0);

int PreflightRunForTool(int argc, const char **argv,
	SPreflightContext *pOutContext = 0,
	const SPreflightPathCheck *pExtraPathChecks = 0, int NumExtraPathChecks = 0);

#endif
