/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MATCH_EVENTS_H
#define GAME_CLIENT_COMPONENTS_MATCH_EVENTS_H

#include <game/client/component.h>

class CMatchEvents : public CComponent
{
public:
	enum
	{
		MAX_KILL_EVENTS = 32,
		MAX_RACE_EVENTS = 16,
		MAX_CHECKPOINT_EVENTS = 16,

		FLAG_MISSING = -1,
		FLAG_ATSTAND = 0,
		FLAG_TAKEN = 1,
		FLAG_DROPPED = 2,
	};

	struct CKillEvent
	{
		int m_Tick;
		int m_VictimID;
		int m_KillerID;
		int m_Weapon;
		int m_ModeSpecial;
		bool m_TeamSwitch;

		bool m_VictimCarryingFlag;
		bool m_KillerCarryingFlag;
		bool m_SameTeamKill;
	};

	struct CRaceFinishEvent
	{
		int m_Tick;
		int m_ClientID;
		int m_Time;
		int m_Diff;
		bool m_RecordPersonal;
		bool m_RecordServer;
	};

	struct CCheckpointEvent
	{
		int m_Tick;
		int64 m_TimeStamp;
		int m_ClientID;
		int m_Diff;
	};

	struct CTeamState
	{
		int m_Score;
		int m_Size;
		int m_AliveCount;
	};

	struct CPlayerActivityState
	{
		int m_Score;
		int m_Latency;
		int m_PlayerFlags;
		bool m_Active;
	};

	class CPlayerMatchStats
	{
	public:
		CPlayerMatchStats() { Reset(); }

		void Reset();

		int m_Kills;
		int m_Deaths;
		int m_Suicides;
		int m_BestSpree;
		int m_CurrentSpree;
		int m_aKillsWith[NUM_WEAPONS];
		int m_aDeathsFrom[NUM_WEAPONS];

		int m_FlagGrabs;
		int m_FlagCaptures;
		int m_CarriersKilled;
		int m_KillsCarrying;
		int m_DeathsCarrying;

		int m_IngameTicks;
	};

private:
	CKillEvent m_aKillEvents[MAX_KILL_EVENTS];
	int m_KillEventCount;
	int m_KillEventNext;
	int m_KillEventGeneration;

	CRaceFinishEvent m_aRaceFinishEvents[MAX_RACE_EVENTS];
	int m_RaceFinishEventCount;
	int m_RaceFinishEventNext;
	int m_RaceFinishEventGeneration;

	CCheckpointEvent m_aCheckpointEvents[MAX_CHECKPOINT_EVENTS];
	int m_CheckpointEventCount;
	int m_CheckpointEventNext;
	int m_CheckpointEventGeneration;

	CPlayerMatchStats m_aPlayerStats[MAX_CLIENTS];
	CPlayerActivityState m_aPlayerActivity[MAX_CLIENTS];

	int m_LastFlagCarrierRed;
	int m_LastFlagCarrierBlue;
	int m_PrevFlagCarrierRed;
	int m_PrevFlagCarrierBlue;
	int m_FlagDropTickRed;
	int m_FlagDropTickBlue;
	int m_FlagStateRed;
	int m_FlagStateBlue;

	CTeamState m_aTeamState[2];

	int m_GameStartTick;
	int m_PrevGameStartTick;
	int m_GameStateFlags;
	int m_PrevGameStateFlags;
	int m_GameStateEndTick;
	int m_SnapNotReadyCount;

	int m_RaceBestTime;
	int m_RaceFlags;

	int m_NumSpectators;

	static bool IsCarryingFlag(int ClientID, int FlagCarrierRed, int FlagCarrierBlue);

public:
	CMatchEvents();

	virtual void OnReset();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual void OnNewSnapshot();

	const CKillEvent *GetKillEvent(int Index) const;
	int NumKillEvents() const { return m_KillEventCount > MAX_KILL_EVENTS ? MAX_KILL_EVENTS : m_KillEventCount; }
	int KillEventGeneration() const { return m_KillEventGeneration; }

	const CRaceFinishEvent *GetRaceFinishEvent(int Index) const;
	int NumRaceFinishEvents() const { return m_RaceFinishEventCount > MAX_RACE_EVENTS ? MAX_RACE_EVENTS : m_RaceFinishEventCount; }
	int RaceFinishEventGeneration() const { return m_RaceFinishEventGeneration; }

	const CCheckpointEvent *GetCheckpointEvent(int Index) const;
	int NumCheckpointEvents() const { return m_CheckpointEventCount > MAX_CHECKPOINT_EVENTS ? MAX_CHECKPOINT_EVENTS : m_CheckpointEventCount; }
	int CheckpointEventGeneration() const { return m_CheckpointEventGeneration; }

	const CCheckpointEvent *LatestCheckpoint(int ClientID) const;

	const CPlayerMatchStats *GetPlayerStats(int ClientID) const { return &m_aPlayerStats[ClientID]; }
	CPlayerMatchStats *PlayerStats(int ClientID) { return &m_aPlayerStats[ClientID]; }

	const CPlayerActivityState *GetPlayerActivity(int ClientID) const { return &m_aPlayerActivity[ClientID]; }

	void OnFlagGrab(int ClientID);
	void OnFlagCapture(int ClientID);
	void OnPlayerEnter(int ClientID, int Team);
	void OnPlayerLeave(int ClientID);
	void UpdatePlayTime(int Ticks);
	void OnMatchStart();

	bool IsFlagCarrier(int ClientID) const;
	bool IsFlagCarrierRed(int ClientID) const { return m_LastFlagCarrierRed == ClientID; }
	bool IsFlagCarrierBlue(int ClientID) const { return m_LastFlagCarrierBlue == ClientID; }
	int GetFlagCarrierRed() const { return m_LastFlagCarrierRed; }
	int GetFlagCarrierBlue() const { return m_LastFlagCarrierBlue; }
	int GetFlagDropTickRed() const { return m_FlagDropTickRed; }
	int GetFlagDropTickBlue() const { return m_FlagDropTickBlue; }
	int GetFlagStateRed() const { return m_FlagStateRed; }
	int GetFlagStateBlue() const { return m_FlagStateBlue; }

	int TeamScore(int Team) const { return m_aTeamState[Team].m_Score; }
	int TeamSize(int Team) const { return m_aTeamState[Team].m_Size; }
	int TeamAliveCount(int Team) const { return m_aTeamState[Team].m_AliveCount; }

	int GameStartTick() const { return m_GameStartTick; }
	int GameStateFlags() const { return m_GameStateFlags; }
	int GameStateEndTick() const { return m_GameStateEndTick; }
	int SnapNotReadyCount() const { return m_SnapNotReadyCount; }

	int RaceBestTime() const { return m_RaceBestTime; }
	int RaceFlags() const { return m_RaceFlags; }

	int NumSpectators() const { return m_NumSpectators; }
};

#endif
