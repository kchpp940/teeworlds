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
	{
		m_aPlayerStats[i].Reset();
		m_aPlayerActivity[i].m_Score = 0;
		m_aPlayerActivity[i].m_Latency = 0;
		m_aPlayerActivity[i].m_PlayerFlags = 0;
		m_aPlayerActivity[i].m_Active = false;
	}

	m_KillEventCount = 0;
	m_KillEventNext = 0;
	m_KillEventGeneration = 0;
	m_RaceFinishEventCount = 0;
	m_RaceFinishEventNext = 0;
	m_RaceFinishEventGeneration = 0;
	m_CheckpointEventCount = 0;
	m_CheckpointEventNext = 0;
	m_CheckpointEventGeneration = 0;

	m_LastFlagCarrierRed = -1;
	m_LastFlagCarrierBlue = -1;
	m_FlagDropTickRed = 0;
	m_FlagDropTickBlue = 0;
	m_FlagStateRed = 0;
	m_FlagStateBlue = 0;

	m_aTeamState[0].m_Score = 0;
	m_aTeamState[0].m_Size = 0;
	m_aTeamState[0].m_AliveCount = 0;
	m_aTeamState[1].m_Score = 0;
	m_aTeamState[1].m_Size = 0;
	m_aTeamState[1].m_AliveCount = 0;

	m_GameStartTick = 0;
	m_GameStateFlags = 0;
	m_GameStateEndTick = 0;
	m_SnapNotReadyCount = 0;

	m_RaceBestTime = -1;
	m_RaceFlags = 0;

	m_NumSpectators = 0;
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

const CMatchEvents::CCheckpointEvent *CMatchEvents::GetCheckpointEvent(int Index) const
{
	if(Index < 0 || Index >= NumCheckpointEvents())
		return 0;
	return &m_aCheckpointEvents[(m_CheckpointEventNext + MAX_CHECKPOINT_EVENTS - NumCheckpointEvents() + Index) % MAX_CHECKPOINT_EVENTS];
}

const CMatchEvents::CCheckpointEvent *CMatchEvents::LatestCheckpoint(int ClientID) const
{
	for(int i = NumCheckpointEvents() - 1; i >= 0; i--)
	{
		const CCheckpointEvent *pEvent = GetCheckpointEvent(i);
		if(pEvent && pEvent->m_ClientID == ClientID)
			return pEvent;
	}
	return 0;
}

void CMatchEvents::OnNewSnapshot()
{
	if(m_pClient->m_Snap.m_pGameData)
	{
		m_GameStartTick = m_pClient->m_Snap.m_pGameData->m_GameStartTick;
		m_GameStateFlags = m_pClient->m_Snap.m_pGameData->m_GameStateFlags;
		m_GameStateEndTick = m_pClient->m_Snap.m_pGameData->m_GameStateEndTick;
	}
	else
	{
		m_GameStartTick = 0;
		m_GameStateFlags = 0;
		m_GameStateEndTick = 0;
	}

	m_SnapNotReadyCount = m_pClient->m_Snap.m_NotReadyCount;

	if(m_pClient->m_Snap.m_pGameDataFlag)
	{
		m_LastFlagCarrierRed = m_pClient->m_Snap.m_pGameDataFlag->m_FlagCarrierRed;
		m_LastFlagCarrierBlue = m_pClient->m_Snap.m_pGameDataFlag->m_FlagCarrierBlue;
		m_FlagDropTickRed = m_pClient->m_Snap.m_pGameDataFlag->m_FlagDropTickRed;
		m_FlagDropTickBlue = m_pClient->m_Snap.m_pGameDataFlag->m_FlagDropTickBlue;
		m_FlagStateRed = (m_LastFlagCarrierRed >= 0) ? FLAG_TAKEN : (m_FlagDropTickRed != 0 ? FLAG_DROPPED : FLAG_ATSTAND);
		m_FlagStateBlue = (m_LastFlagCarrierBlue >= 0) ? FLAG_TAKEN : (m_FlagDropTickBlue != 0 ? FLAG_DROPPED : FLAG_ATSTAND);
	}
	else
	{
		m_LastFlagCarrierRed = -1;
		m_LastFlagCarrierBlue = -1;
		m_FlagDropTickRed = 0;
		m_FlagDropTickBlue = 0;
		m_FlagStateRed = 0;
		m_FlagStateBlue = 0;
	}

	if(m_pClient->m_Snap.m_pGameDataTeam)
	{
		m_aTeamState[TEAM_RED].m_Score = m_pClient->m_Snap.m_pGameDataTeam->m_TeamscoreRed;
		m_aTeamState[TEAM_BLUE].m_Score = m_pClient->m_Snap.m_pGameDataTeam->m_TeamscoreBlue;
	}
	else
	{
		m_aTeamState[TEAM_RED].m_Score = 0;
		m_aTeamState[TEAM_BLUE].m_Score = 0;
	}

	m_aTeamState[TEAM_RED].m_Size = m_pClient->m_GameInfo.m_aTeamSize[TEAM_RED];
	m_aTeamState[TEAM_BLUE].m_Size = m_pClient->m_GameInfo.m_aTeamSize[TEAM_BLUE];
	m_aTeamState[TEAM_RED].m_AliveCount = m_pClient->m_Snap.m_AliveCount[TEAM_RED];
	m_aTeamState[TEAM_BLUE].m_AliveCount = m_pClient->m_Snap.m_AliveCount[TEAM_BLUE];

	if(m_pClient->m_Snap.m_pGameDataRace)
	{
		m_RaceBestTime = m_pClient->m_Snap.m_pGameDataRace->m_BestTime;
		m_RaceFlags = m_pClient->m_Snap.m_pGameDataRace->m_RaceFlags;
	}
	else
	{
		m_RaceBestTime = -1;
		m_RaceFlags = 0;
	}

	int NumSpec = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aPlayerActivity[i].m_Active = m_pClient->m_aClients[i].m_Active;
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_apPlayerInfos[i];
		if(pInfo && m_aPlayerActivity[i].m_Active)
		{
			m_aPlayerActivity[i].m_Score = pInfo->m_Score;
			m_aPlayerActivity[i].m_Latency = pInfo->m_Latency;
			m_aPlayerActivity[i].m_PlayerFlags = pInfo->m_PlayerFlags;
		}
		else
		{
			m_aPlayerActivity[i].m_Score = 0;
			m_aPlayerActivity[i].m_Latency = 0;
			m_aPlayerActivity[i].m_PlayerFlags = 0;
		}

		if(m_aPlayerActivity[i].m_Active && m_pClient->m_aClients[i].m_Team == TEAM_SPECTATORS)
			NumSpec++;
	}
	m_NumSpectators = NumSpec;
}

void CMatchEvents::OnMessage(int MsgType, void *pRawMsg)
{
	if(m_pClient->m_SuppressEvents)
		return;

	int FlagCarrierRed = m_LastFlagCarrierRed;
	int FlagCarrierBlue = m_LastFlagCarrierBlue;

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
		m_KillEventGeneration++;

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
		m_RaceFinishEventGeneration++;
	}
	else if(MsgType == NETMSGTYPE_SV_CHECKPOINT)
	{
		CNetMsg_Sv_Checkpoint *pMsg = (CNetMsg_Sv_Checkpoint *)pRawMsg;

		CCheckpointEvent Checkpoint;
		Checkpoint.m_Tick = Client()->GameTick();
		Checkpoint.m_TimeStamp = time_get();
		Checkpoint.m_ClientID = m_pClient->m_LocalClientID;
		Checkpoint.m_Diff = pMsg->m_Diff;

		m_aCheckpointEvents[m_CheckpointEventNext] = Checkpoint;
		m_CheckpointEventNext = (m_CheckpointEventNext + 1) % MAX_CHECKPOINT_EVENTS;
		m_CheckpointEventCount++;
		m_CheckpointEventGeneration++;
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
