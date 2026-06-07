/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <algorithm> // sort  TODO: remove this

#include <base/math.h>

#include <engine/shared/config.h>
#include <engine/client/contacts.h>
#include <engine/serverbrowser.h>

#include "serverbrowser_entry.h"
#include "serverbrowser_filter.h"

// ---- Built-in filter presets (defined in engine, UI never needs these) ----
static const CServerFilterInfo gs_FilterStandard = {IServerBrowser::FILTER_COMPAT_VERSION|IServerBrowser::FILTER_PURE|IServerBrowser::FILTER_PURE_MAP, 999, -1, 0, {{0}}, {0}, {0}};
static const CServerFilterInfo gs_FilterRace = {IServerBrowser::FILTER_COMPAT_VERSION, 999, -1, 0, {{"Race"}}, {false}, {0}};
static const CServerFilterInfo gs_FilterFavorites = {IServerBrowser::FILTER_COMPAT_VERSION|IServerBrowser::FILTER_FAVORITE, 999, -1, 0, {{0}}, {0}, {0}};
static const CServerFilterInfo gs_FilterAll = {IServerBrowser::FILTER_COMPAT_VERSION, 999, -1, 0, {{0}}, {0}, {0}};

static const CServerFilterInfo *GetPresetFilterInfo(int Preset)
{
	switch(Preset)
	{
	case IServerBrowser::PRESET_STANDARD: return &gs_FilterStandard;
	case IServerBrowser::PRESET_RACE: return &gs_FilterRace;
	case IServerBrowser::PRESET_FAVORITES: return &gs_FilterFavorites;
	case IServerBrowser::PRESET_ALL: return &gs_FilterAll;
	default: return &gs_FilterAll;
	}
}


class SortWrap
{
	typedef bool (CServerBrowserFilter::CServerFilter::*SortFunc)(int, int) const;
	SortFunc m_pfnSort;
	CServerBrowserFilter::CServerFilter *m_pThis;
public:
	SortWrap(CServerBrowserFilter::CServerFilter *t, SortFunc f) : m_pfnSort(f), m_pThis(t) {}
	bool operator()(int a, int b) { return (m_pThis->Config()->m_BrSortOrder ? (m_pThis->*m_pfnSort)(b, a) : (m_pThis->*m_pfnSort)(a, b)); }
};

//	CServerFilter
CServerBrowserFilter::CServerFilter::CServerFilter()
{
	m_pServerBrowserFilter = 0;
	m_Id = -1;
	m_Preset = IServerBrowser::PRESET_CUSTOM;
	m_aName[0] = 0;

	m_FilterInfo.m_SortHash = 0;
	m_FilterInfo.m_Ping = 0;
	m_FilterInfo.m_Country = 0;
	m_FilterInfo.m_ServerLevel = 0;
	for(int i = 0; i < CServerFilterInfo::MAX_GAMETYPES; ++i)
	{
		m_FilterInfo.m_aGametype[i][0] = 0;
		m_FilterInfo.m_aGametypeExclusive[i] = false;
	}
	m_FilterInfo.m_aAddress[0] = 0;

	m_NumSortedPlayers = 0;
	m_NumSortedServers = 0;
	m_SortedServersCapacity = 0;

	m_pSortedServerlist = 0;
}

CServerBrowserFilter::CServerFilter::~CServerFilter()
{
	if(m_pSortedServerlist)
		mem_free(m_pSortedServerlist);
}

CServerBrowserFilter::CServerFilter& CServerBrowserFilter::CServerFilter::operator=(const CServerBrowserFilter::CServerFilter& Other)
{
	if(&Other != this)
	{
		m_pServerBrowserFilter = Other.m_pServerBrowserFilter;
		m_Id = Other.m_Id;
		m_Preset = Other.m_Preset;
		str_copy(m_aName, Other.m_aName, sizeof(m_aName));
		m_FilterInfo.Set(&Other.m_FilterInfo);
		m_NumSortedPlayers = Other.m_NumSortedPlayers;
		m_NumSortedServers = Other.m_NumSortedServers;
		m_SortedServersCapacity = Other.m_SortedServersCapacity;

		if(m_pSortedServerlist)
			mem_free(m_pSortedServerlist);
		m_pSortedServerlist = (int *)mem_alloc(m_SortedServersCapacity * sizeof(int));
		for(int i = 0; i < m_SortedServersCapacity; ++i)
			m_pSortedServerlist[i] = Other.m_pSortedServerlist[i];
	}
	return *this;
}

void CServerBrowserFilter::CServerFilter::Filter()
{
	int NumServers = m_pServerBrowserFilter->m_NumServers;
	m_NumSortedServers = 0;
	m_NumSortedPlayers = 0;

	// allocate the sorted list
	if(m_SortedServersCapacity < NumServers)
	{
		if(m_pSortedServerlist)
			mem_free(m_pSortedServerlist);
		m_SortedServersCapacity = maximum(1000, NumServers+NumServers/2);
		m_pSortedServerlist = (int *)mem_alloc(m_SortedServersCapacity*sizeof(int));
	}

	// filter the servers
	for(int i = 0; i < NumServers; i++)
	{
		bool Filtered = false;

		int RelevantClientCount = (m_FilterInfo.m_SortHash&IServerBrowser::FILTER_SPECTATORS) ? m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_NumPlayers : m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_NumClients;
		int RelevantMaxCount = (m_FilterInfo.m_SortHash&IServerBrowser::FILTER_SPECTATORS) ? m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_MaxPlayers : m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_MaxClients;
		if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_BOTS)
		{
			RelevantClientCount -= m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_NumBotPlayers;
			RelevantMaxCount -= m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_NumBotPlayers;
			if(!(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_SPECTATORS))
			{
				RelevantClientCount -= m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_NumBotSpectators;
				RelevantMaxCount -= m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_NumBotSpectators;
			}
		}
		if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_SPECTATORS)
		{
			int SpecNum = m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_NumClients - m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_NumPlayers;
			if(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_MaxClients - m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_MaxPlayers < SpecNum)
				RelevantMaxCount -= SpecNum;
		}

		// store derived display fields so UI never has to recompute
		m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_NumDisplayClients = RelevantClientCount;
		m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_MaxDisplayClients = RelevantMaxCount;

		if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_EMPTY && RelevantClientCount == 0)
			Filtered = true;
		else if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_FULL && ((m_FilterInfo.m_SortHash&IServerBrowser::FILTER_SPECTATORS && m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_NumPlayers == m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_MaxPlayers) ||
				m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_NumClients == m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_MaxClients))
			Filtered = true;
		else if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_PW && m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_Flags&IServerBrowser::FLAG_PASSWORD)
			Filtered = true;
		else if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_FAVORITE && !m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_Favorite)
			Filtered = true;
		else if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_PURE && !(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_Flags&IServerBrowser::FLAG_PURE))
			Filtered = true;
		else if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_PURE_MAP &&  !(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_Flags&IServerBrowser::FLAG_PUREMAP))
			Filtered = true;
		else if(m_FilterInfo.m_Ping < m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_Latency)
			Filtered = true;
		else if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_COMPAT_VERSION && str_comp_num(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_aVersion, m_pServerBrowserFilter->m_aNetVersion, 3) != 0)
			Filtered = true;
		else if(m_FilterInfo.m_aAddress[0] && !str_find_nocase(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_aAddress, m_FilterInfo.m_aAddress))
			Filtered = true;
		else if(m_FilterInfo.IsLevelFiltered(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_ServerLevel))
			Filtered = true;
		else
		{
			if(m_FilterInfo.m_aGametype[0][0])
			{
				bool Excluded = false, DoInclude = false, Included = false;
				for(int Index = 0; Index < CServerFilterInfo::MAX_GAMETYPES; ++Index)
				{
					if(!m_FilterInfo.m_aGametype[Index][0])
						break;
					if(m_FilterInfo.m_aGametypeExclusive[Index])
					{
						if(!str_comp_nocase(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_aGameType, m_FilterInfo.m_aGametype[Index]))
						{
							Excluded = true;
							break;
						}
					}
					else
					{
						DoInclude = true;
						if(!str_comp_nocase(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_aGameType, m_FilterInfo.m_aGametype[Index]))
						{
							Included = true;
							break;
						}
					}
				}
				Filtered = Excluded || (DoInclude && !Included);
			}

			if(!Filtered && m_FilterInfo.m_SortHash&IServerBrowser::FILTER_COUNTRY)
			{
				Filtered = true;
				// match against player country
				for(int p = 0; p < m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_NumClients; p++)
				{
					if(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_aClients[p].m_Country == m_FilterInfo.m_Country)
					{
						Filtered = false;
						break;
					}
				}
			}

			if(!Filtered && Config()->m_BrFilterString[0] != 0)
			{
				m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_QuickSearchHit = 0;

				// match against server name
				if(str_find_nocase(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_aName, Config()->m_BrFilterString))
					m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_QuickSearchHit |= IServerBrowser::QUICK_SERVERNAME;

				// match against players
				for(int p = 0; p < m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_NumClients; p++)
				{
					if(str_find_nocase(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_aClients[p].m_aName, Config()->m_BrFilterString) ||
						str_find_nocase(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_aClients[p].m_aClan, Config()->m_BrFilterString))
					{
						m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_QuickSearchHit |= IServerBrowser::QUICK_PLAYER;
						break;
					}
				}

				// match against map
				if(str_find_nocase(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_aMap, Config()->m_BrFilterString))
					m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_QuickSearchHit |= IServerBrowser::QUICK_MAPNAME;

				// match against game type
				if(str_find_nocase(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_aGameType, Config()->m_BrFilterString))
					m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_QuickSearchHit |= IServerBrowser::QUICK_GAMETYPE;

				if(!m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_QuickSearchHit)
					Filtered = true;
			}
		}

		if(!Filtered)
		{
			// check for friend
			m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_FriendState = CContactInfo::CONTACT_NO;
			for(int p = 0; p < m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_NumClients; p++)
			{
				m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_aClients[p].m_FriendState = m_pServerBrowserFilter->m_pFriends->GetFriendState(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_aClients[p].m_aName,
					m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_aClients[p].m_aClan);
				m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_FriendState = maximum(m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_FriendState, m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_aClients[p].m_FriendState);
			}

			if(!(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_FRIENDS) || m_pServerBrowserFilter->m_ppServerlist[i]->m_Info.m_FriendState != CContactInfo::CONTACT_NO)
			{
				m_pSortedServerlist[m_NumSortedServers++] = i;
				m_NumSortedPlayers += RelevantClientCount;
			}
		}
	}
}

int CServerBrowserFilter::CServerFilter::GetSortHash() const
{
	int i = Config()->m_BrSort&0x7;
	i |= Config()->m_BrSortOrder<<3;
	if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_BOTS) i |= 1<<4;
	if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_EMPTY) i |= 1<<5;
	if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_FULL) i |= 1<<6;
	if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_SPECTATORS) i |= 1<<7;
	if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_FRIENDS) i |= 1<<8;
	if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_PW) i |= 1<<9;
	if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_FAVORITE) i |= 1<<10;
	if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_COMPAT_VERSION) i |= 1<<11;
	if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_PURE) i |= 1<<12;
	if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_PURE_MAP) i |= 1<<13;
	if(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_COUNTRY) i |= 1<<14;
	// fold quick-search string into hash so any change triggers a re-filter
	if(Config()->m_BrFilterString[0])
	{
		unsigned h = 2166136261u;
		for(const char *p = Config()->m_BrFilterString; *p; ++p)
			h = (h ^ (unsigned char)*p) * 16777619u;
		i ^= (int)(h & 0x7FFF) << 15;
	}
	return i;
}

void CServerBrowserFilter::CServerFilter::Sort()
{
	// create filtered list
	Filter();

	// sort
	switch(Config()->m_BrSort)
	{
	case IServerBrowser::SORT_NAME:
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist+m_NumSortedServers, SortWrap(this, &CServerBrowserFilter::CServerFilter::SortCompareName));
		break;
	case IServerBrowser::SORT_PING:
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist+m_NumSortedServers, SortWrap(this, &CServerBrowserFilter::CServerFilter::SortComparePing));
		break;
	case IServerBrowser::SORT_MAP:
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist+m_NumSortedServers, SortWrap(this, &CServerBrowserFilter::CServerFilter::SortCompareMap));
		break;
	case IServerBrowser::SORT_NUMPLAYERS:
		if(!(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_BOTS))
			std::stable_sort(m_pSortedServerlist, m_pSortedServerlist+m_NumSortedServers, SortWrap(this,
						(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_SPECTATORS) ? &CServerBrowserFilter::CServerFilter::SortCompareNumPlayers : &CServerBrowserFilter::CServerFilter::SortCompareNumClients));
		else
			std::stable_sort(m_pSortedServerlist, m_pSortedServerlist+m_NumSortedServers, SortWrap(this,
						(m_FilterInfo.m_SortHash&IServerBrowser::FILTER_SPECTATORS) ? &CServerBrowserFilter::CServerFilter::SortCompareNumRealPlayers : &CServerBrowserFilter::CServerFilter::SortCompareNumRealClients));
		break;
	case IServerBrowser::SORT_GAMETYPE:
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist+m_NumSortedServers, SortWrap(this, &CServerBrowserFilter::CServerFilter::SortCompareGametype));
	}

	m_FilterInfo.m_SortHash = GetSortHash();
}

bool CServerBrowserFilter::CServerFilter::SortCompareName(int Index1, int Index2) const
{
	CServerEntry *a = m_pServerBrowserFilter->m_ppServerlist[Index1];
	CServerEntry *b = m_pServerBrowserFilter->m_ppServerlist[Index2];
	//	make sure empty entries are listed last
	return (a->m_InfoState == CServerEntry::STATE_READY && b->m_InfoState == CServerEntry::STATE_READY) || (a->m_InfoState != CServerEntry::STATE_READY && b->m_InfoState != CServerEntry::STATE_READY) ? str_comp_nocase(a->m_Info.m_aName, b->m_Info.m_aName) < 0 :
			a->m_InfoState == CServerEntry::STATE_READY;
}

bool CServerBrowserFilter::CServerFilter::SortCompareMap(int Index1, int Index2) const
{
	CServerEntry *a = m_pServerBrowserFilter->m_ppServerlist[Index1];
	CServerEntry *b = m_pServerBrowserFilter->m_ppServerlist[Index2];
	int Result = str_comp_nocase(a->m_Info.m_aMap, b->m_Info.m_aMap);
	return Result < 0 || (Result == 0 && (a->m_Info.m_Flags&IServerBrowser::FLAG_PURE) && !(b->m_Info.m_Flags&IServerBrowser::FLAG_PURE));
}

bool CServerBrowserFilter::CServerFilter::SortComparePing(int Index1, int Index2) const
{
	CServerEntry *a = m_pServerBrowserFilter->m_ppServerlist[Index1];
	CServerEntry *b = m_pServerBrowserFilter->m_ppServerlist[Index2];
	return a->m_Info.m_Latency < b->m_Info.m_Latency ||
		(a->m_Info.m_Latency == b->m_Info.m_Latency && (a->m_Info.m_Flags&IServerBrowser::FLAG_PURE) && !(b->m_Info.m_Flags&IServerBrowser::FLAG_PURE));
}

bool CServerBrowserFilter::CServerFilter::SortCompareGametype(int Index1, int Index2) const
{
	CServerEntry *a = m_pServerBrowserFilter->m_ppServerlist[Index1];
	CServerEntry *b = m_pServerBrowserFilter->m_ppServerlist[Index2];
	return str_comp_nocase(a->m_Info.m_aGameType, b->m_Info.m_aGameType) < 0;
}

bool CServerBrowserFilter::CServerFilter::SortCompareNumPlayers(int Index1, int Index2) const
{
	CServerEntry *a = m_pServerBrowserFilter->m_ppServerlist[Index1];
	CServerEntry *b = m_pServerBrowserFilter->m_ppServerlist[Index2];
	return a->m_Info.m_NumPlayers < b->m_Info.m_NumPlayers ||
		(a->m_Info.m_NumPlayers == b->m_Info.m_NumPlayers && !(a->m_Info.m_Flags&IServerBrowser::FLAG_PURE) && (b->m_Info.m_Flags&IServerBrowser::FLAG_PURE));
}

bool CServerBrowserFilter::CServerFilter::SortCompareNumRealPlayers(int Index1, int Index2) const
{
	CServerEntry *a = m_pServerBrowserFilter->m_ppServerlist[Index1];
	CServerEntry *b = m_pServerBrowserFilter->m_ppServerlist[Index2];
	return (a->m_Info.m_NumPlayers - a->m_Info.m_NumBotPlayers) < (b->m_Info.m_NumPlayers - b->m_Info.m_NumBotPlayers) ||
		((a->m_Info.m_NumPlayers - a->m_Info.m_NumBotPlayers) == (b->m_Info.m_NumPlayers - b->m_Info.m_NumBotPlayers) && !(a->m_Info.m_Flags&IServerBrowser::FLAG_PURE) && (b->m_Info.m_Flags&IServerBrowser::FLAG_PURE));
}

bool CServerBrowserFilter::CServerFilter::SortCompareNumClients(int Index1, int Index2) const
{
	CServerEntry *a = m_pServerBrowserFilter->m_ppServerlist[Index1];
	CServerEntry *b = m_pServerBrowserFilter->m_ppServerlist[Index2];
	return a->m_Info.m_NumClients < b->m_Info.m_NumClients ||
		(a->m_Info.m_NumClients == b->m_Info.m_NumClients && !(a->m_Info.m_Flags&IServerBrowser::FLAG_PURE) && (b->m_Info.m_Flags&IServerBrowser::FLAG_PURE));
}

bool CServerBrowserFilter::CServerFilter::SortCompareNumRealClients(int Index1, int Index2) const
{
	CServerEntry *a = m_pServerBrowserFilter->m_ppServerlist[Index1];
	CServerEntry *b = m_pServerBrowserFilter->m_ppServerlist[Index2];
	return (a->m_Info.m_NumClients - a->m_Info.m_NumBotPlayers - a->m_Info.m_NumBotSpectators) < (b->m_Info.m_NumClients - b->m_Info.m_NumBotPlayers - b->m_Info.m_NumBotSpectators) ||
		((a->m_Info.m_NumClients - a->m_Info.m_NumBotPlayers - a->m_Info.m_NumBotSpectators) == (b->m_Info.m_NumClients - b->m_Info.m_NumBotPlayers - b->m_Info.m_NumBotSpectators) && !(a->m_Info.m_Flags&IServerBrowser::FLAG_PURE) && (b->m_Info.m_Flags&IServerBrowser::FLAG_PURE));
}

//	CServerBrowserFilter
void CServerBrowserFilter::Init(CConfig *pConfig, IFriends *pFriends, const char *pNetVersion)
{
	m_pConfig = pConfig;
	m_pFriends = pFriends;
	str_copy(m_aNetVersion, pNetVersion, sizeof(m_aNetVersion));
	for(int i = 0; i < IServerBrowser::NUM_TYPES; ++i)
		m_aActiveFilters[i] = -1;
	m_NextFilterId = 1;
}

int CServerBrowserFilter::GetFilterIndex(int FilterId) const
{
	for(int i = 0; i < m_lFilters.size(); ++i)
	{
		if(m_lFilters[i].m_Id == FilterId)
			return i;
	}
	return -1;
}

void CServerBrowserFilter::Clear()
{
	for(int i = 0; i < m_lFilters.size(); i++)
	{
		m_lFilters[i].m_NumSortedServers = 0;
		m_lFilters[i].m_NumSortedPlayers = 0;
	}
}

void CServerBrowserFilter::Sort(CServerEntry **ppServerlist, int NumServers, int ResortFlags)
{
	m_ppServerlist = ppServerlist;
	m_NumServers = NumServers;
	for(int i = 0; i < m_lFilters.size(); i++)
	{
		// check if we need to resort
		CServerFilter *pFilter = &m_lFilters[i];
		if((ResortFlags&RESORT_FLAG_FORCE) || ((ResortFlags&RESORT_FLAG_FAV) && pFilter->m_FilterInfo.m_SortHash&IServerBrowser::FILTER_FAVORITE) || pFilter->m_FilterInfo.m_SortHash != pFilter->GetSortHash())
			pFilter->Sort();
	}
}

void CServerFilterInfo::Set(const CServerFilterInfo *pSrc)
{
	m_SortHash = pSrc->m_SortHash;
	m_Ping = pSrc->m_Ping;
	m_Country = pSrc->m_Country;
	m_ServerLevel = pSrc->m_ServerLevel;
	for(int i = 0; i < CServerFilterInfo::MAX_GAMETYPES; ++i)
	{
		str_copy(m_aGametype[i], pSrc->m_aGametype[i], sizeof(m_aGametype[i]));
		m_aGametypeExclusive[i] = m_aGametype[i][0] && pSrc->m_aGametypeExclusive[i];
	}
	str_copy(m_aAddress, pSrc->m_aAddress, sizeof(m_aAddress));
}

int CServerBrowserFilter::AddFilter(const CServerFilterInfo *pFilterInfo)
{
	CServerFilter Filter;
	Filter.m_FilterInfo.Set(pFilterInfo);
	Filter.m_pSortedServerlist = 0;
	Filter.m_NumSortedPlayers = 0;
	Filter.m_NumSortedServers = 0;
	Filter.m_SortedServersCapacity = 0;
	Filter.m_pServerBrowserFilter = this;
	m_lFilters.add(Filter);

	return m_lFilters.size()-1;
}

void CServerBrowserFilter::GetFilter(int Index, CServerFilterInfo *pFilterInfo) const
{
	pFilterInfo->Set(&m_lFilters[Index].m_FilterInfo);
}

void CServerBrowserFilter::SetFilter(int Index, const CServerFilterInfo *pFilterInfo)
{
	CServerFilter *pFilter = &m_lFilters[Index];
	pFilter->m_FilterInfo.Set(pFilterInfo);
	pFilter->Sort();
}

void CServerBrowserFilter::RemoveFilter(int Index)
{
	m_lFilters.remove_index(Index);
}

// ---- Semantic filter state API implementations ----

void CServerBrowserFilter::SetFilterFlag(int FilterId, int Flag, bool Enabled)
{
	const int Idx = GetFilterIndex(FilterId);
	if(Idx < 0) return;
	CServerFilter *pFilter = &m_lFilters[Idx];
	if(Enabled)
		pFilter->m_FilterInfo.m_SortHash |= Flag;
	else
		pFilter->m_FilterInfo.m_SortHash &= ~Flag;
	pFilter->Sort();
}

void CServerBrowserFilter::SetFilterPing(int FilterId, int Ping)
{
	const int Idx = GetFilterIndex(FilterId);
	if(Idx < 0) return;
	CServerFilter *pFilter = &m_lFilters[Idx];
	pFilter->m_FilterInfo.m_Ping = Ping;
	pFilter->Sort();
}

void CServerBrowserFilter::SetFilterAddress(int FilterId, const char *pAddress)
{
	const int Idx = GetFilterIndex(FilterId);
	if(Idx < 0) return;
	CServerFilter *pFilter = &m_lFilters[Idx];
	str_copy(pFilter->m_FilterInfo.m_aAddress, pAddress, sizeof(pFilter->m_FilterInfo.m_aAddress));
	pFilter->Sort();
}

void CServerBrowserFilter::SetFilterCountry(int FilterId, int Country)
{
	const int Idx = GetFilterIndex(FilterId);
	if(Idx < 0) return;
	CServerFilter *pFilter = &m_lFilters[Idx];
	pFilter->m_FilterInfo.m_Country = Country;
	pFilter->Sort();
}

void CServerBrowserFilter::ToggleLevelFilter(int FilterId, int Level)
{
	const int Idx = GetFilterIndex(FilterId);
	if(Idx < 0) return;
	CServerFilter *pFilter = &m_lFilters[Idx];
	pFilter->m_FilterInfo.ToggleLevel(Level);
	pFilter->Sort();
}

int CServerBrowserFilter::GetNumGametypeFilters(int FilterId) const
{
	const int Idx = GetFilterIndex(FilterId);
	if(Idx < 0) return 0;
	const CServerFilterInfo &Info = m_lFilters[Idx].m_FilterInfo;
	int Count = 0;
	for(int i = 0; i < CServerFilterInfo::MAX_GAMETYPES; ++i)
	{
		if(Info.m_aGametype[i][0])
			Count++;
		else
			break;
	}
	return Count;
}

void CServerBrowserFilter::GetGametypeFilter(int FilterId, int Idx2, char *pName, int NameSize, bool *pExclusive) const
{
	const int Idx = GetFilterIndex(FilterId);
	if(Idx < 0)
	{
		if(pName && NameSize > 0)
			pName[0] = 0;
		if(pExclusive)
			*pExclusive = false;
		return;
	}
	const CServerFilterInfo &Info = m_lFilters[Idx].m_FilterInfo;
	if(Idx2 >= 0 && Idx2 < CServerFilterInfo::MAX_GAMETYPES && Info.m_aGametype[Idx2][0])
	{
		if(pName)
			str_copy(pName, Info.m_aGametype[Idx2], NameSize);
		if(pExclusive)
			*pExclusive = Info.m_aGametypeExclusive[Idx2] != 0;
	}
	else
	{
		if(pName && NameSize > 0)
			pName[0] = 0;
		if(pExclusive)
			*pExclusive = false;
	}
}

void CServerBrowserFilter::AddGametypeFilter(int FilterId, const char *pName, bool Exclusive)
{
	const int Idx = GetFilterIndex(FilterId);
	if(Idx < 0) return;
	CServerFilter *pFilter = &m_lFilters[Idx];
	for(int i = 0; i < CServerFilterInfo::MAX_GAMETYPES; ++i)
	{
		if(!pFilter->m_FilterInfo.m_aGametype[i][0])
		{
			str_copy(pFilter->m_FilterInfo.m_aGametype[i], pName, sizeof(pFilter->m_FilterInfo.m_aGametype[i]));
			pFilter->m_FilterInfo.m_aGametypeExclusive[i] = Exclusive;
			pFilter->Sort();
			return;
		}
	}
}

void CServerBrowserFilter::RemoveGametypeFilter(int FilterId, int Idx2)
{
	const int Idx = GetFilterIndex(FilterId);
	if(Idx < 0) return;
	CServerFilter *pFilter = &m_lFilters[Idx];
	if(Idx2 < 0 || Idx2 >= CServerFilterInfo::MAX_GAMETYPES)
		return;
	if(Idx2 == CServerFilterInfo::MAX_GAMETYPES - 1 || !pFilter->m_FilterInfo.m_aGametype[Idx2 + 1][0])
	{
		pFilter->m_FilterInfo.m_aGametype[Idx2][0] = 0;
		pFilter->m_FilterInfo.m_aGametypeExclusive[Idx2] = false;
	}
	else
	{
		int j = Idx2;
		for(; j < CServerFilterInfo::MAX_GAMETYPES - 1 && pFilter->m_FilterInfo.m_aGametype[j + 1][0]; ++j)
		{
			str_copy(pFilter->m_FilterInfo.m_aGametype[j], pFilter->m_FilterInfo.m_aGametype[j + 1], sizeof(pFilter->m_FilterInfo.m_aGametype[j]));
			pFilter->m_FilterInfo.m_aGametypeExclusive[j] = pFilter->m_FilterInfo.m_aGametypeExclusive[j + 1];
		}
		pFilter->m_FilterInfo.m_aGametype[j][0] = 0;
		pFilter->m_FilterInfo.m_aGametypeExclusive[j] = false;
	}
	pFilter->Sort();
}

void CServerBrowserFilter::ClearGametypeFilters(int FilterId)
{
	const int Idx = GetFilterIndex(FilterId);
	if(Idx < 0) return;
	CServerFilter *pFilter = &m_lFilters[Idx];
	for(int i = 0; i < CServerFilterInfo::MAX_GAMETYPES; ++i)
	{
		pFilter->m_FilterInfo.m_aGametype[i][0] = 0;
		pFilter->m_FilterInfo.m_aGametypeExclusive[i] = false;
	}
	pFilter->Sort();
}

void CServerBrowserFilter::CServerFilter::GetDisplayCounts(int Index, int *pNum, int *pMax) const
{
	if(Index < 0 || Index >= m_pServerBrowserFilter->m_NumServers)
	{
		if(pNum) *pNum = 0;
		if(pMax) *pMax = 0;
		return;
	}
	const CServerInfo &Info = m_pServerBrowserFilter->m_ppServerlist[Index]->m_Info;
	if(pNum) *pNum = Info.m_NumDisplayClients;
	if(pMax) *pMax = Info.m_MaxDisplayClients;
}

bool CServerBrowserFilter::CServerFilter::IsClientHidden(int Index, int ClientIndex) const
{
	if(Index < 0 || Index >= m_pServerBrowserFilter->m_NumServers)
		return true;
	const CServerInfo &Info = m_pServerBrowserFilter->m_ppServerlist[Index]->m_Info;
	if(ClientIndex < 0 || ClientIndex >= Info.m_NumClients)
		return true;
	return (m_FilterInfo.m_SortHash & IServerBrowser::FILTER_BOTS) != 0
		&& (Info.m_aClients[ClientIndex].m_PlayerType & CServerInfo::CClient::PLAYERFLAG_BOT) != 0;
}

// ---- Filter presets and metadata API implementations ----

int CServerBrowserFilter::AddFilterFromPreset(int Preset, const char *pName)
{
	const CServerFilterInfo *pPresetInfo = GetPresetFilterInfo(Preset);
	int FilterIndex = AddFilter(pPresetInfo);
	m_lFilters[FilterIndex].m_Id = m_NextFilterId++;
	m_lFilters[FilterIndex].m_Preset = Preset;
	str_copy(m_lFilters[FilterIndex].m_aName, pName, sizeof(m_lFilters[FilterIndex].m_aName));
	return m_lFilters[FilterIndex].m_Id;
}

void CServerBrowserFilter::ResetFilterToPreset(int FilterId)
{
	const int Idx = GetFilterIndex(FilterId);
	if(Idx < 0) return;
	CServerFilter *pFilter = &m_lFilters[Idx];
	const CServerFilterInfo *pPresetInfo = GetPresetFilterInfo(pFilter->m_Preset);
	pFilter->m_FilterInfo.Set(pPresetInfo);
	pFilter->Sort();
}

void CServerBrowserFilter::SetFilterName(int FilterId, const char *pName)
{
	const int Idx = GetFilterIndex(FilterId);
	if(Idx < 0) return;
	str_copy(m_lFilters[Idx].m_aName, pName, sizeof(m_lFilters[Idx].m_aName));
}

// ---- Aggregated persistence getters/setters ----

void CServerBrowserFilter::SetFilterFlags(int FilterId, int Flags)
{
	const int Idx = GetFilterIndex(FilterId);
	if(Idx < 0) return;
	CServerFilter *pFilter = &m_lFilters[Idx];
	const int FilterFlagsMask = 0xFFFF;
	pFilter->m_FilterInfo.m_SortHash = (pFilter->m_FilterInfo.m_SortHash & ~FilterFlagsMask) | (Flags & FilterFlagsMask);
	pFilter->Sort();
}

void CServerBrowserFilter::SetFilterLevelMask(int FilterId, int Mask)
{
	const int Idx = GetFilterIndex(FilterId);
	if(Idx < 0) return;
	CServerFilter *pFilter = &m_lFilters[Idx];
	pFilter->m_FilterInfo.m_ServerLevel = Mask & ((1 << CServerInfo::NUM_SERVER_LEVELS) - 1);
	pFilter->Sort();
}

// ---- Filter store in-memory operations ----

void CServerBrowserFilter::EnsureDefaultFilters()
{
	int Filters = 0;
	for(int i = 0; i < m_lFilters.size(); i++)
		Filters |= 1 << m_lFilters[i].m_Preset;

	if((Filters & (1 << IServerBrowser::PRESET_STANDARD)) == 0)
	{
		const int NewFilterId = AddFilterFromPreset(IServerBrowser::PRESET_STANDARD, "Teeworlds");
		for(int Pos = m_lFilters.size() - 1; Pos > 0; --Pos)
			MoveFilter(GetFilterId(Pos), true);
		(void)NewFilterId;
	}

	if((Filters & (1 << IServerBrowser::PRESET_RACE)) == 0)
	{
		AddFilterFromPreset(IServerBrowser::PRESET_RACE, "Race");
		for(int Pos = m_lFilters.size() - 1; Pos > 1; --Pos)
			MoveFilter(GetFilterId(Pos), true);
	}

	if((Filters & (1 << IServerBrowser::PRESET_FAVORITES)) == 0)
	{
		AddFilterFromPreset(IServerBrowser::PRESET_FAVORITES, "Favorites");
		for(int Pos = m_lFilters.size() - 1; Pos > 2; --Pos)
			MoveFilter(GetFilterId(Pos), true);
	}

	if((Filters & (1 << IServerBrowser::PRESET_ALL)) == 0)
	{
		AddFilterFromPreset(IServerBrowser::PRESET_ALL, "All");
		for(int Pos = m_lFilters.size() - 1; Pos > 3; --Pos)
			MoveFilter(GetFilterId(Pos), true);
	}

	const bool UseDefaultFilters = Filters == 0;
	if(UseDefaultFilters)
	{
		const int AllFilterId = GetFilterId(m_lFilters.size() - 1);
		for(int i = 0; i < IServerBrowser::NUM_TYPES; ++i)
			m_aActiveFilters[i] = AllFilterId;
	}
}

void CServerBrowserFilter::DeleteFilter(int FilterId)
{
	const int Idx = GetFilterIndex(FilterId);
	if(Idx < 0) return;
	RemoveFilter(Idx);
	// active filters store stable ids; only patch the one that just got deleted
	const int FallbackId = m_lFilters.size() > 0 ? m_lFilters[clamp(Idx, 0, (int)m_lFilters.size() - 1)].m_Id : -1;
	for(int i = 0; i < IServerBrowser::NUM_TYPES; ++i)
	{
		if(m_aActiveFilters[i] == FilterId)
			m_aActiveFilters[i] = FallbackId;
	}
}

void CServerBrowserFilter::MoveFilter(int FilterId, bool Up)
{
	const int FilterIndex = GetFilterIndex(FilterId);
	if(FilterIndex < 0) return;
	if(Up)
	{
		if(FilterIndex > 0)
		{
			CServerFilter Temp = m_lFilters[FilterIndex];
			m_lFilters[FilterIndex] = m_lFilters[FilterIndex - 1];
			m_lFilters[FilterIndex - 1] = Temp;
		}
	}
	else
	{
		if(FilterIndex < m_lFilters.size() - 1)
		{
			CServerFilter Temp = m_lFilters[FilterIndex];
			m_lFilters[FilterIndex] = m_lFilters[FilterIndex + 1];
			m_lFilters[FilterIndex + 1] = Temp;
		}
	}
	// active filters store stable ids; no adjustment needed when reordering
}
