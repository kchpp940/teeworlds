/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/config.h>
#include <engine/console.h>
#include <engine/storage.h>
#include <engine/shared/config.h>
#include <game/version.h>

#include <cstddef>


void EscapeParam(char *pDst, const char *pSrc, int size)
{
	for(int i = 0; *pSrc && i < size - 1; ++i)
	{
		if(*pSrc == '"' || *pSrc == '\\') // escape \ and "
			*pDst++ = '\\';
		*pDst++ = *pSrc++;
	}
	*pDst = 0;
}

static void Con_SaveConfig(IConsole::IResult *pResult, void *pUserData)
{
	char aFilename[128];
	if(pResult->NumArguments())
		str_format(aFilename, sizeof(aFilename), "configs/%s.cfg", pResult->GetString(0));
	else
	{
		char aDate[20];
		str_timestamp(aDate, sizeof(aDate));
		str_format(aFilename, sizeof(aFilename), "configs/config_%s.cfg", aDate);
	}
	((CConfigManager *)pUserData)->Save(aFilename);
}

CConfigManager::CConfigManager()
{
	m_pStorage = 0;
	m_pConsole = 0;
	m_ConfigFile = 0;
	m_FlagMask = 0;
	m_NumCallbacks = 0;
	m_pMetaTable = 0;
	m_NumMeta = 0;
}

void CConfigManager::InitMetaTable(const CConfigMeta **ppTable, int *pNum)
{
	#define MACRO_CONFIG_INT(Name,ScriptName,def,min,max,flags,desc) { CConfigMeta::TYPE_INT, #ScriptName, (int)offsetof(CConfig, m_##Name), (int)sizeof(int), (int)(def), 0, (int)(min), (int)(max), (int)(flags) },
	#define MACRO_CONFIG_STR(Name,ScriptName,len,def,flags,desc) { CConfigMeta::TYPE_STR, #ScriptName, (int)offsetof(CConfig, m_##Name), (int)(len), 0, (def), 0, 0, (int)(flags) },
	#define MACRO_CONFIG_UTF8STR(Name,ScriptName,size,len,def,flags,desc) { CConfigMeta::TYPE_UTF8STR, #ScriptName, (int)offsetof(CConfig, m_##Name), (int)(size), 0, (def), 0, (int)(len), (int)(flags) },

	static const CConfigMeta s_aMetaTable[] = {
		#include "config_variables.h"
	};

	#undef MACRO_CONFIG_INT
	#undef MACRO_CONFIG_STR
	#undef MACRO_CONFIG_UTF8STR

	*ppTable = s_aMetaTable;
	*pNum = (int)(sizeof(s_aMetaTable) / sizeof(s_aMetaTable[0]));
}

bool CConfigManager::FindMetaByName(const char *pScriptName, const CConfigMeta *pTable, int Num, CConfigMeta *pOut)
{
	for(int i = 0; i < Num; i++)
	{
		if(str_comp(pTable[i].m_pScriptName, pScriptName) == 0)
		{
			*pOut = pTable[i];
			return true;
		}
	}
	return false;
}

void CConfigManager::Init(int FlagMask)
{
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_FlagMask = FlagMask;
	InitMetaTable(&m_pMetaTable, &m_NumMeta);
	Reset();

	if(m_pConsole)
		m_pConsole->Register("save_config", "?s[file]", CFGFLAG_SERVER|CFGFLAG_CLIENT|CFGFLAG_STORE, Con_SaveConfig, this, "Save config to file");
}

void CConfigManager::Reset()
{
	#define MACRO_CONFIG_INT(Name,ScriptName,def,min,max,flags,desc) m_Values.m_##Name = def;
	#define MACRO_CONFIG_STR(Name,ScriptName,len,def,flags,desc) str_copy(m_Values.m_##Name, def, len);
	#define MACRO_CONFIG_UTF8STR(Name,ScriptName,size,len,def,flags,desc) str_utf8_copy_num(m_Values.m_##Name, def, size, len);

	#include "config_variables.h"

	#undef MACRO_CONFIG_INT
	#undef MACRO_CONFIG_STR
	#undef MACRO_CONFIG_UTF8STR
}

void CConfigManager::RestoreStrings()
{
	#define MACRO_CONFIG_INT(Name,ScriptName,def,min,max,flags,desc)	// nop
	#define MACRO_CONFIG_STR(Name,ScriptName,len,def,flags,desc) if(!m_Values.m_##Name[0] && def[0]) str_copy(m_Values.m_##Name, def, len);
	#define MACRO_CONFIG_UTF8STR(Name,ScriptName,size,len,def,flags,desc) if(!m_Values.m_##Name[0] && def[0]) str_utf8_copy_num(m_Values.m_##Name, def, size, len);

	#include "config_variables.h"

	#undef MACRO_CONFIG_INT
	#undef MACRO_CONFIG_STR
	#undef MACRO_CONFIG_UTF8STR
}

void CConfigManager::Validate()
{
	if(!m_pMetaTable)
		return;

	int Corrected = 0;

	for(int i = 0; i < m_NumMeta; i++)
	{
		const CConfigMeta &Meta = m_pMetaTable[i];

		if(!(Meta.m_Flags & m_FlagMask))
			continue;

		if(Meta.m_Type == CConfigMeta::TYPE_INT)
		{
			int *pVal = (int *)(((char *)&m_Values) + Meta.m_Offset);
			bool HasRange = Meta.m_Min != Meta.m_Max;
			bool MaxIsZero = Meta.m_Max == 0;
			int Orig = *pVal;

			if(HasRange && *pVal < Meta.m_Min)
			{
				*pVal = Meta.m_Min;
				Corrected++;
				if(m_pConsole)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "config: corrected %s from %d to min %d", Meta.m_pScriptName, Orig, Meta.m_Min);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
				}
			}
			else if(HasRange && !MaxIsZero && *pVal > Meta.m_Max)
			{
				*pVal = Meta.m_Max;
				Corrected++;
				if(m_pConsole)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "config: corrected %s from %d to max %d", Meta.m_pScriptName, Orig, Meta.m_Max);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
				}
			}
		}
		else if(Meta.m_Type == CConfigMeta::TYPE_STR || Meta.m_Type == CConfigMeta::TYPE_UTF8STR)
		{
			char *pStr = (char *)(((char *)&m_Values) + Meta.m_Offset);
			int Size = Meta.m_Size;
			const char *pDefault = Meta.m_pDefaultStr;

			if(pDefault && pDefault[0] && pStr[0] == 0)
			{
				if(Meta.m_Type == CConfigMeta::TYPE_UTF8STR)
				{
					int Len = Meta.m_Max;
					str_utf8_copy_num(pStr, pDefault, Size, Len ? Len : Size);
				}
				else
					str_copy(pStr, pDefault, Size);
				Corrected++;
				if(m_pConsole)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "config: restored empty string %s to default", Meta.m_pScriptName);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "config", aBuf);
				}
			}
		}
	}

	if(Corrected > 0 && m_pConsole)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "config validation: corrected %d field(s)", Corrected);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
	}
}

void CConfigManager::Upgrade()
{
	if(m_Values.m_ConfigVersion >= 1)
		return;

	if(m_pConsole)
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", "upgrading config to version 1");

	m_Values.m_ConfigVersion = 1;
}

bool CConfigManager::Load(const char *pFilename)
{
	if(!m_pStorage || !m_pConsole)
		return false;

	if(!pFilename)
		pFilename = SETTINGS_FILENAME ".cfg";

	Reset();

	bool Loaded = m_pConsole->ExecuteFile(pFilename);

	if(!Loaded)
	{
		const char *pLegacy = "settings.cfg";
		if(str_comp(pFilename, pLegacy) != 0)
		{
			Loaded = m_pConsole->ExecuteFile(pLegacy);
			if(Loaded && m_pConsole)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "config: loaded legacy '%s'", pLegacy);
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
			}
		}
	}

	Upgrade();
	Validate();
	RestoreStrings();

	if(m_pConsole)
	{
		char aBuf[256];
		if(Loaded)
			str_format(aBuf, sizeof(aBuf), "config: loaded '%s' (v%d)", pFilename, m_Values.m_ConfigVersion);
		else
			str_format(aBuf, sizeof(aBuf), "config: no config file found, using defaults");
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
	}

	return Loaded;
}

void CConfigManager::Save(const char *pFilename)
{
	if(!m_pStorage)
		return;

	Validate();

	if(!pFilename)
		pFilename = SETTINGS_FILENAME ".cfg";
	m_ConfigFile = m_pStorage->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);

	if(!m_ConfigFile)
		return;

	WriteLine("# Teeworlds " GAME_VERSION);

	char aLineBuf[1024*2];
	char aEscapeBuf[1024*2];

	#define MACRO_CONFIG_INT(Name,ScriptName,def,min,max,flags,desc) if(((flags)&(CFGFLAG_SAVE))&&((flags)&(m_FlagMask))&&(m_Values.m_##Name!=int(def))){ str_format(aLineBuf, sizeof(aLineBuf), "%s %i", #ScriptName, m_Values.m_##Name); WriteLine(aLineBuf); }
	#define MACRO_CONFIG_STR(Name,ScriptName,len,def,flags,desc) if(((flags)&(CFGFLAG_SAVE))&&((flags)&(m_FlagMask)&&(str_comp(m_Values.m_##Name,def)))){ EscapeParam(aEscapeBuf, m_Values.m_##Name, sizeof(aEscapeBuf)); str_format(aLineBuf, sizeof(aLineBuf), "%s \"%s\"", #ScriptName, aEscapeBuf); WriteLine(aLineBuf); }
	#define MACRO_CONFIG_UTF8STR(Name,ScriptName,size,len,def,flags,desc) if(((flags)&(CFGFLAG_SAVE))&&((flags)&(m_FlagMask)&&(str_comp(m_Values.m_##Name,def)))){ EscapeParam(aEscapeBuf, m_Values.m_##Name, sizeof(aEscapeBuf)); str_format(aLineBuf, sizeof(aLineBuf), "%s \"%s\"", #ScriptName, aEscapeBuf); WriteLine(aLineBuf); }

	#include "config_variables.h"

	#undef MACRO_CONFIG_INT
	#undef MACRO_CONFIG_STR
	#undef MACRO_CONFIG_UTF8STR

	for(int i = 0; i < m_NumCallbacks; i++)
		m_aCallbacks[i].m_pfnFunc(this, m_aCallbacks[i].m_pUserData);

	io_close(m_ConfigFile);

	if(m_pConsole)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "saved config to '%s'", pFilename);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
	}
}

void CConfigManager::RegisterCallback(SAVECALLBACKFUNC pfnFunc, void *pUserData)
{
	dbg_assert(m_NumCallbacks < MAX_CALLBACKS, "too many config callbacks");
	m_aCallbacks[m_NumCallbacks].m_pfnFunc = pfnFunc;
	m_aCallbacks[m_NumCallbacks].m_pUserData = pUserData;
	m_NumCallbacks++;
}

void CConfigManager::WriteLine(const char *pLine)
{
	if(!m_ConfigFile)
		return;

	io_write(m_ConfigFile, pLine, str_length(pLine));
	io_write_newline(m_ConfigFile);
}

IConfigManager *CreateConfigManager() { return new CConfigManager; }
