/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_CONFIG_H
#define ENGINE_SHARED_CONFIG_H

#include <engine/config.h>
#include <engine/console.h>
#include "protocol.h"

class CConfig
{
public:
	#define MACRO_CONFIG_INT(Name,ScriptName,Def,Min,Max,Save,Desc) int m_##Name;
	#define MACRO_CONFIG_STR(Name,ScriptName,Len,Def,Save,Desc) char m_##Name[Len]; // Flawfinder: ignore
	#define MACRO_CONFIG_UTF8STR(Name,ScriptName,Size,Len,Def,Save,Desc) char m_##Name[Size]; // Flawfinder: ignore

	#include "config_variables.h"
	
	#undef MACRO_CONFIG_INT
	#undef MACRO_CONFIG_STR
	#undef MACRO_CONFIG_UTF8STR
};

enum
{
	CFGFLAG_SAVE=1,
	CFGFLAG_CLIENT=2,
	CFGFLAG_SERVER=4,
	CFGFLAG_STORE=8,
	CFGFLAG_MASTER=16,
	CFGFLAG_ECON=32,
	CFGFLAG_BASICACCESS=64,
};

class CConfigManager : public IConfigManager
{
	enum
	{
		MAX_CALLBACKS = 16,
		MAX_META = 256,
		MAX_DEPRECATED = 64,
		MAX_CORRECTIONS = 256,
		MAX_MISSING = 256,
	};

	struct CCallback
	{
		SAVECALLBACKFUNC m_pfnFunc;
		void *m_pUserData;
	};

	struct CConfigMeta
	{
		enum EType
		{
			TYPE_INT = 0,
			TYPE_STR,
			TYPE_UTF8STR
		};

		EType m_Type;
		const char *m_pScriptName;
		int m_Offset;
		int m_Size;
		int m_DefaultInt;
		const char *m_pDefaultStr;
		int m_Min;
		int m_Max;
		int m_Flags;
	};

	struct CDeprecatedMapping
	{
		const char *m_pOldName;
		const char *m_pNewName;
	};

	CCorrection m_aCorrections[MAX_CORRECTIONS];
	int m_NumCorrections;

	const char *m_apMissingFields[MAX_MISSING];
	int m_NumMissingFields;

	CDeprecatedMapping m_aDeprecated[MAX_DEPRECATED];
	int m_NumDeprecated;

	unsigned char m_aFieldLoaded[MAX_META];
	int m_NumTrackedFields;

	class IStorage *m_pStorage;
	class IConsole *m_pConsole;
	IOHANDLE m_ConfigFile;
	int m_FlagMask;
	CCallback m_aCallbacks[MAX_CALLBACKS];
	int m_NumCallbacks;
	CConfig m_Values;

	const CConfigMeta *m_pMetaTable;
	int m_NumMeta;

	static void InitMetaTable(const CConfigMeta **ppTable, int *pNum);
	bool FindMetaByName(const char *pScriptName, CConfigMeta *pOut) const;
	bool RedirectDeprecated(const char *pScriptName, char *pRedirectBuffer, int BufferSize) const;
	void AddCorrection(ECorrectionReason Reason, const char *pFieldName, const char *pOldValue, const char *pNewValue);
	void MarkFieldLoaded(const char *pScriptName);
	void DetectMissingFields();
	int ClampIntValue(const CConfigMeta &Meta, int Value, bool *pCorrected = 0);
	const char *ClampStrValue(const CConfigMeta &Meta, const char *pValue, char *pBuffer, int BufferSize, bool *pCorrected = 0);

	static void ConSaveConfig(IConsole::IResult *pResult, void *pUserData);
	static void ConSetConfig(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpConfig(IConsole::IResult *pResult, void *pUserData);
	static void ConResetConfig(IConsole::IResult *pResult, void *pUserData);

public:
	CConfigManager();

	virtual void Init(int FlagMask);
	virtual void Reset();
	virtual void RestoreStrings();
	virtual void Save(const char *pFilename);
	virtual CConfig *Values() { return &m_Values; }

	virtual void Validate();
	virtual void Upgrade();
	virtual bool Load(const char *pFilename);

	virtual bool SetInt(const char *pScriptName, int Value);
	virtual bool SetStr(const char *pScriptName, const char *pValue);
	virtual bool GetInt(const char *pScriptName, int *pOutValue) const;
	virtual bool GetStr(const char *pScriptName, char *pOutValue, int OutSize) const;

	virtual int NumCorrections() const { return m_NumCorrections; }
	virtual const CCorrection *GetCorrection(int Index) const;
	virtual void ClearCorrections() { m_NumCorrections = 0; }

	virtual void RegisterDeprecated(const char *pOldScriptName, const char *pNewScriptName);

	virtual int NumMissingFields() const { return m_NumMissingFields; }
	virtual const char *GetMissingField(int Index) const;

	virtual void RegisterCallback(SAVECALLBACKFUNC pfnFunc, void *pUserData);

	virtual void WriteLine(const char *pLine);
};

#endif
