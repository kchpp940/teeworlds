/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_PREFLIGHT_H
#define ENGINE_PREFLIGHT_H

#include "kernel.h"

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

struct SPreflightResult
{
	EPreflightCheck m_Check;
	EPreflightSeverity m_Severity;
	char m_aMessage[512];
	char m_aFixSuggestion[512];
};

typedef int (*FPreflightCustomCheck)(class IPreflight *pPreflight, void *pUser);

class IPreflight : public IInterface
{
	MACRO_INTERFACE("preflight", 0)
public:
	enum
	{
		MAX_RESULTS = 32,
		MAX_RESOURCE_PATHS = 8,
		MAX_CUSTOM_CHECKS = 4,
	};

	virtual void Reset() = 0;
	virtual void EnableCheck(EPreflightCheck Check) = 0;
	virtual void DisableCheck(EPreflightCheck Check) = 0;
	virtual void SetClientMode() = 0;
	virtual void SetServerMode() = 0;
	virtual void SetServerPort(int Port) = 0;
	virtual void SetAppName(const char *pAppName) = 0;
	virtual void AddResourcePath(const char *pPath) = 0;
	virtual void RegisterCustomCheck(FPreflightCustomCheck pfnCheck, void *pUser) = 0;
	virtual void AddResult(EPreflightCheck Check, EPreflightSeverity Severity, const char *pMessage, const char *pFixSuggestion) = 0;
	virtual int RunAllChecks(int argc, const char **argv) = 0;
	virtual int ResultCount() const = 0;
	virtual const SPreflightResult *GetResult(int Index) const = 0;
	virtual bool HasErrors() const = 0;
	virtual bool HasWarnings() const = 0;
	virtual void PrintResults() = 0;
};

extern IPreflight *CreatePreflight();

#endif
