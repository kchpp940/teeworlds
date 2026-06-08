/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/config.h>
#include <engine/console.h>
#include <engine/storage.h>
#include <engine/shared/config.h>
#include <game/version.h>

#include <cstddef>
#include <cstring>


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

CConfigManager::CConfigManager()
{
	m_pStorage = 0;
	m_pConsole = 0;
	m_ConfigFile = 0;
	m_FlagMask = 0;
	m_NumCallbacks = 0;
	m_pMetaTable = 0;
	m_NumMeta = 0;
	m_NumCorrections = 0;
	m_NumMissingFields = 0;
	m_NumDeprecated = 0;
	m_NumTrackedFields = 0;
	mem_zero(m_aFieldLoaded, sizeof(m_aFieldLoaded));
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

bool CConfigManager::FindMetaByName(const char *pScriptName, CConfigMeta *pOut) const
{
	if(!m_pMetaTable)
		return false;
	for(int i = 0; i < m_NumMeta; i++)
	{
		if(str_comp(m_pMetaTable[i].m_pScriptName, pScriptName) == 0)
		{
			*pOut = m_pMetaTable[i];
			return true;
		}
	}
	return false;
}

bool CConfigManager::RedirectDeprecated(const char *pScriptName, char *pRedirectBuffer, int BufferSize) const
{
	for(int i = 0; i < m_NumDeprecated; i++)
	{
		if(str_comp(m_aDeprecated[i].m_pOldName, pScriptName) == 0)
		{
			str_copy(pRedirectBuffer, m_aDeprecated[i].m_pNewName, BufferSize);
			return true;
		}
	}
	return false;
}

void CConfigManager::AddCorrection(ECorrectionReason Reason, const char *pFieldName, const char *pOldValue, const char *pNewValue)
{
	if(m_NumCorrections >= MAX_CORRECTIONS)
		return;
	CCorrection &c = m_aCorrections[m_NumCorrections++];
	c.m_Reason = Reason;
	c.m_pFieldName = pFieldName;
	str_copy(c.m_aOldValue, pOldValue ? pOldValue : "", sizeof(c.m_aOldValue));
	str_copy(c.m_aNewValue, pNewValue ? pNewValue : "", sizeof(c.m_aNewValue));
}

void CConfigManager::MarkFieldLoaded(const char *pScriptName)
{
	if(!m_pMetaTable || m_NumTrackedFields <= 0)
		return;
	for(int i = 0; i < m_NumMeta && i < MAX_META; i++)
	{
		if(str_comp(m_pMetaTable[i].m_pScriptName, pScriptName) == 0)
		{
			m_aFieldLoaded[i] = 1;
			return;
		}
	}
}

void CConfigManager::DetectMissingFields()
{
	m_NumMissingFields = 0;
	if(!m_pMetaTable)
		return;
	for(int i = 0; i < m_NumMeta && i < MAX_META; i++)
	{
		const CConfigMeta &Meta = m_pMetaTable[i];
		if(!(Meta.m_Flags & m_FlagMask))
			continue;
		if(!(Meta.m_Flags & CFGFLAG_SAVE))
			continue;
		if(!m_aFieldLoaded[i])
		{
			if(m_NumMissingFields < MAX_MISSING)
			{
				char aOld[64], aNew[64];
				if(Meta.m_Type == CConfigMeta::TYPE_INT)
				{
					str_format(aOld, sizeof(aOld), "<missing>");
					str_format(aNew, sizeof(aNew), "%d", Meta.m_DefaultInt);
				}
				else
				{
					str_format(aOld, sizeof(aOld), "<missing>");
					str_copy(aNew, Meta.m_pDefaultStr ? Meta.m_pDefaultStr : "", sizeof(aNew));
				}
				m_apMissingFields[m_NumMissingFields++] = Meta.m_pScriptName;
				AddCorrection(CORRECTION_MISSING_FIELD, Meta.m_pScriptName, aOld, aNew);
			}
		}
	}
}

int CConfigManager::ClampIntValue(const CConfigMeta &Meta, int Value, bool *pCorrected)
{
	bool HasRange = Meta.m_Min != Meta.m_Max;
	bool MaxIsZero = Meta.m_Max == 0;
	int Orig = Value;

	if(HasRange && Value < Meta.m_Min)
	{
		if(pCorrected) *pCorrected = true;
		char aOld[32], aNew[32];
		str_format(aOld, sizeof(aOld), "%d", Orig);
		str_format(aNew, sizeof(aNew), "%d", Meta.m_Min);
		AddCorrection(CORRECTION_RANGE_MIN, Meta.m_pScriptName, aOld, aNew);
		return Meta.m_Min;
	}
	if(HasRange && !MaxIsZero && Value > Meta.m_Max)
	{
		if(pCorrected) *pCorrected = true;
		char aOld[32], aNew[32];
		str_format(aOld, sizeof(aOld), "%d", Orig);
		str_format(aNew, sizeof(aNew), "%d", Meta.m_Max);
		AddCorrection(CORRECTION_RANGE_MAX, Meta.m_pScriptName, aOld, aNew);
		return Meta.m_Max;
	}
	return Value;
}

const char *CConfigManager::ClampStrValue(const CConfigMeta &Meta, const char *pValue, char *pBuffer, int BufferSize, bool *pCorrected)
{
	if(!pValue)
		pValue = "";

	if(Meta.m_Type == CConfigMeta::TYPE_UTF8STR)
	{
		int Size = Meta.m_Size;
		int Len = Meta.m_Max ? Meta.m_Max : Size;
		str_utf8_copy_num(pBuffer, pValue, Size, Len);

		if(str_comp(pValue, pBuffer) != 0)
		{
			if(pCorrected) *pCorrected = true;
			AddCorrection(CORRECTION_INVALID_UTF8, Meta.m_pScriptName, pValue, pBuffer);
		}
		return pBuffer;
	}
	else
	{
		str_copy(pBuffer, pValue, Meta.m_Size < BufferSize ? Meta.m_Size : BufferSize);
		return pBuffer;
	}
}

void CConfigManager::ConSaveConfig(IConsole::IResult *pResult, void *pUserData)
{
	CConfigManager *pSelf = (CConfigManager *)pUserData;
	char aFilename[128];
	if(pResult->NumArguments())
		str_format(aFilename, sizeof(aFilename), "configs/%s.cfg", pResult->GetString(0));
	else
	{
		char aDate[20];
		str_timestamp(aDate, sizeof(aDate));
		str_format(aFilename, sizeof(aFilename), "configs/config_%s.cfg", aDate);
	}
	pSelf->Save(aFilename);
}

void CConfigManager::ConSetConfig(IConsole::IResult *pResult, void *pUserData)
{
	CConfigManager *pSelf = (CConfigManager *)pUserData;
	if(pResult->NumArguments() < 2)
		return;
	const char *pName = pResult->GetString(0);
	const char *pValue = pResult->GetString(1);

	CConfigMeta Meta;
	char aRedirect[128];
	if(!pSelf->FindMetaByName(pName, &Meta))
	{
		if(pSelf->RedirectDeprecated(pName, aRedirect, sizeof(aRedirect)))
		{
			char aOld[64], aNew[64];
			str_copy(aOld, pName, sizeof(aOld));
			str_copy(aNew, aRedirect, sizeof(aNew));
			pSelf->AddCorrection(CORRECTION_DEPRECATED_REDIRECT, pName, aOld, aNew);
			pName = aRedirect;
			if(!pSelf->FindMetaByName(pName, &Meta))
				return;
		}
		else
			return;
	}

	if(Meta.m_Type == CConfigMeta::TYPE_INT)
		pSelf->SetInt(pName, pResult->GetInteger(1));
	else
		pSelf->SetStr(pName, pValue);
}

void CConfigManager::ConDumpConfig(IConsole::IResult *pResult, void *pUserData)
{
	CConfigManager *pSelf = (CConfigManager *)pUserData;
	IConsole *pConsole = pSelf->m_pConsole;
	if(!pConsole) return;

	char aBuf[512];
	for(int i = 0; i < pSelf->m_NumMeta; i++)
	{
		const CConfigMeta &Meta = pSelf->m_pMetaTable[i];
		if(!(Meta.m_Flags & pSelf->m_FlagMask))
			continue;
		if(Meta.m_Type == CConfigMeta::TYPE_INT)
		{
			int *pVal = (int *)(((char *)&pSelf->m_Values) + Meta.m_Offset);
			str_format(aBuf, sizeof(aBuf), "%s = %d", Meta.m_pScriptName, *pVal);
		}
		else
		{
			char *pStr = (char *)(((char *)&pSelf->m_Values) + Meta.m_Offset);
			str_format(aBuf, sizeof(aBuf), "%s = \"%s\"", Meta.m_pScriptName, pStr);
		}
		pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
	}

	str_format(aBuf, sizeof(aBuf), "--- %d corrections, %d missing fields ---", pSelf->m_NumCorrections, pSelf->m_NumMissingFields);
	pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
	for(int i = 0; i < pSelf->m_NumCorrections; i++)
	{
		const CCorrection &c = pSelf->m_aCorrections[i];
		str_format(aBuf, sizeof(aBuf), "  [%d] %s: '%s' -> '%s'", (int)c.m_Reason, c.m_pFieldName, c.m_aOldValue, c.m_aNewValue);
		pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
	}
}

void CConfigManager::ConResetConfig(IConsole::IResult *pResult, void *pUserData)
{
	CConfigManager *pSelf = (CConfigManager *)pUserData;
	pSelf->Reset();
	pSelf->ClearCorrections();
	if(pSelf->m_pConsole)
		pSelf->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", "config reset to defaults");
}

void CConfigManager::Init(int FlagMask)
{
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_FlagMask = FlagMask;
	InitMetaTable(&m_pMetaTable, &m_NumMeta);
	m_NumTrackedFields = m_NumMeta < MAX_META ? m_NumMeta : MAX_META;
	Reset();

	if(m_pConsole)
	{
		m_pConsole->Register("save_config", "?s[file]", CFGFLAG_SERVER|CFGFLAG_CLIENT|CFGFLAG_STORE, ConSaveConfig, this, "Save config to file");
		m_pConsole->Register("config_set", "s[name] s[value]", CFGFLAG_SERVER|CFGFLAG_CLIENT, ConSetConfig, this, "Set config value via managed setter");
		m_pConsole->Register("config_dump", "", CFGFLAG_SERVER|CFGFLAG_CLIENT, ConDumpConfig, this, "Dump all config values and corrections");
		m_pConsole->Register("config_reset", "", CFGFLAG_SERVER|CFGFLAG_CLIENT, ConResetConfig, this, "Reset all config to defaults");
	}
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

	mem_zero(m_aFieldLoaded, sizeof(m_aFieldLoaded));
	m_NumMissingFields = 0;
}

void CConfigManager::RestoreStrings()
{
	#define MACRO_CONFIG_INT(Name,ScriptName,def,min,max,flags,desc)
	#define MACRO_CONFIG_STR(Name,ScriptName,len,def,flags,desc) if(!m_Values.m_##Name[0] && def[0]) str_copy(m_Values.m_##Name, def, len);
	#define MACRO_CONFIG_UTF8STR(Name,ScriptName,size,len,def,flags,desc) if(!m_Values.m_##Name[0] && def[0]) str_utf8_copy_num(m_Values.m_##Name, def, size, len);

	#include "config_variables.h"

	#undef MACRO_CONFIG_INT
	#undef MACRO_CONFIG_STR
	#undef MACRO_CONFIG_UTF8STR
}

bool CConfigManager::SetInt(const char *pScriptName, int Value)
{
	CConfigMeta Meta;
	char aRedirect[128];
	const char *pEffectiveName = pScriptName;

	if(!FindMetaByName(pScriptName, &Meta))
	{
		if(RedirectDeprecated(pScriptName, aRedirect, sizeof(aRedirect)))
		{
			char aOld[64], aNew[64];
			str_copy(aOld, pScriptName, sizeof(aOld));
			str_copy(aNew, aRedirect, sizeof(aNew));
			AddCorrection(CORRECTION_DEPRECATED_REDIRECT, pScriptName, aOld, aNew);
			pEffectiveName = aRedirect;
			if(!FindMetaByName(pEffectiveName, &Meta))
				return false;
		}
		else
			return false;
	}

	if(Meta.m_Type != CConfigMeta::TYPE_INT)
		return false;
	if(!(Meta.m_Flags & m_FlagMask) && !(Meta.m_Flags & CFGFLAG_STORE))
		return false;

	bool Corrected = false;
	int FinalValue = ClampIntValue(Meta, Value, &Corrected);

	int *pVal = (int *)(((char *)&m_Values) + Meta.m_Offset);
	*pVal = FinalValue;

	if(Corrected && m_pConsole)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "config: %s clamped to %d (was %d)", Meta.m_pScriptName, FinalValue, Value);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "config", aBuf);
	}

	MarkFieldLoaded(pEffectiveName);
	return true;
}

bool CConfigManager::SetStr(const char *pScriptName, const char *pValue)
{
	CConfigMeta Meta;
	char aRedirect[128];
	const char *pEffectiveName = pScriptName;

	if(!FindMetaByName(pScriptName, &Meta))
	{
		if(RedirectDeprecated(pScriptName, aRedirect, sizeof(aRedirect)))
		{
			char aOld[64], aNew[64];
			str_copy(aOld, pScriptName, sizeof(aOld));
			str_copy(aNew, aRedirect, sizeof(aNew));
			AddCorrection(CORRECTION_DEPRECATED_REDIRECT, pScriptName, aOld, aNew);
			pEffectiveName = aRedirect;
			if(!FindMetaByName(pEffectiveName, &Meta))
				return false;
		}
		else
			return false;
	}

	if(Meta.m_Type != CConfigMeta::TYPE_STR && Meta.m_Type != CConfigMeta::TYPE_UTF8STR)
		return false;
	if(!(Meta.m_Flags & m_FlagMask) && !(Meta.m_Flags & CFGFLAG_STORE))
		return false;

	char aBuffer[1024];
	bool Corrected = false;
	const char *pFinal = ClampStrValue(Meta, pValue ? pValue : "", aBuffer, sizeof(aBuffer), &Corrected);

	char *pStr = (char *)(((char *)&m_Values) + Meta.m_Offset);
	if(Meta.m_Type == CConfigMeta::TYPE_UTF8STR)
	{
		int Size = Meta.m_Size;
		int Len = Meta.m_Max ? Meta.m_Max : Size;
		str_utf8_copy_num(pStr, pFinal, Size, Len);
	}
	else
		str_copy(pStr, pFinal, Meta.m_Size);

	if(Corrected && m_pConsole)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "config: %s corrected for length/utf8", Meta.m_pScriptName);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "config", aBuf);
	}

	MarkFieldLoaded(pEffectiveName);
	return true;
}

bool CConfigManager::GetInt(const char *pScriptName, int *pOutValue) const
{
	CConfigMeta Meta;
	if(!FindMetaByName(pScriptName, &Meta))
		return false;
	if(Meta.m_Type != CConfigMeta::TYPE_INT)
		return false;
	const int *pVal = (const int *)(((const char *)&m_Values) + Meta.m_Offset);
	if(pOutValue)
		*pOutValue = *pVal;
	return true;
}

bool CConfigManager::GetStr(const char *pScriptName, char *pOutValue, int OutSize) const
{
	CConfigMeta Meta;
	if(!FindMetaByName(pScriptName, &Meta))
		return false;
	if(Meta.m_Type != CConfigMeta::TYPE_STR && Meta.m_Type != CConfigMeta::TYPE_UTF8STR)
		return false;
	const char *pStr = (const char *)(((const char *)&m_Values) + Meta.m_Offset);
	if(pOutValue && OutSize > 0)
		str_copy(pOutValue, pStr, OutSize);
	return true;
}

const IConfigManager::CCorrection *CConfigManager::GetCorrection(int Index) const
{
	if(Index < 0 || Index >= m_NumCorrections)
		return 0;
	return &m_aCorrections[Index];
}

const char *CConfigManager::GetMissingField(int Index) const
{
	if(Index < 0 || Index >= m_NumMissingFields)
		return 0;
	return m_apMissingFields[Index];
}

void CConfigManager::RegisterDeprecated(const char *pOldScriptName, const char *pNewScriptName)
{
	if(m_NumDeprecated >= MAX_DEPRECATED)
		return;
	m_aDeprecated[m_NumDeprecated].m_pOldName = pOldScriptName;
	m_aDeprecated[m_NumDeprecated].m_pNewName = pNewScriptName;
	m_NumDeprecated++;
}

void CConfigManager::Validate()
{
	if(!m_pMetaTable)
		return;

	for(int i = 0; i < m_NumMeta; i++)
	{
		const CConfigMeta &Meta = m_pMetaTable[i];

		if(!(Meta.m_Flags & m_FlagMask))
			continue;

		if(Meta.m_Type == CConfigMeta::TYPE_INT)
		{
			int *pVal = (int *)(((char *)&m_Values) + Meta.m_Offset);
			bool Corrected = false;
			int Clamped = ClampIntValue(Meta, *pVal, &Corrected);
			if(Corrected)
				*pVal = Clamped;
		}
		else if(Meta.m_Type == CConfigMeta::TYPE_STR || Meta.m_Type == CConfigMeta::TYPE_UTF8STR)
		{
			char *pStr = (char *)(((char *)&m_Values) + Meta.m_Offset);
			const char *pDefault = Meta.m_pDefaultStr;

			if(pDefault && pDefault[0] && pStr[0] == 0)
			{
				char aOld[128], aNew[128];
				str_copy(aOld, "<empty>", sizeof(aOld));
				str_copy(aNew, pDefault, sizeof(aNew));
				AddCorrection(CORRECTION_EMPTY_STRING, Meta.m_pScriptName, aOld, aNew);

				if(Meta.m_Type == CConfigMeta::TYPE_UTF8STR)
				{
					int Len = Meta.m_Max;
					str_utf8_copy_num(pStr, pDefault, Meta.m_Size, Len ? Len : Meta.m_Size);
				}
				else
					str_copy(pStr, pDefault, Meta.m_Size);
			}
			else
			{
				char aBuffer[1024];
				bool Corrected = false;
				const char *pFinal = ClampStrValue(Meta, pStr, aBuffer, sizeof(aBuffer), &Corrected);
				if(Corrected)
				{
					if(Meta.m_Type == CConfigMeta::TYPE_UTF8STR)
					{
						int Len = Meta.m_Max ? Meta.m_Max : Meta.m_Size;
						str_utf8_copy_num(pStr, pFinal, Meta.m_Size, Len);
					}
					else
						str_copy(pStr, pFinal, Meta.m_Size);
				}
			}
		}
	}

	if(m_pConsole && m_NumCorrections > 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "config validation: %d correction(s) recorded", m_NumCorrections);
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
	ClearCorrections();

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

	DetectMissingFields();
	Upgrade();
	Validate();
	RestoreStrings();

	if(m_pConsole)
	{
		char aBuf[512];
		if(Loaded)
			str_format(aBuf, sizeof(aBuf), "config: loaded '%s' (v%d) — %d correction(s), %d missing field(s)", pFilename, m_Values.m_ConfigVersion, m_NumCorrections, m_NumMissingFields);
		else
			str_format(aBuf, sizeof(aBuf), "config: no config file found, using defaults — %d correction(s)", m_NumCorrections);
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
		str_format(aBuf, sizeof(aBuf), "saved config to '%s' (%d corrections applied before save)", pFilename, m_NumCorrections);
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
