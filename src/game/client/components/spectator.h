/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SPECTATOR_H
#define GAME_CLIENT_COMPONENTS_SPECTATOR_H
#include <base/vmath.h>

#include <game/client/component.h>

class CSpectator : public CComponent
{
	enum
	{
		NO_SELECTION=-1,
		MAX_SPEC_AUTO_FOLLOW_CANDIDATES = 16,
		MAX_FOLLOW_REASON_LEN = 64,
	};

	enum EFollowReason
	{
		REASON_NONE = 0,
		REASON_FLAG_CARRIER,
		REASON_FLAG_CARRIER_SCORING,
		REASON_FLAG_AT_STAND,
		REASON_RECENT_KILL,
		REASON_RECENT_DEATH,
		REASON_HIGH_SCORE,
		REASON_ACTIVE_PLAYER,
		REASON_KILLED_FLAG_CARRIER,
	};

	struct CPlayerEvent
	{
		int64 m_LastKillTime;
		int64 m_LastDeathTime;
		int64 m_LastKillCarrierTime;
		int64 m_LastFlagGrabTime;
		int m_RecentKills;
		int m_RecentDeaths;
	};

	struct CFollowCandidate
	{
		int m_ClientID;
		int m_Score;
		int m_Kills;
		int m_Deaths;
		int m_FlagState;
		int m_FlagTeam;
		bool m_Alive;
		bool m_Active;
		float m_DistanceToScore;
		EFollowReason m_Reason;
		float m_Priority;
	};

	struct CFollowInfo
	{
		int m_ClientID;
		EFollowReason m_Reason;
		int64 m_StartTime;
	};

	bool m_Active;
	bool m_WasActive;

	int m_SelectedSpectatorID;
	int m_SelectedSpecMode;
	vec2 m_SelectorMouse;

	bool m_AutoFollowActive;
	bool m_AutoFollowPaused;
	int64 m_AutoFollowPauseUntil;
	int m_LastFollowedClientID;
	int64 m_LastFollowChangeTime;
	int64 m_NextFollowEvalTime;

	CFollowInfo m_CurrentFollow;
	float m_CurrentFollowPriority;
	CPlayerEvent m_aPlayerEvents[MAX_CLIENTS];

	int m_LastFlagCarrierRed;
	int m_LastFlagCarrierBlue;
	vec2 m_RedFlagStandPos;
	vec2 m_BlueFlagStandPos;
	bool m_HasFlagStandPositions;

	bool CanSpectate();
	bool SpecModePossible(int SpecMode, int SpectatorID);
	void HandleSpectateNextPrev(int Direction);

	static void ConKeySpectator(IConsole::IResult *pResult, void *pUserData);
	static void ConSpectate(IConsole::IResult *pResult, void *pUserData);
	static void ConSpectateNext(IConsole::IResult *pResult, void *pUserData);
	static void ConSpectatePrevious(IConsole::IResult *pResult, void *pUserData);
	static void ConSpecAutoFollow(IConsole::IResult *pResult, void *pUserData);
	static void ConSpecAutoFollowToggle(IConsole::IResult *pResult, void *pUserData);

	void OnAutoFollow();
	int FindBestFollowTarget();
	void EvaluateCandidatesCTF(CFollowCandidate *pCandidates, int &NumCandidates);
	void EvaluateCandidatesDM(CFollowCandidate *pCandidates, int &NumCandidates);
	void SortCandidates(CFollowCandidate *pCandidates, int NumCandidates);
	void PauseAutoFollow();
	bool IsTargetValid(int ClientID);
	bool CanSwitchFollowTarget(int NewTargetID);
	bool ShouldSwitchTarget(int NewTargetID, float NewTargetPriority, EFollowReason NewTargetReason);
	bool IsHighPriorityReason(EFollowReason Reason);
	void UpdateFlagStates();
	void RecordKill(int KillerID, int VictimID, int ModeSpecial);
	void RecordFlagGrab(int ClientID, int Team);
	static const char *FollowReasonToString(EFollowReason Reason);

public:
	CSpectator();

	virtual void OnConsoleInit();
	virtual bool OnCursorMove(float x, float y, int CursorType);
	virtual void OnRender();
	virtual void OnRelease();
	virtual void OnReset();
	virtual void OnRefreshSkins();
	virtual void OnMessage(int MsgType, void *pRawMsg);

	void SendSpectate(int SpecMode, int SpectatorID, bool Manual = false);
	bool IsAutoFollowActive() const { return m_AutoFollowActive; }
	bool IsAutoFollowPaused() const { return m_AutoFollowPaused; }
	int GetCurrentFollowClientID() const { return m_CurrentFollow.m_ClientID; }
	EFollowReason GetCurrentFollowReason() const { return m_CurrentFollow.m_Reason; }
	const char *GetCurrentFollowReasonString() const { return FollowReasonToString(m_CurrentFollow.m_Reason); }
};

#endif
