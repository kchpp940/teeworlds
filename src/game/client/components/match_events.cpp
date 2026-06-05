/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <generated/client_data.h>

#include <game/client/gameclient.h>
#include "match_events.h"

void CMatchEvents::CPlayerMatchStats::Reset()
{
	m_IngameTicks = 0;
	m_Kills = 0;
	m_Deaths = 0;
	m_Suicides = 0;
	m_BestSpree = 0;
	m_CurrentSpree = 0;
	for(int j = 0; j < NUM_WEAPONS; j++)
	{
		m_aKillsWith[j] = 0;
		m_aDeathsFrom[j] = 0;
	}
	m_FlagGrabs = 0;
	m_FlagCaptures = 0;
	m_CarriersKilled = 0;
	m_KillsCarrying = 0;
	m_DeathsCarrying = 0;
}

CMatchEvents::CMatchEvents()
{
	OnReset();
}

void CMatchEvents::OnReset()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aPlayerStats[i].Reset();

	m_KillEventCount = 0;
	m_KillEventNext = 0;
	m_RaceFinishEventCount = 0;
	m_RaceFinishEventNext = 0;

	m_LastFlagCarrierRed = -1;
	m_LastFlagCarrierBlue = -1;
}

bool CMatchEvents::IsCarryingFlag(int ClientID, int FlagCarrierRed, int FlagCarrierBlue)
{
	return ClientID == FlagCarrierRed || ClientID == FlagCarrierBlue;
}

bool CMatchEvents::IsFlagCarrier(int ClientID) const
{
	return IsCarryingFlag(ClientID, m_LastFlagCarrierRed, m_LastFlagCarrierBlue);
}

const CMatchEvents::CKillEvent *CMatchEvents::GetKillEvent(int Index) const
{
	if(Index < 0 || Index >= NumKillEvents())
		return 0;
	return &m_aKillEvents[(m_KillEventNext + MAX_KILL_EVENTS - NumKillEvents() + Index) % MAX_KILL_EVENTS];
}

const CMatchEvents::CRaceFinishEvent *CMatchEvents::GetRaceFinishEvent(int Index) const
{
	if(Index < 0 || Index >= NumRaceFinishEvents())
		return 0;
	return &m_aRaceFinishEvents[(m_RaceFinishEventNext + MAX_RACE_EVENTS - NumRaceFinishEvents() + Index) % MAX_RACE_EVENTS];
}

void CMatchEvents::OnMessage(int MsgType, void *pRawMsg)
{
	if(m_pClient->m_SuppressEvents)
		return;

	int FlagCarrierRed = -1;
	int FlagCarrierBlue = -1;
	if(m_pClient->m_Snap.m_pGameDataFlag)
	{
		FlagCarrierRed = m_pClient->m_Snap.m_pGameDataFlag->m_FlagCarrierRed;
		FlagCarrierBlue = m_pClient->m_Snap.m_pGameDataFlag->m_FlagCarrierBlue;
	}

	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;

		CKillEvent Kill;
		Kill.m_Tick = Client()->GameTick();
		Kill.m_VictimID = pMsg->m_Victim;
		Kill.m_KillerID = pMsg->m_Killer;
		Kill.m_Weapon = pMsg->m_Weapon;
		Kill.m_ModeSpecial = pMsg->m_ModeSpecial;
		Kill.m_TeamSwitch = (pMsg->m_Weapon == -3);

		Kill.m_VictimCarryingFlag = IsCarryingFlag(pMsg->m_Victim, FlagCarrierRed, FlagCarrierBlue);
		Kill.m_KillerCarryingFlag = IsCarryingFlag(pMsg->m_Killer, FlagCarrierRed, FlagCarrierBlue);
		Kill.m_SameTeamKill = (pMsg->m_Victim != pMsg->m_Killer &&
			m_pClient->m_aClients[pMsg->m_Victim].m_Team == m_pClient->m_aClients[pMsg->m_Killer].m_Team &&
			m_pClient->m_aClients[pMsg->m_Victim].m_Team != TEAM_SPECTATORS);

		m_aKillEvents[m_KillEventNext] = Kill;
		m_KillEventNext = (m_KillEventNext + 1) % MAX_KILL_EVENTS;
		m_KillEventCount++;

		if(!Kill.m_TeamSwitch)
			m_aPlayerStats[Kill.m_VictimID].m_Deaths++;
		m_aPlayerStats[Kill.m_VictimID].m_CurrentSpree = 0;
		if(Kill.m_Weapon >= 0)
			m_aPlayerStats[Kill.m_VictimID].m_aDeathsFrom[Kill.m_Weapon]++;
		if((Kill.m_ModeSpecial & 1) && !Kill.m_TeamSwitch)
			m_aPlayerStats[Kill.m_VictimID].m_DeathsCarrying++;

		if(Kill.m_VictimID != Kill.m_KillerID)
		{
			m_aPlayerStats[Kill.m_KillerID].m_Kills++;
			m_aPlayerStats[Kill.m_KillerID].m_CurrentSpree++;

			if(m_aPlayerStats[Kill.m_KillerID].m_CurrentSpree > m_aPlayerStats[Kill.m_KillerID].m_BestSpree)
				m_aPlayerStats[Kill.m_KillerID].m_BestSpree = m_aPlayerStats[Kill.m_KillerID].m_CurrentSpree;
			if(Kill.m_Weapon >= 0)
				m_aPlayerStats[Kill.m_KillerID].m_aKillsWith[Kill.m_Weapon]++;
			if(Kill.m_ModeSpecial & 1)
				m_aPlayerStats[Kill.m_KillerID].m_CarriersKilled++;
			if(Kill.m_ModeSpecial & 2)
				m_aPlayerStats[Kill.m_KillerID].m_KillsCarrying++;
		}
		else if(!Kill.m_TeamSwitch)
			m_aPlayerStats[Kill.m_VictimID].m_Suicides++;
	}
	else if(MsgType == NETMSGTYPE_SV_RACEFINISH)
	{
		CNetMsg_Sv_RaceFinish *pMsg = (CNetMsg_Sv_RaceFinish *)pRawMsg;

		CRaceFinishEvent Finish;
		Finish.m_Tick = Client()->GameTick();
		Finish.m_ClientID = pMsg->m_ClientID;
		Finish.m_Time = pMsg->m_Time;
		Finish.m_Diff = pMsg->m_Diff;
		Finish.m_RecordPersonal = pMsg->m_RecordPersonal;
		Finish.m_RecordServer = pMsg->m_RecordServer;

		m_aRaceFinishEvents[m_RaceFinishEventNext] = Finish;
		m_RaceFinishEventNext = (m_RaceFinishEventNext + 1) % MAX_RACE_EVENTS;
		m_RaceFinishEventCount++;
	}
}

void CMatchEvents::OnFlagGrab(int ClientID)
{
	m_aPlayerStats[ClientID].m_FlagGrabs++;
}

void CMatchEvents::OnFlagCapture(int ClientID)
{
	m_aPlayerStats[ClientID].m_FlagCaptures++;
}

void CMatchEvents::OnPlayerEnter(int ClientID, int Team)
{
	m_aPlayerStats[ClientID].Reset();
}

void CMatchEvents::OnPlayerLeave(int ClientID)
{
	m_aPlayerStats[ClientID].Reset();
}

void CMatchEvents::UpdatePlayTime(int Ticks)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_pClient->m_aClients[i].m_Active && m_pClient->m_aClients[i].m_Team != TEAM_SPECTATORS)
			m_aPlayerStats[i].m_IngameTicks += Ticks;
	}
}

void CMatchEvents::OnMatchStart()
{
	OnReset();
}
