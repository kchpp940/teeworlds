/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CONFIG_H
#define ENGINE_CONFIG_H

#include "kernel.h"

class IConfigManager : public IInterface
{
	MACRO_INTERFACE("config", 0)
public:
	typedef void (*SAVECALLBACKFUNC)(IConfigManager *pConfigManager, void *pUserData);

	enum ECorrectionReason
	{
		CORRECTION_RANGE_MIN = 0,
		CORRECTION_RANGE_MAX,
		CORRECTION_EMPTY_STRING,
		CORRECTION_MISSING_FIELD,
		CORRECTION_DEPRECATED_REDIRECT,
		CORRECTION_INVALID_UTF8,
	};

	struct CCorrection
	{
		ECorrectionReason m_Reason;
		const char *m_pFieldName;
		char m_aOldValue[128];
		char m_aNewValue[128];
	};

	virtual void Init(int FlagMask) = 0;
	virtual void Reset() = 0;
	virtual void RestoreStrings() = 0;
	virtual void Save(const char *pFilename=0) = 0;
	virtual class CConfig *Values() = 0;

	virtual void Validate() = 0;
	virtual void Upgrade() = 0;
	virtual bool Load(const char *pFilename=0) = 0;

	virtual bool SetInt(const char *pScriptName, int Value) = 0;
	virtual bool SetStr(const char *pScriptName, const char *pValue) = 0;
	virtual bool GetInt(const char *pScriptName, int *pOutValue) const = 0;
	virtual bool GetStr(const char *pScriptName, char *pOutValue, int OutSize) const = 0;

	virtual int NumCorrections() const = 0;
	virtual const CCorrection *GetCorrection(int Index) const = 0;
	virtual void ClearCorrections() = 0;

	virtual void RegisterDeprecated(const char *pOldScriptName, const char *pNewScriptName) = 0;

	virtual int NumMissingFields() const = 0;
	virtual const char *GetMissingField(int Index) const = 0;

	virtual void RegisterCallback(SAVECALLBACKFUNC pfnFunc, void *pUserData) = 0;

	virtual void WriteLine(const char *pLine) = 0;
};

extern IConfigManager *CreateConfigManager();

#endif
