/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVERBROWSER_H
#define ENGINE_SERVERBROWSER_H

#include <engine/shared/protocol.h>

#include "kernel.h"

/*
	Structure: CServerInfo
*/
class CServerInfo
{
public:
	/*
		Structure: CInfoClient
	*/
	class CClient
	{
	public:
		char m_aName[MAX_NAME_ARRAY_SIZE];
		char m_aClan[MAX_CLAN_ARRAY_SIZE];
		int m_Country;
		int m_Score;
		int m_PlayerType;

		int m_FriendState;

		enum
		{
			PLAYERFLAG_SPEC=1,
			PLAYERFLAG_BOT=2,
			PLAYERFLAG_MASK=3,
		};
	};

	enum
	{
		LEVEL_CASUAL = 0,
		LEVEL_NORMAL = 1,
		LEVEL_COMPETITIVE = 2,
		NUM_SERVER_LEVELS = 3
	};

	//int m_SortedIndex;
	int m_ServerIndex;

	NETADDR m_NetAddr;

	int m_QuickSearchHit;
	int m_FriendState;

	int m_MaxClients;
	int m_NumClients;
	int m_MaxPlayers;
	int m_NumPlayers;
	int m_NumBotPlayers;
	int m_NumBotSpectators;
	int m_Flags;
	int m_ServerLevel;
	bool m_Favorite;
	int m_Latency; // in ms
	char m_aGameType[16];
	char m_aName[64];
	char m_aHostname[128];
	char m_aMap[32];
	char m_aVersion[32];
	char m_aAddress[NETADDR_MAXSTRSIZE];
	CClient m_aClients[MAX_CLIENTS];

	// ---- derived display fields (calculated during filter) ----
	// NumClients/NumPlayers adjusted by bot-filter and spectator-filter settings,
	// ready for UI display without re-running the business rules.
	int m_NumDisplayClients;
	int m_MaxDisplayClients;
};

class CServerFilterInfo
{
public:
	enum
	{
		MAX_GAMETYPES=8,
	};
	int m_SortHash;
	int m_Ping;
	int m_Country;
	int m_ServerLevel;
	char m_aGametype[MAX_GAMETYPES][16];
	char m_aGametypeExclusive[MAX_GAMETYPES];
	char m_aAddress[NETADDR_MAXSTRSIZE];

	void Set(const CServerFilterInfo *pSrc);

	void ToggleLevel(int Level)
	{
		m_ServerLevel ^= 1 << Level;
		if(m_ServerLevel == (1 << CServerInfo::NUM_SERVER_LEVELS)-1)
		{
			// Prevent filter that excludes everything
			m_ServerLevel = 0;
		}
	}

	int IsLevelFiltered(int Level) const
	{
		return m_ServerLevel & (1 << Level);
	}
};

class IServerBrowser : public IInterface
{
	MACRO_INTERFACE("serverbrowser", 0)
public:

	/* Constants: Server Browser Sorting
		SORT_NAME - Sort by name.
		SORT_PING - Sort by ping.
		SORT_MAP - Sort by map
		SORT_GAMETYPE - Sort by game type. DM, TDM etc.
		SORT_NUMPLAYERS - Sort after how many players there are on the server.
	*/
	enum {
		SORT_NAME=0,
		SORT_PING,
		SORT_MAP,
		SORT_GAMETYPE,
		SORT_NUMPLAYERS,

		QUICK_SERVERNAME=1,
		QUICK_PLAYER=2,
		QUICK_MAPNAME=4,
		QUICK_GAMETYPE=8,

		TYPE_INTERNET=0,
		TYPE_LAN,
		NUM_TYPES,

		REFRESHFLAG_INTERNET=1,
		REFRESHFLAG_LAN=2,

		LAN_PORT_BEGIN = 8303,
		LAN_PORT_END = 8310,

		FLAG_PASSWORD=1,
		FLAG_PURE=2,
		FLAG_PUREMAP=4,
		FLAG_TIMESCORE=8,

		FILTER_BOTS=16,
		FILTER_EMPTY=32,
		FILTER_FULL=64,
		FILTER_SPECTATORS=128,
		FILTER_FRIENDS=256,
		FILTER_PW=512,
		FILTER_FAVORITE=1024,
		FILTER_COMPAT_VERSION=2048,
		FILTER_PURE=4096,
		FILTER_PURE_MAP=8192,
		FILTER_COUNTRY= 16384,
	};

	virtual int GetType() = 0;
	virtual void SetType(int Type) = 0;
	virtual void Refresh(int RefreshFlags) = 0;
	virtual bool IsRefreshing() const = 0;
	virtual bool IsRefreshingMasters() const = 0;
	virtual bool WasUpdated(bool Purge) = 0;
	virtual int LoadingProgression() const = 0;

	virtual int NumServers() const = 0;
	virtual int NumPlayers() const = 0;
	virtual int NumClients() const = 0;
	virtual const CServerInfo *Get(int Index) const = 0;

	virtual int NumSortedServers(int Index) const = 0;
	virtual int NumSortedPlayers(int Index) const = 0;
	virtual const CServerInfo *SortedGet(int FilterIndex, int Index) const = 0;
	virtual const void *GetID(int FilterIndex, int Index) const = 0;

	// UI-facing helpers: run all business rules inside engine so menus don't
	// need to know about FILTER_* flags or recompute counts.
	virtual void GetDisplayCounts(int FilterIndex, int Index, int *pNum, int *pMax) const = 0;
	virtual bool IsClientHidden(int FilterIndex, int Index, int ClientIndex) const = 0;

	virtual void AddFavorite(const CServerInfo *pInfo) = 0;
	virtual void RemoveFavorite(const CServerInfo *pInfo) = 0;
	virtual void UpdateFavoriteState(CServerInfo *pInfo) = 0;
	virtual void SetFavoritePassword(const char *pAddress, const char *pPassword) = 0;
	virtual const char *GetFavoritePassword(const char *pAddress) = 0;

	virtual int AddFilter(const CServerFilterInfo *pFilterInfo) = 0;
	virtual void SetFilter(int Index, const CServerFilterInfo *pFilterInfo) = 0;
	virtual void GetFilter(int Index, CServerFilterInfo *pFilterInfo) = 0;
	virtual void RemoveFilter(int Index) = 0;
	virtual int NumFilters() const = 0;

	// ---- Filter presets and metadata ----
	// Presets are built-in filter templates that live in engine.
	// UI creates filters from presets and never needs to know the
	// default values of FILTER_COMPAT_VERSION, gametype lists, etc.
	enum
	{
		PRESET_CUSTOM = 0,
		PRESET_ALL,
		PRESET_STANDARD,
		PRESET_FAVORITES,
		PRESET_RACE,
		NUM_PRESETS,
	};
	virtual int AddFilterFromPreset(int Preset, const char *pName) = 0;
	virtual void ResetFilterToPreset(int FilterIndex) = 0;
	virtual int GetFilterPreset(int FilterIndex) const = 0;
	virtual void GetFilterName(int FilterIndex, char *pBuf, int Size) const = 0;
	virtual void SetFilterName(int FilterIndex, const char *pName) = 0;

	// ---- Filter store (persistence + CRUD + ordering + active selection) ----
	// Engine owns the entire filter list, its order, per-type active filter
	// selection, and JSON persistence. UI only asks engine to mutate state.
	virtual void LoadFilters() = 0;
	virtual void SaveFilters() = 0;
	virtual void EnsureDefaultFilters() = 0;

	virtual int GetActiveFilter(int Type) const = 0;
	virtual void SetActiveFilter(int Type, int FilterIndex) = 0;

	virtual int CreateFilter(int Preset, const char *pName) = 0;
	virtual void DeleteFilter(int FilterIndex) = 0;
	virtual void RenameFilter(int FilterIndex, const char *pName) = 0;
	virtual void MoveFilter(int FilterIndex, bool Up) = 0;

	// ---- Aggregated state getters/setters for persistence ----
	// These let the persistence layer (UI settings file) read/write
	// entire flags mask / level mask without understanding how
	// FILTER_* flags are packed into SortHash or how Level bits are
	// stored. They are intentionally coarse and not used by normal UI.
	virtual int GetFilterFlags(int FilterIndex) const = 0;
	virtual void SetFilterFlags(int FilterIndex, int Flags) = 0;
	virtual int GetFilterLevelMask(int FilterIndex) const = 0;
	virtual void SetFilterLevelMask(int FilterIndex, int Mask) = 0;

	// ---- Semantic filter state API ----
	// UI calls these instead of manipulating CServerFilterInfo directly.
	// All mutations auto-trigger re-filter/sort and update derived display fields.

	virtual bool GetFilterFlag(int FilterIndex, int Flag) const = 0;
	virtual void SetFilterFlag(int FilterIndex, int Flag, bool Enabled) = 0;

	virtual int GetFilterPing(int FilterIndex) const = 0;
	virtual void SetFilterPing(int FilterIndex, int Ping) = 0;

	virtual void GetFilterAddress(int FilterIndex, char *pBuf, int Size) const = 0;
	virtual void SetFilterAddress(int FilterIndex, const char *pAddress) = 0;

	virtual bool GetFilterCountryEnabled(int FilterIndex) const = 0;
	virtual void SetFilterCountryEnabled(int FilterIndex, bool Enabled) = 0;
	virtual int GetFilterCountry(int FilterIndex) const = 0;
	virtual void SetFilterCountry(int FilterIndex, int Country) = 0;

	virtual bool IsLevelFiltered(int FilterIndex, int Level) const = 0;
	virtual void ToggleLevelFilter(int FilterIndex, int Level) = 0;

	virtual int GetNumGametypeFilters(int FilterIndex) const = 0;
	virtual void GetGametypeFilter(int FilterIndex, int Idx, char *pName, int NameSize, bool *pExclusive) const = 0;
	virtual void AddGametypeFilter(int FilterIndex, const char *pName, bool Exclusive) = 0;
	virtual void RemoveGametypeFilter(int FilterIndex, int Idx) = 0;
	virtual void ClearGametypeFilters(int FilterIndex) = 0;

	// Global sorting (not per-filter)
	virtual int GetSort() const = 0;
	virtual int GetSortOrder() const = 0;
	virtual void SetSortAndOrder(int SortType, int SortOrder) = 0;

	// Global quick-search string (lives in Config, but accessed uniformly)
	virtual void GetQuickSearchString(char *pBuf, int Size) const = 0;
	virtual void SetQuickSearchString(const char *pString) = 0;
};

#endif
