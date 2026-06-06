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

	// ---- Filter presets and metadata ----
	int AddFilterFromPreset(int Preset, const char *pName);
	void ResetFilterToPreset(int FilterIndex);
	int GetFilterPreset(int FilterIndex) const { return m_lFilters[FilterIndex].m_Preset; }
	void GetFilterName(int FilterIndex, char *pBuf, int Size) const { str_copy(pBuf, m_lFilters[FilterIndex].m_aName, Size); }
	void SetFilterName(int FilterIndex, const char *pName);

	// ---- Filter store in-memory operations (CRUD + ordering + active selection) ----
	void EnsureDefaultFilters();
	int CreateFilter(int Preset, const char *pName) { return AddFilterFromPreset(Preset, pName); }
	void DeleteFilter(int FilterIndex);
	void MoveFilter(int FilterIndex, bool Up);

	int GetActiveFilter(int Type) const { return m_aActiveFilters[Type]; }
	void SetActiveFilter(int Type, int FilterIndex) { m_aActiveFilters[Type] = FilterIndex; }

	// ---- Aggregated getters/setters for persistence ----
	int GetFilterFlags(int FilterIndex) const { return m_lFilters[FilterIndex].m_FilterInfo.m_SortHash & 0xFFFF; }
	void SetFilterFlags(int FilterIndex, int Flags);
	int GetFilterLevelMask(int FilterIndex) const { return m_lFilters[FilterIndex].m_FilterInfo.m_ServerLevel; }
	void SetFilterLevelMask(int FilterIndex, int Mask);
	
	// stats
	const void *GetID(int FilterIndex, int Index) const { return &m_lFilters[FilterIndex].m_pSortedServerlist[Index]; }
	int GetIndex(int FilterIndex, int Index) const { return m_lFilters[FilterIndex].m_pSortedServerlist[Index]; }
	int GetNumSortedServers(int FilterIndex) const { return m_lFilters[FilterIndex].m_NumSortedServers; }
	int GetNumSortedPlayers(int FilterIndex) const { return m_lFilters[FilterIndex].m_NumSortedPlayers; }

	// UI-facing helpers
	void GetDisplayCounts(int FilterIndex, int Index, int *pNum, int *pMax) const { m_lFilters[FilterIndex].GetDisplayCounts(Index, pNum, pMax); }
	bool IsClientHidden(int FilterIndex, int Index, int ClientIndex) const { return m_lFilters[FilterIndex].IsClientHidden(Index, ClientIndex); }

	// ---- Semantic filter state API ----
	bool GetFilterFlag(int FilterIndex, int Flag) const { return (m_lFilters[FilterIndex].m_FilterInfo.m_SortHash & Flag) != 0; }
	void SetFilterFlag(int FilterIndex, int Flag, bool Enabled);

	int GetFilterPing(int FilterIndex) const { return m_lFilters[FilterIndex].m_FilterInfo.m_Ping; }
	void SetFilterPing(int FilterIndex, int Ping);

	void GetFilterAddress(int FilterIndex, char *pBuf, int Size) const { str_copy(pBuf, m_lFilters[FilterIndex].m_FilterInfo.m_aAddress, Size); }
	void SetFilterAddress(int FilterIndex, const char *pAddress);

	bool GetFilterCountryEnabled(int FilterIndex) const { return (m_lFilters[FilterIndex].m_FilterInfo.m_SortHash & IServerBrowser::FILTER_COUNTRY) != 0; }
	void SetFilterCountryEnabled(int FilterIndex, bool Enabled);
	int GetFilterCountry(int FilterIndex) const { return m_lFilters[FilterIndex].m_FilterInfo.m_Country; }
	void SetFilterCountry(int FilterIndex, int Country);

	bool IsLevelFiltered(int FilterIndex, int Level) const { return m_lFilters[FilterIndex].m_FilterInfo.IsLevelFiltered(Level) != 0; }
	void ToggleLevelFilter(int FilterIndex, int Level);

	int GetNumGametypeFilters(int FilterIndex) const;
	void GetGametypeFilter(int FilterIndex, int Idx, char *pName, int NameSize, bool *pExclusive) const;
	void AddGametypeFilter(int FilterIndex, const char *pName, bool Exclusive);
	void RemoveGametypeFilter(int FilterIndex, int Idx);
	void ClearGametypeFilters(int FilterIndex);

private:
	class CConfig *m_pConfig;
	class IFriends *m_pFriends;
	char m_aNetVersion[128];
	array<CServerFilter> m_lFilters;
	int m_aActiveFilters[IServerBrowser::NUM_TYPES];

	// get updated on sort
	class CServerEntry **m_ppServerlist;
	int m_NumServers;
};

#endif
