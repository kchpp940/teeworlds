/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_SERVERBROWSER_H
#define ENGINE_CLIENT_SERVERBROWSER_H

#include <engine/serverbrowser.h>
#include "serverbrowser_entry.h"
#include "serverbrowser_fav.h"
#include "serverbrowser_filter.h"

class CServerBrowser : public IServerBrowser
{
public:
	enum
	{
		SET_MASTER_ADD=1,
		SET_FAV_ADD,
		SET_TOKEN,
	};
		
	CServerBrowser();
	void Init(class CNetClient *pClient, const char *pNetVersion);
	void Set(const NETADDR &Addr, int SetType, int Token, const CServerInfo *pInfo);
	void Update();

	// interface functions
	int GetType() { return m_ActServerlistType; }
	void SetType(int Type);
	void Refresh(int RefreshFlags);
	bool IsRefreshing() const { return m_pFirstReqServer != 0; }
	bool IsRefreshingMasters() const { return m_pMasterServer->IsRefreshing(); }
	bool WasUpdated(bool Purge);
	int LoadingProgression() const;
	void RequestResort() { m_NeedResort = true; }

	int NumServers() const { return m_aServerlist[m_ActServerlistType].m_NumServers; }
	int NumPlayers() const { return m_aServerlist[m_ActServerlistType].m_NumPlayers; }
	int NumClients() const { return m_aServerlist[m_ActServerlistType].m_NumClients; }
	const CServerInfo *Get(int Index) const { return &m_aServerlist[m_ActServerlistType].m_ppServerlist[Index]->m_Info; }

	int NumSortedServers(int FilterId) const { return m_ServerBrowserFilter.GetNumSortedServers(FilterId); }
	int NumSortedPlayers(int FilterId) const { return m_ServerBrowserFilter.GetNumSortedPlayers(FilterId); }
	const CServerInfo *SortedGet(int FilterId, int Index) const { return &m_aServerlist[m_ActServerlistType].m_ppServerlist[m_ServerBrowserFilter.GetIndex(FilterId, Index)]->m_Info; }
	const void *GetID(int FilterId, int Index) const { return m_ServerBrowserFilter.GetID(FilterId, Index); }

	void GetDisplayCounts(int FilterId, int Index, int *pNum, int *pMax) const { m_ServerBrowserFilter.GetDisplayCounts(FilterId, m_ServerBrowserFilter.GetIndex(FilterId, Index), pNum, pMax); }
	bool IsClientHidden(int FilterId, int Index, int ClientIndex) const { return m_ServerBrowserFilter.IsClientHidden(FilterId, m_ServerBrowserFilter.GetIndex(FilterId, Index), ClientIndex); }

	void AddFavorite(const CServerInfo *pInfo);
	void RemoveFavorite(const CServerInfo *pInfo);
	void UpdateFavoriteState(CServerInfo *pInfo);
	void SetFavoritePassword(const char *pAddress, const char *pPassword);
	const char *GetFavoritePassword(const char *pAddress);

	int AddFilter(const CServerFilterInfo *pFilterInfo) { return m_ServerBrowserFilter.AddFilter(pFilterInfo); }
	void SetFilter(int Index, const CServerFilterInfo *pFilterInfo) { m_ServerBrowserFilter.SetFilter(Index, pFilterInfo); }
	void GetFilter(int Index, CServerFilterInfo *pFilterInfo) { m_ServerBrowserFilter.GetFilter(Index, pFilterInfo); }
	void RemoveFilter(int Index) { m_ServerBrowserFilter.RemoveFilter(Index); }
	int NumFilters() const { return m_ServerBrowserFilter.NumFilters(); }

	// ---- Stable FilterId <-> display order helpers ----
	int GetFilterId(int Index) const { return m_ServerBrowserFilter.GetFilterId(Index); }
	int GetFilterIndex(int FilterId) const { return m_ServerBrowserFilter.GetFilterIndex(FilterId); }

	// ---- Filter presets and metadata ----
	int AddFilterFromPreset(int Preset, const char *pName) { return m_ServerBrowserFilter.AddFilterFromPreset(Preset, pName); }
	void ResetFilterToPreset(int FilterId) { m_ServerBrowserFilter.ResetFilterToPreset(FilterId); RequestResort(); }
	int GetFilterPreset(int FilterId) const { return m_ServerBrowserFilter.GetFilterPreset(FilterId); }
	void GetFilterName(int FilterId, char *pBuf, int Size) const { m_ServerBrowserFilter.GetFilterName(FilterId, pBuf, Size); }
	void SetFilterName(int FilterId, const char *pName) { m_ServerBrowserFilter.SetFilterName(FilterId, pName); }

	// ---- Filter store (persistence + CRUD + ordering + active selection) ----
	void LoadFilters();
	void SaveFilters();
	void EnsureDefaultFilters() { m_ServerBrowserFilter.EnsureDefaultFilters(); }

	int GetActiveFilter(int Type) const { return m_ServerBrowserFilter.GetActiveFilter(Type); }
	void SetActiveFilter(int Type, int FilterId) { m_ServerBrowserFilter.SetActiveFilter(Type, FilterId); }

	int CreateFilter(int Preset, const char *pName) { RequestResort(); return m_ServerBrowserFilter.CreateFilter(Preset, pName); }
	void DeleteFilter(int FilterId) { m_ServerBrowserFilter.DeleteFilter(FilterId); RequestResort(); }
	void RenameFilter(int FilterId, const char *pName) { m_ServerBrowserFilter.SetFilterName(FilterId, pName); }
	void MoveFilter(int FilterId, bool Up) { m_ServerBrowserFilter.MoveFilter(FilterId, Up); RequestResort(); }

	// ---- Aggregated getters/setters for persistence ----
	int GetFilterFlags(int FilterId) const { return m_ServerBrowserFilter.GetFilterFlags(FilterId); }
	void SetFilterFlags(int FilterId, int Flags) { m_ServerBrowserFilter.SetFilterFlags(FilterId, Flags); RequestResort(); }
	int GetFilterLevelMask(int FilterId) const { return m_ServerBrowserFilter.GetFilterLevelMask(FilterId); }
	void SetFilterLevelMask(int FilterId, int Mask) { m_ServerBrowserFilter.SetFilterLevelMask(FilterId, Mask); RequestResort(); }

	// ---- Semantic filter state API ----
	bool GetFilterFlag(int FilterId, int Flag) const { return m_ServerBrowserFilter.GetFilterFlag(FilterId, Flag); }
	void SetFilterFlag(int FilterId, int Flag, bool Enabled) { m_ServerBrowserFilter.SetFilterFlag(FilterId, Flag, Enabled); RequestResort(); }

	int GetFilterPing(int FilterId) const { return m_ServerBrowserFilter.GetFilterPing(FilterId); }
	void SetFilterPing(int FilterId, int Ping) { m_ServerBrowserFilter.SetFilterPing(FilterId, Ping); RequestResort(); }

	void GetFilterAddress(int FilterId, char *pBuf, int Size) const { m_ServerBrowserFilter.GetFilterAddress(FilterId, pBuf, Size); }
	void SetFilterAddress(int FilterId, const char *pAddress) { m_ServerBrowserFilter.SetFilterAddress(FilterId, pAddress); RequestResort(); }

	bool GetFilterCountryEnabled(int FilterId) const { return m_ServerBrowserFilter.GetFilterCountryEnabled(FilterId); }
	void SetFilterCountryEnabled(int FilterId, bool Enabled) { m_ServerBrowserFilter.SetFilterCountryEnabled(FilterId, Enabled); RequestResort(); }
	int GetFilterCountry(int FilterId) const { return m_ServerBrowserFilter.GetFilterCountry(FilterId); }
	void SetFilterCountry(int FilterId, int Country) { m_ServerBrowserFilter.SetFilterCountry(FilterId, Country); RequestResort(); }

	bool IsLevelFiltered(int FilterId, int Level) const { return m_ServerBrowserFilter.IsLevelFiltered(FilterId, Level); }
	void ToggleLevelFilter(int FilterId, int Level) { m_ServerBrowserFilter.ToggleLevelFilter(FilterId, Level); RequestResort(); }

	int GetNumGametypeFilters(int FilterId) const { return m_ServerBrowserFilter.GetNumGametypeFilters(FilterId); }
	void GetGametypeFilter(int FilterId, int Idx, char *pName, int NameSize, bool *pExclusive) const { m_ServerBrowserFilter.GetGametypeFilter(FilterId, Idx, pName, NameSize, pExclusive); }
	void AddGametypeFilter(int FilterId, const char *pName, bool Exclusive) { m_ServerBrowserFilter.AddGametypeFilter(FilterId, pName, Exclusive); RequestResort(); }
	void RemoveGametypeFilter(int FilterId, int Idx) { m_ServerBrowserFilter.RemoveGametypeFilter(FilterId, Idx); RequestResort(); }
	void ClearGametypeFilters(int FilterId) { m_ServerBrowserFilter.ClearGametypeFilters(FilterId); RequestResort(); }

	int GetSort() const { return Config()->m_BrSort; }
	int GetSortOrder() const { return Config()->m_BrSortOrder; }
	void SetSortAndOrder(int SortType, int SortOrder);

	void GetQuickSearchString(char *pBuf, int Size) const { str_copy(pBuf, Config()->m_BrFilterString, Size); }
	void SetQuickSearchString(const char *pString);

	static void CBFTrackPacket(int TrackID, void *pUser);
	
	void LoadServerlist();
	void SaveServerlist();

private:
	class CNetClient *m_pNetClient;
	class CConfig *m_pConfig;
	class IConsole *m_pConsole;
	class IStorage *m_pStorage;
	class IMasterServer *m_pMasterServer;
	class IMapChecker *m_pMapChecker;

	class CServerBrowserFavorites m_ServerBrowserFavorites;
	class CServerBrowserFilter m_ServerBrowserFilter;

	class CConfig *Config() const { return m_pConfig; }
	class IConsole *Console() const { return m_pConsole; }
	class IStorage *Storage() const { return m_pStorage; }

	// serverlist
	int m_ActServerlistType;
	class CServerlist
	{
	public:
		class CHeap m_ServerlistHeap;

		int m_NumClients;
		int m_NumPlayers;
		int m_NumServers;
		int m_NumServerCapacity;
	
		CServerEntry *m_aServerlistIp[256]; // ip hash list
		CServerEntry **m_ppServerlist;

		~CServerlist();
		void Clear();
	} m_aServerlist[NUM_TYPES];

	CServerEntry *m_pFirstReqServer; // request list
	CServerEntry *m_pLastReqServer;
	int m_NumRequests;

	bool m_NeedRefresh;
	bool m_InfoUpdated;
	bool m_NeedResort;

	// the token is to keep server refresh separated from each other
	int m_CurrentLanToken;

	int m_RefreshFlags;
	int64 m_BroadcastTime;
	int64 m_MasterRefreshTime;

	CServerEntry *Add(int ServerlistType, const NETADDR &Addr);
	CServerEntry *Find(int ServerlistType, const NETADDR &Addr);
	void QueueRequest(CServerEntry *pEntry);
	void RemoveRequest(CServerEntry *pEntry);
	void RequestImpl(const NETADDR &Addr, CServerEntry *pEntry);
	void SetInfo(int ServerlistType, CServerEntry *pEntry, const CServerInfo &Info);
};

#endif
