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

	int NumSortedServers(int FilterIndex) const { return m_ServerBrowserFilter.GetNumSortedServers(FilterIndex); }
	int NumSortedPlayers(int FilterIndex) const { return m_ServerBrowserFilter.GetNumSortedPlayers(FilterIndex); }
	const CServerInfo *SortedGet(int FilterIndex, int Index) const { return &m_aServerlist[m_ActServerlistType].m_ppServerlist[m_ServerBrowserFilter.GetIndex(FilterIndex, Index)]->m_Info; }
	const void *GetID(int FilterIndex, int Index) const { return m_ServerBrowserFilter.GetID(FilterIndex, Index); }

	void GetDisplayCounts(int FilterIndex, int Index, int *pNum, int *pMax) const { m_ServerBrowserFilter.GetDisplayCounts(FilterIndex, m_ServerBrowserFilter.GetIndex(FilterIndex, Index), pNum, pMax); }
	bool IsClientHidden(int FilterIndex, int Index, int ClientIndex) const { return m_ServerBrowserFilter.IsClientHidden(FilterIndex, m_ServerBrowserFilter.GetIndex(FilterIndex, Index), ClientIndex); }

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

	// ---- Filter presets and metadata ----
	int AddFilterFromPreset(int Preset, const char *pName) { return m_ServerBrowserFilter.AddFilterFromPreset(Preset, pName); }
	void ResetFilterToPreset(int FilterIndex) { m_ServerBrowserFilter.ResetFilterToPreset(FilterIndex); RequestResort(); }
	int GetFilterPreset(int FilterIndex) const { return m_ServerBrowserFilter.GetFilterPreset(FilterIndex); }
	void GetFilterName(int FilterIndex, char *pBuf, int Size) const { m_ServerBrowserFilter.GetFilterName(FilterIndex, pBuf, Size); }
	void SetFilterName(int FilterIndex, const char *pName) { m_ServerBrowserFilter.SetFilterName(FilterIndex, pName); }

	// ---- Aggregated getters/setters for persistence ----
	int GetFilterFlags(int FilterIndex) const { return m_ServerBrowserFilter.GetFilterFlags(FilterIndex); }
	void SetFilterFlags(int FilterIndex, int Flags) { m_ServerBrowserFilter.SetFilterFlags(FilterIndex, Flags); RequestResort(); }
	int GetFilterLevelMask(int FilterIndex) const { return m_ServerBrowserFilter.GetFilterLevelMask(FilterIndex); }
	void SetFilterLevelMask(int FilterIndex, int Mask) { m_ServerBrowserFilter.SetFilterLevelMask(FilterIndex, Mask); RequestResort(); }

	// ---- Semantic filter state API ----
	bool GetFilterFlag(int FilterIndex, int Flag) const { return m_ServerBrowserFilter.GetFilterFlag(FilterIndex, Flag); }
	void SetFilterFlag(int FilterIndex, int Flag, bool Enabled) { m_ServerBrowserFilter.SetFilterFlag(FilterIndex, Flag, Enabled); RequestResort(); }

	int GetFilterPing(int FilterIndex) const { return m_ServerBrowserFilter.GetFilterPing(FilterIndex); }
	void SetFilterPing(int FilterIndex, int Ping) { m_ServerBrowserFilter.SetFilterPing(FilterIndex, Ping); RequestResort(); }

	void GetFilterAddress(int FilterIndex, char *pBuf, int Size) const { m_ServerBrowserFilter.GetFilterAddress(FilterIndex, pBuf, Size); }
	void SetFilterAddress(int FilterIndex, const char *pAddress) { m_ServerBrowserFilter.SetFilterAddress(FilterIndex, pAddress); RequestResort(); }

	bool GetFilterCountryEnabled(int FilterIndex) const { return m_ServerBrowserFilter.GetFilterCountryEnabled(FilterIndex); }
	void SetFilterCountryEnabled(int FilterIndex, bool Enabled) { m_ServerBrowserFilter.SetFilterCountryEnabled(FilterIndex, Enabled); RequestResort(); }
	int GetFilterCountry(int FilterIndex) const { return m_ServerBrowserFilter.GetFilterCountry(FilterIndex); }
	void SetFilterCountry(int FilterIndex, int Country) { m_ServerBrowserFilter.SetFilterCountry(FilterIndex, Country); RequestResort(); }

	bool IsLevelFiltered(int FilterIndex, int Level) const { return m_ServerBrowserFilter.IsLevelFiltered(FilterIndex, Level); }
	void ToggleLevelFilter(int FilterIndex, int Level) { m_ServerBrowserFilter.ToggleLevelFilter(FilterIndex, Level); RequestResort(); }

	int GetNumGametypeFilters(int FilterIndex) const { return m_ServerBrowserFilter.GetNumGametypeFilters(FilterIndex); }
	void GetGametypeFilter(int FilterIndex, int Idx, char *pName, int NameSize, bool *pExclusive) const { m_ServerBrowserFilter.GetGametypeFilter(FilterIndex, Idx, pName, NameSize, pExclusive); }
	void AddGametypeFilter(int FilterIndex, const char *pName, bool Exclusive) { m_ServerBrowserFilter.AddGametypeFilter(FilterIndex, pName, Exclusive); RequestResort(); }
	void RemoveGametypeFilter(int FilterIndex, int Idx) { m_ServerBrowserFilter.RemoveGametypeFilter(FilterIndex, Idx); RequestResort(); }
	void ClearGametypeFilters(int FilterIndex) { m_ServerBrowserFilter.ClearGametypeFilters(FilterIndex); RequestResort(); }

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
