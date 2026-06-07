/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_SERVERBROWSER_FILTER_H
#define ENGINE_CLIENT_SERVERBROWSER_FILTER_H

#include <base/tl/array.h>

class CServerBrowserFilter
{
public:
	enum
	{
		RESORT_FLAG_FORCE=1,
		RESORT_FLAG_FAV=2,
	};

	class CServerFilter
	{
	public:
		CServerBrowserFilter *m_pServerBrowserFilter;
		CConfig *Config() const { return m_pServerBrowserFilter->m_pConfig; }

		// stable filter identity (never reused, never changes after creation)
		int m_Id;

		// filter metadata (preset + name, owned by engine)
		int m_Preset;
		char m_aName[64];

		// filter settings
		CServerFilterInfo m_FilterInfo;
		
		// stats
		int m_NumSortedPlayers;
		int m_NumSortedServers;
		int *m_pSortedServerlist;
		int m_SortedServersCapacity;
		
		CServerFilter();
		~CServerFilter();
		CServerFilter& operator=(const CServerFilter& Other);

		void Filter();
		int GetSortHash() const;
		void Sort();

		// UI-facing helpers
		void GetDisplayCounts(int Index, int *pNum, int *pMax) const;
		bool IsClientHidden(int Index, int ClientIndex) const;

		// sorting criterions
		bool SortCompareName(int Index1, int Index2) const;
		bool SortCompareMap(int Index1, int Index2) const;
		bool SortComparePing(int Index1, int Index2) const;
		bool SortCompareGametype(int Index1, int Index2) const;
		bool SortCompareNumPlayers(int Index1, int Index2) const;
		bool SortCompareNumRealPlayers(int Index1, int Index2) const;
		bool SortCompareNumClients(int Index1, int Index2) const;
		bool SortCompareNumRealClients(int Index1, int Index2) const;
	};
	CConfig *Config() { return m_pConfig; }

	//
	void Init(class CConfig *pConfig, class IFriends *pFriends, const char *pNetVersion);
	void Clear();
	void Sort(class CServerEntry **ppServerlist, int NumServers, int ResortFlags);

	// filter
	int AddFilter(const class CServerFilterInfo *pFilterInfo);
	void GetFilter(int Index, class CServerFilterInfo *pFilterInfo) const;
	void RemoveFilter(int Index);
	void SetFilter(int Index, const class CServerFilterInfo *pFilterInfo);
	int NumFilters() const { return m_lFilters.size(); }

	// ---- Stable FilterId <-> array index helpers ----
	// Lookup by stable id (linear scan; filter count is small)
	int GetFilterIndex(int FilterId) const;
	// Get stable id by display order (for UI iteration)
	int GetFilterId(int Index) const { return m_lFilters[Index].m_Id; }

	// ---- Filter presets and metadata ----
	int AddFilterFromPreset(int Preset, const char *pName);
	void ResetFilterToPreset(int FilterId);
	int GetFilterPreset(int FilterId) const { return m_lFilters[GetFilterIndex(FilterId)].m_Preset; }
	void GetFilterName(int FilterId, char *pBuf, int Size) const { str_copy(pBuf, m_lFilters[GetFilterIndex(FilterId)].m_aName, Size); }
	void SetFilterName(int FilterId, const char *pName);

	// ---- Filter store in-memory operations (CRUD + ordering + active selection) ----
	void EnsureDefaultFilters();
	int CreateFilter(int Preset, const char *pName) { return AddFilterFromPreset(Preset, pName); }
	void DeleteFilter(int FilterId);
	void MoveFilter(int FilterId, bool Up);

	int GetActiveFilter(int Type) const { return m_aActiveFilters[Type]; }
	void SetActiveFilter(int Type, int FilterId) { m_aActiveFilters[Type] = FilterId; }

	// ---- Aggregated getters/setters for persistence ----
	int GetFilterFlags(int FilterId) const { return m_lFilters[GetFilterIndex(FilterId)].m_FilterInfo.m_SortHash & 0xFFFF; }
	void SetFilterFlags(int FilterId, int Flags);
	int GetFilterLevelMask(int FilterId) const { return m_lFilters[GetFilterIndex(FilterId)].m_FilterInfo.m_ServerLevel; }
	void SetFilterLevelMask(int FilterId, int Mask);
	
	// stats
	const void *GetID(int FilterId, int Index) const { return &m_lFilters[GetFilterIndex(FilterId)].m_pSortedServerlist[Index]; }
	int GetIndex(int FilterId, int Index) const { return m_lFilters[GetFilterIndex(FilterId)].m_pSortedServerlist[Index]; }
	int GetNumSortedServers(int FilterId) const { return m_lFilters[GetFilterIndex(FilterId)].m_NumSortedServers; }
	int GetNumSortedPlayers(int FilterId) const { return m_lFilters[GetFilterIndex(FilterId)].m_NumSortedPlayers; }

	// UI-facing helpers
	void GetDisplayCounts(int FilterId, int Index, int *pNum, int *pMax) const { m_lFilters[GetFilterIndex(FilterId)].GetDisplayCounts(Index, pNum, pMax); }
	bool IsClientHidden(int FilterId, int Index, int ClientIndex) const { return m_lFilters[GetFilterIndex(FilterId)].IsClientHidden(Index, ClientIndex); }

	// ---- Semantic filter state API ----
	bool GetFilterFlag(int FilterId, int Flag) const { return (m_lFilters[GetFilterIndex(FilterId)].m_FilterInfo.m_SortHash & Flag) != 0; }
	void SetFilterFlag(int FilterId, int Flag, bool Enabled);

	int GetFilterPing(int FilterId) const { return m_lFilters[GetFilterIndex(FilterId)].m_FilterInfo.m_Ping; }
	void SetFilterPing(int FilterId, int Ping);

	void GetFilterAddress(int FilterId, char *pBuf, int Size) const { str_copy(pBuf, m_lFilters[GetFilterIndex(FilterId)].m_FilterInfo.m_aAddress, Size); }
	void SetFilterAddress(int FilterId, const char *pAddress);

	bool GetFilterCountryEnabled(int FilterId) const { return GetFilterFlag(FilterId, IServerBrowser::FILTER_COUNTRY); }
	void SetFilterCountryEnabled(int FilterId, bool Enabled) { SetFilterFlag(FilterId, IServerBrowser::FILTER_COUNTRY, Enabled); }
	int GetFilterCountry(int FilterId) const { return m_lFilters[GetFilterIndex(FilterId)].m_FilterInfo.m_Country; }
	void SetFilterCountry(int FilterId, int Country);

	bool IsLevelFiltered(int FilterId, int Level) const { return m_lFilters[GetFilterIndex(FilterId)].m_FilterInfo.IsLevelFiltered(Level) != 0; }
	void ToggleLevelFilter(int FilterId, int Level);

	int GetNumGametypeFilters(int FilterId) const;
	void GetGametypeFilter(int FilterId, int Idx, char *pName, int NameSize, bool *pExclusive) const;
	void AddGametypeFilter(int FilterId, const char *pName, bool Exclusive);
	void RemoveGametypeFilter(int FilterId, int Idx);
	void ClearGametypeFilters(int FilterId);

private:
	class CConfig *m_pConfig;
	class IFriends *m_pFriends;
	char m_aNetVersion[128];
	array<CServerFilter> m_lFilters;
	int m_aActiveFilters[IServerBrowser::NUM_TYPES]; // stores stable FilterId, NOT array index
	int m_NextFilterId;

	// get updated on sort
	class CServerEntry **m_ppServerlist;
	int m_NumServers;
};

#endif
