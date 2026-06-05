/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/localization.h>
#include <game/client/render.h>

#include "spectator.h"

bool CSpectator::CanSpectate()
{
	return m_pClient->m_Snap.m_SpecInfo.m_Active
		&& (Client()->State() != IClient::STATE_DEMOPLAYBACK || DemoPlayer()->GetDemoType() == IDemoPlayer::DEMOTYPE_SERVER);
}

void CSpectator::ConKeySpectator(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	if(pSelf->CanSpectate())
		pSelf->m_Active = pResult->GetInteger(0) != 0;
}

void CSpectator::ConSpectate(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	if(pSelf->CanSpectate())
		pSelf->SendSpectate(pResult->GetInteger(0), pResult->GetInteger(1), true);
}

bool CSpectator::SpecModePossible(int SpecMode, int SpectatorID)
{
	switch(SpecMode)
	{
	case SPEC_PLAYER:
		if(!m_pClient->m_aClients[SpectatorID].m_Active || m_pClient->m_aClients[SpectatorID].m_Team == TEAM_SPECTATORS)
		{
			return false;
		}
		if(m_pClient->m_LocalClientID != -1
			&& m_pClient->m_aClients[m_pClient->m_LocalClientID].m_Team != TEAM_SPECTATORS
			&& (SpectatorID == m_pClient->m_LocalClientID
				|| m_pClient->m_aClients[m_pClient->m_LocalClientID].m_Team != m_pClient->m_aClients[SpectatorID].m_Team
				|| (m_pClient->m_Snap.m_apPlayerInfos[SpectatorID] && (m_pClient->m_Snap.m_apPlayerInfos[SpectatorID]->m_PlayerFlags&PLAYERFLAG_DEAD))))
		{
			return false;
		}
		return true;
	case SPEC_FLAGRED:
	case SPEC_FLAGBLUE:
		return m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_FLAGS;
	case SPEC_FREEVIEW:
		return m_pClient->m_LocalClientID == -1 || m_pClient->m_aClients[m_pClient->m_LocalClientID].m_Team == TEAM_SPECTATORS;
	default:
		dbg_assert(false, "invalid spec mode");
		return false;
	}
}

static void IterateSpecMode(int Direction, int *pSpecMode, int *pSpectatorID)
{
	dbg_assert(Direction == -1 || Direction == 1, "invalid direction");
	if(*pSpecMode == SPEC_PLAYER)
	{
		*pSpectatorID += Direction;
		if(0 <= *pSpectatorID && *pSpectatorID < MAX_CLIENTS)
		{
			return;
		}
		*pSpectatorID = -1;
	}
	*pSpecMode = (*pSpecMode + Direction + NUM_SPECMODES) % NUM_SPECMODES;
	if(*pSpecMode == SPEC_PLAYER)
	{
		*pSpectatorID = 0;
		if(Direction == -1)
		{
			*pSpectatorID = MAX_CLIENTS - 1;
		}
	}
}

void CSpectator::HandleSpectateNextPrev(int Direction)
{
	if(!m_pClient->m_Snap.m_SpecInfo.m_Active || (Client()->State() == IClient::STATE_DEMOPLAYBACK && DemoPlayer()->GetDemoType() != IDemoPlayer::DEMOTYPE_SERVER))
		return;

	int NewSpecMode = m_pClient->m_Snap.m_SpecInfo.m_SpecMode;
	int NewSpectatorID = -1;
	if(NewSpecMode == SPEC_PLAYER)
	{
		NewSpectatorID = m_pClient->m_Snap.m_SpecInfo.m_SpectatorID;
	}

	// Ensure the loop terminates even if no spec modes are possible.
	for(int i = 0; i < NUM_SPECMODES + MAX_CLIENTS; i++)
	{
		IterateSpecMode(Direction, &NewSpecMode, &NewSpectatorID);
		if(SpecModePossible(NewSpecMode, NewSpectatorID))
		{
			SendSpectate(NewSpecMode, NewSpectatorID, true);
			return;
		}
	}
}

void CSpectator::ConSpectateNext(IConsole::IResult *pResult, void *pUserData)
{
	((CSpectator *)pUserData)->HandleSpectateNextPrev(1);
}

void CSpectator::ConSpectatePrevious(IConsole::IResult *pResult, void *pUserData)
{
	((CSpectator *)pUserData)->HandleSpectateNextPrev(-1);
}

CSpectator::CSpectator()
{
	OnReset();
}

void CSpectator::ConSpecAutoFollow(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	pSelf->m_AutoFollowActive = pResult->GetInteger(0) != 0;
	if(pSelf->m_AutoFollowActive)
	{
		pSelf->m_AutoFollowPaused = false;
		pSelf->m_AutoFollowPauseUntil = 0;
	}
}

void CSpectator::ConSpecAutoFollowToggle(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	pSelf->m_AutoFollowActive = !pSelf->m_AutoFollowActive;
	if(pSelf->m_AutoFollowActive)
	{
		pSelf->m_AutoFollowPaused = false;
		pSelf->m_AutoFollowPauseUntil = 0;
	}
}

void CSpectator::OnConsoleInit()
{
	Console()->Register("+spectate", "", CFGFLAG_CLIENT, ConKeySpectator, this, "Open spectator mode selector");
	Console()->Register("spectate", "i[mode] i[target]", CFGFLAG_CLIENT, ConSpectate, this, "Switch spectator mode");
	Console()->Register("spectate_next", "", CFGFLAG_CLIENT, ConSpectateNext, this, "Spectate the next player");
	Console()->Register("spectate_previous", "", CFGFLAG_CLIENT, ConSpectatePrevious, this, "Spectate the previous player");
	Console()->Register("spec_auto_follow", "i[value]", CFGFLAG_CLIENT, ConSpecAutoFollow, this, "Enable/disable auto follow key players");
	Console()->Register("spec_auto_follow_toggle", "", CFGFLAG_CLIENT, ConSpecAutoFollowToggle, this, "Toggle auto follow key players");
}

bool CSpectator::OnCursorMove(float x, float y, int CursorType)
{
	if(!m_Active)
		return false;

	UI()->ConvertCursorMove(&x, &y, CursorType);
	m_SelectorMouse += vec2(x, y);
	return true;
}

void CSpectator::OnRelease()
{
	OnReset();
}

void CSpectator::OnRender()
{
	if(!m_Active && CanSpectate())
	{
		OnAutoFollow();
	}

	if(!m_Active)
	{
		if(m_WasActive)
		{
			if(m_SelectedSpecMode != NO_SELECTION)
				SendSpectate(m_SelectedSpecMode, m_SelectedSpectatorID, true);
			m_WasActive = false;
		}
		return;
	}

	if(!m_pClient->m_Snap.m_SpecInfo.m_Active)
	{
		m_Active = false;
		m_WasActive = false;
		return;
	}

	m_WasActive = true;
	m_SelectedSpecMode = NO_SELECTION;
	m_SelectedSpectatorID = -1;

	int TotalCount = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(!SpecModePossible(SPEC_PLAYER, i))
			continue;
		TotalCount++;
	}

	int ColumnSize = 8;
	float ScaleX = 1.0f;
	float ScaleY = 1.0f;
	if(TotalCount > 16)
	{
		ColumnSize = 16;
		ScaleY = 0.5f;
	}
	if(TotalCount > 48)
		ScaleX = 2.0f;
	else if(TotalCount > 32)
		ScaleX = 1.5f;

	// draw background
	const float ScreenHeight = 400.0f * 3.0f;
	const float ScreenWidth = ScreenHeight * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0, 0, ScreenWidth, ScreenHeight);

	const float Height = 600.0f;
	const float Width = 600.0f;
	const float Margin = 20.0f;

	const vec2 CenterOffset(ScreenWidth / 2.0f, ScreenHeight / 2.0f);
	CUIRect BackgroundRect = {CenterOffset.x - Width / 2.0f * ScaleX, CenterOffset.y - Height / 2.0f, Width * ScaleX, Height};
	Graphics()->BlendNormal();
	BackgroundRect.Draw(vec4(0.0f, 0.0f, 0.0f, 0.3f), Margin);

	// clamp mouse position to selector area
	m_SelectorMouse.x = clamp(m_SelectorMouse.x, -Width / 2.0f * ScaleX + Margin, Width / 2.0f * ScaleX - Margin);
	m_SelectorMouse.y = clamp(m_SelectorMouse.y, -Height / 2.0f + Margin, Height / 2.0f - Margin);

	const float FontSize = 20.0f;

	// draw free-view selection
	if(m_pClient->m_LocalClientID == -1 || m_pClient->m_aClients[m_pClient->m_LocalClientID].m_Team == TEAM_SPECTATORS)
	{
		CUIRect FreeViewRect;
		FreeViewRect.x = CenterOffset.x - 280.0f;
		FreeViewRect.y = CenterOffset.y - 280.0f;
		FreeViewRect.w = 270.0f;
		FreeViewRect.h = 60.0f;
		if(m_pClient->m_Snap.m_SpecInfo.m_SpecMode == SPEC_FREEVIEW)
			FreeViewRect.Draw(vec4(1.0f, 1.0f, 1.0f, 0.25f), 10.0f);

		const bool Selected = FreeViewRect.Inside(m_SelectorMouse + CenterOffset);
		if(Selected)
			m_SelectedSpecMode = SPEC_FREEVIEW;

		TextRender()->TextColor(1.0f, 1.0f, 1.0f, Selected ? 1.0f : 0.5f);
		static CTextCursor s_FreeViewLabelCursor;
		s_FreeViewLabelCursor.m_Align = TEXTALIGN_ML;
		s_FreeViewLabelCursor.m_FontSize = FontSize;
		s_FreeViewLabelCursor.MoveTo(FreeViewRect.x + 40.0f, FreeViewRect.y + FreeViewRect.h / 2.0f);
		s_FreeViewLabelCursor.Reset((g_Localization.Version() << 1) | (Selected ? 1 : 0));
		TextRender()->TextOutlined(&s_FreeViewLabelCursor, Localize("Free-View"), -1);
	}

	// draw flag selection
	float x = Margin, y = -270.0f;
	if(m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_FLAGS)
	{
		for(int Flag = SPEC_FLAGRED; Flag <= SPEC_FLAGBLUE; ++Flag)
		{
			CUIRect FlagRect;
			FlagRect.x = CenterOffset.x + x - 10.0f;
			FlagRect.y = CenterOffset.y + y - 10.0f;
			FlagRect.w = 120.0f;
			FlagRect.h = 60.0f;
			if(m_pClient->m_Snap.m_SpecInfo.m_SpecMode == Flag)
				FlagRect.Draw(vec4(1.0f, 1.0f, 1.0f, 0.25f), 10.0f);

			const bool Selected = FlagRect.Inside(m_SelectorMouse + CenterOffset);
			if(Selected)
				m_SelectedSpecMode = Flag;

			const float Size = 60.0f / 1.5f + (Selected ? 12.0f : 8.0f);
			const vec2 FlagSize = vec2(Size / 2.0f, Size);
			const vec2 FlagPos = FlagRect.Center() - FlagSize / 2.0f;

			Graphics()->BlendNormal();
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
			Graphics()->QuadsBegin();
			RenderTools()->SelectSprite(Flag == SPEC_FLAGRED ? SPRITE_FLAG_RED : SPRITE_FLAG_BLUE);
			IGraphics::CQuadItem QuadItem(FlagPos.x, FlagPos.y, FlagSize.x, FlagSize.y);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();

			x += FlagRect.w + Margin;
		}
	}

	const float PlayerStartY = -210.0f + Margin * ScaleY;
	x = -Width / 2.0f * ScaleX + 30.0f, y = PlayerStartY;

	// draw player selection
	for(int i = 0, Count = 0; i < MAX_CLIENTS; ++i)
	{
		if(!SpecModePossible(SPEC_PLAYER, i))
			continue;

		if(Count != 0 && Count % ColumnSize == 0)
		{
			x += 290.0f;
			y = PlayerStartY;
		}
		Count++;

		CUIRect PlayerRect;
		PlayerRect.x = CenterOffset.x + x - 10.0f;
		PlayerRect.y = CenterOffset.y + y + 10.0f - Margin * ScaleY;
		PlayerRect.w = 270.0f;
		PlayerRect.h = 60.0f * ScaleY;
		if(m_pClient->m_Snap.m_SpecInfo.m_SpecMode == SPEC_PLAYER && m_pClient->m_Snap.m_SpecInfo.m_SpectatorID == i)
			PlayerRect.Draw(vec4(1.0f, 1.0f, 1.0f, 0.25f), 10.0f);

		const bool Selected = PlayerRect.Inside(m_SelectorMouse + CenterOffset);
		if(Selected)
		{
			m_SelectedSpecMode = SPEC_PLAYER;
			m_SelectedSpectatorID = i;
		}

		// carried flag
		float PosX = PlayerRect.x + PlayerRect.h / 2.0f;
		if(m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_FLAGS
			&& m_pClient->m_Snap.m_pGameDataFlag
			&& (m_pClient->m_Snap.m_pGameDataFlag->m_FlagCarrierRed == i || m_pClient->m_Snap.m_pGameDataFlag->m_FlagCarrierBlue == i))
		{
			Graphics()->BlendNormal();
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
			Graphics()->QuadsBegin();
			RenderTools()->SelectSprite(i == m_pClient->m_Snap.m_pGameDataFlag->m_FlagCarrierBlue ? SPRITE_FLAG_BLUE : SPRITE_FLAG_RED, SPRITE_FLAG_FLIP_X);
			IGraphics::CQuadItem QuadItem(PosX - PlayerRect.h / 4.0f, PlayerRect.y - PlayerRect.h * 0.05f, PlayerRect.h / 2.0f, PlayerRect.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		// tee
		CTeeRenderInfo TeeInfo = m_pClient->m_aClients[i].m_RenderInfo;
		TeeInfo.m_Size *= ScaleY;
		RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), vec2(PosX, PlayerRect.y + PlayerRect.h * 0.6f));
		PosX += PlayerRect.h / 2.0f;

		// client ID and name
		PosX += UI()->DrawClientID(FontSize, vec2(PosX, PlayerRect.y + PlayerRect.h / 2.0f - FontSize * 0.6f), i);
		if(Config()->m_ClShowsocial)
		{
			static CTextCursor s_PlayerNameCursor;
			s_PlayerNameCursor.m_Align = TEXTALIGN_ML;
			s_PlayerNameCursor.m_FontSize = FontSize;
			s_PlayerNameCursor.Reset();
			s_PlayerNameCursor.MoveTo(PosX, PlayerRect.y + PlayerRect.h / 2.0f);
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, Selected ? 1.0f : 0.5f);
			TextRender()->TextOutlined(&s_PlayerNameCursor, m_pClient->m_aClients[i].m_aName, -1);
		}

		y += PlayerRect.h;
	}
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

	RenderTools()->RenderCursor(m_SelectorMouse + CenterOffset, 48.0f);
}

void CSpectator::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_SelectedSpecMode = NO_SELECTION;
	m_SelectedSpectatorID = -1;
	m_AutoFollowActive = false;
	m_AutoFollowPaused = false;
	m_AutoFollowPauseUntil = 0;
	m_LastFollowedClientID = -1;
	m_LastFollowChangeTime = 0;
	m_NextFollowEvalTime = 0;
	m_LastFlagCarrierRed = FLAG_ATSTAND;
	m_LastFlagCarrierBlue = FLAG_ATSTAND;
	m_RedFlagStandPos = vec2(0, 0);
	m_BlueFlagStandPos = vec2(0, 0);
	m_HasFlagStandPositions = false;

	m_CurrentFollow.m_ClientID = -1;
	m_CurrentFollow.m_Reason = REASON_NONE;
	m_CurrentFollow.m_StartTime = 0;
	m_CurrentFollowPriority = 0.0f;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aPlayerEvents[i].m_LastKillTime = 0;
		m_aPlayerEvents[i].m_LastDeathTime = 0;
		m_aPlayerEvents[i].m_LastKillCarrierTime = 0;
		m_aPlayerEvents[i].m_LastFlagGrabTime = 0;
		m_aPlayerEvents[i].m_RecentKills = 0;
		m_aPlayerEvents[i].m_RecentDeaths = 0;
	}
}

void CSpectator::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;
		RecordKill(pMsg->m_Killer, pMsg->m_Victim, pMsg->m_ModeSpecial);
	}
}

void CSpectator::UpdateFlagStates()
{
	if(!m_pClient->m_Snap.m_pGameDataFlag || !m_pClient->m_Snap.m_apFlags[0] || !m_pClient->m_Snap.m_apFlags[1])
		return;

	int FlagCarrierRed = m_pClient->m_Snap.m_pGameDataFlag->m_FlagCarrierRed;
	int FlagCarrierBlue = m_pClient->m_Snap.m_pGameDataFlag->m_FlagCarrierBlue;

	if(!m_HasFlagStandPositions)
	{
		if(FlagCarrierRed == FLAG_ATSTAND)
		{
			m_RedFlagStandPos = vec2(m_pClient->m_Snap.m_apFlags[0]->m_X, m_pClient->m_Snap.m_apFlags[0]->m_Y);
		}
		if(FlagCarrierBlue == FLAG_ATSTAND)
		{
			m_BlueFlagStandPos = vec2(m_pClient->m_Snap.m_apFlags[1]->m_X, m_pClient->m_Snap.m_apFlags[1]->m_Y);
		}
		if(FlagCarrierRed == FLAG_ATSTAND && FlagCarrierBlue == FLAG_ATSTAND)
		{
			m_HasFlagStandPositions = true;
		}
	}

	if(m_LastFlagCarrierRed == FLAG_ATSTAND && FlagCarrierRed >= 0 && FlagCarrierRed < MAX_CLIENTS)
	{
		RecordFlagGrab(FlagCarrierRed, TEAM_RED);
	}
	if(m_LastFlagCarrierBlue == FLAG_ATSTAND && FlagCarrierBlue >= 0 && FlagCarrierBlue < MAX_CLIENTS)
	{
		RecordFlagGrab(FlagCarrierBlue, TEAM_BLUE);
	}

	m_LastFlagCarrierRed = FlagCarrierRed;
	m_LastFlagCarrierBlue = FlagCarrierBlue;
}

void CSpectator::RecordKill(int KillerID, int VictimID, int ModeSpecial)
{
	int64 Now = time_get();

	if(VictimID >= 0 && VictimID < MAX_CLIENTS)
	{
		m_aPlayerEvents[VictimID].m_LastDeathTime = Now;
		m_aPlayerEvents[VictimID].m_RecentDeaths++;
	}

	if(KillerID >= 0 && KillerID < MAX_CLIENTS && KillerID != VictimID)
	{
		m_aPlayerEvents[KillerID].m_LastKillTime = Now;
		m_aPlayerEvents[KillerID].m_RecentKills++;

		if(ModeSpecial & 1)
		{
			m_aPlayerEvents[KillerID].m_LastKillCarrierTime = Now;
		}
	}
}

void CSpectator::RecordFlagGrab(int ClientID, int Team)
{
	if(ClientID >= 0 && ClientID < MAX_CLIENTS)
	{
		m_aPlayerEvents[ClientID].m_LastFlagGrabTime = time_get();
	}
}

const char *CSpectator::FollowReasonToString(EFollowReason Reason)
{
	switch(Reason)
	{
	case REASON_FLAG_CARRIER: return "Flag Carrier";
	case REASON_FLAG_CARRIER_SCORING: return "Scoring Carrier";
	case REASON_FLAG_AT_STAND: return "Flag at Stand";
	case REASON_RECENT_KILL: return "Recent Kill";
	case REASON_RECENT_DEATH: return "Recent Death";
	case REASON_HIGH_SCORE: return "High Score";
	case REASON_ACTIVE_PLAYER: return "Active Player";
	case REASON_KILLED_FLAG_CARRIER: return "Killed Carrier";
	default: return "";
	}
}

void CSpectator::SendSpectate(int SpecMode, int SpectatorID, bool Manual)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		m_pClient->m_DemoSpecMode = clamp(SpecMode, 0, NUM_SPECMODES-1);
		m_pClient->m_DemoSpecID = clamp(SpectatorID, -1, MAX_CLIENTS-1);
		return;
	}

	if(m_pClient->m_Snap.m_SpecInfo.m_SpecMode == SpecMode && (SpecMode != SPEC_PLAYER || m_pClient->m_Snap.m_SpecInfo.m_SpectatorID == SpectatorID))
		return;

	if(Manual && Config()->m_ClSpecAutoFollowPause)
	{
		PauseAutoFollow();
	}

	if(!Manual && SpecMode == SPEC_PLAYER)
	{
		m_LastFollowedClientID = SpectatorID;
		m_LastFollowChangeTime = time_get();
	}

	CNetMsg_Cl_SetSpectatorMode Msg;
	Msg.m_SpecMode = SpecMode;
	Msg.m_SpectatorID = SpectatorID;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
}

void CSpectator::PauseAutoFollow()
{
	m_AutoFollowPaused = true;
	m_AutoFollowPauseUntil = time_get() + time_freq() * 15;
}

bool CSpectator::IsTargetValid(int ClientID)
{
	return SpecModePossible(SPEC_PLAYER, ClientID);
}

bool CSpectator::CanSwitchFollowTarget(int NewTargetID)
{
	if(m_AutoFollowPaused)
		return false;

	if(!Config()->m_ClSpecAutoFollow && !m_AutoFollowActive)
		return false;

	if(NewTargetID == m_pClient->m_Snap.m_SpecInfo.m_SpectatorID)
		return false;

	if(!IsTargetValid(NewTargetID))
		return false;

	int64 Now = time_get();
	if(Now < m_NextFollowEvalTime)
		return false;

	const int64 MinFollowTime = time_freq() * 5;
	if(m_CurrentFollow.m_ClientID != -1 && (Now - m_CurrentFollow.m_StartTime) < MinFollowTime)
	{
		if(IsTargetValid(m_CurrentFollow.m_ClientID))
			return false;
	}

	return true;
}

void CSpectator::OnRefreshSkins()
{
	if(!CanSpectate())
		return;

	OnAutoFollow();
}

void CSpectator::OnAutoFollow()
{
	int64 Now = time_get();

	UpdateFlagStates();

	if((Config()->m_ClSpecAutoFollow || m_AutoFollowActive) && m_AutoFollowPaused)
	{
		if(Now > m_AutoFollowPauseUntil)
		{
			m_AutoFollowPaused = false;
		}
		else
		{
			return;
		}
	}

	if(m_CurrentFollow.m_ClientID != -1 && !IsTargetValid(m_CurrentFollow.m_ClientID))
	{
		m_CurrentFollow.m_ClientID = -1;
		m_CurrentFollow.m_Reason = REASON_NONE;
	}

	if(Now < m_NextFollowEvalTime)
		return;

	m_NextFollowEvalTime = Now + time_freq() / 2;

	int BestTarget = FindBestFollowTarget();
	if(BestTarget == -1)
		return;

	if(CanSwitchFollowTarget(BestTarget))
	{
		SendSpectate(SPEC_PLAYER, BestTarget, false);
	}
}

bool CSpectator::IsHighPriorityReason(EFollowReason Reason)
{
	return Reason == REASON_FLAG_CARRIER
		|| Reason == REASON_FLAG_CARRIER_SCORING
		|| Reason == REASON_KILLED_FLAG_CARRIER;
}

bool CSpectator::ShouldSwitchTarget(int NewTargetID, float NewTargetPriority, EFollowReason NewTargetReason)
{
	if(m_CurrentFollow.m_ClientID == -1)
		return true;

	if(NewTargetID == m_CurrentFollow.m_ClientID)
		return false;

	if(!IsTargetValid(m_CurrentFollow.m_ClientID))
		return true;

	if(IsHighPriorityReason(NewTargetReason) && !IsHighPriorityReason(m_CurrentFollow.m_Reason))
		return true;

	const float PRIORITY_THRESHOLD = 500.0f;
	if(NewTargetPriority > m_CurrentFollowPriority + PRIORITY_THRESHOLD)
		return true;

	return false;
}

int CSpectator::FindBestFollowTarget()
{
	CFollowCandidate aCandidates[MAX_SPEC_AUTO_FOLLOW_CANDIDATES];
	int NumCandidates = 0;
	int64 Now = time_get();
	const int64 EventWindow = time_freq() * 10;

	float CurrentTargetPriority = 0.0f;
	bool CurrentTargetFound = false;

	for(int i = 0; i < MAX_CLIENTS && NumCandidates < MAX_SPEC_AUTO_FOLLOW_CANDIDATES; i++)
	{
		if(!SpecModePossible(SPEC_PLAYER, i))
			continue;

		aCandidates[NumCandidates].m_ClientID = i;
		aCandidates[NumCandidates].m_Score = 0;
		aCandidates[NumCandidates].m_Kills = 0;
		aCandidates[NumCandidates].m_Deaths = 0;
		aCandidates[NumCandidates].m_FlagState = 0;
		aCandidates[NumCandidates].m_FlagTeam = m_pClient->m_aClients[i].m_Team;
		aCandidates[NumCandidates].m_Alive = true;
		aCandidates[NumCandidates].m_Active = false;
		aCandidates[NumCandidates].m_DistanceToScore = 0.0f;
		aCandidates[NumCandidates].m_Reason = REASON_NONE;
		aCandidates[NumCandidates].m_Priority = 0.0f;

		if(m_pClient->m_Snap.m_apPlayerInfos[i])
		{
			aCandidates[NumCandidates].m_Score = m_pClient->m_Snap.m_apPlayerInfos[i]->m_Score;
			int Flags = m_pClient->m_Snap.m_apPlayerInfos[i]->m_PlayerFlags;
			aCandidates[NumCandidates].m_Alive = !(Flags & PLAYERFLAG_DEAD);
		}

		int64 LastAction = m_aPlayerEvents[i].m_LastKillTime;
		if(m_aPlayerEvents[i].m_LastDeathTime > LastAction) LastAction = m_aPlayerEvents[i].m_LastDeathTime;
		if(m_aPlayerEvents[i].m_LastKillCarrierTime > LastAction) LastAction = m_aPlayerEvents[i].m_LastKillCarrierTime;
		if(m_aPlayerEvents[i].m_LastFlagGrabTime > LastAction) LastAction = m_aPlayerEvents[i].m_LastFlagGrabTime;
		aCandidates[NumCandidates].m_Active = (Now - LastAction) < EventWindow;

		NumCandidates++;
	}

	if(NumCandidates == 0)
		return -1;

	if(m_pClient->m_GameInfo.m_GameFlags & GAMEFLAG_FLAGS)
	{
		EvaluateCandidatesCTF(aCandidates, NumCandidates);
	}
	else
	{
		EvaluateCandidatesDM(aCandidates, NumCandidates);
	}

	SortCandidates(aCandidates, NumCandidates);

	for(int i = 0; i < NumCandidates; i++)
	{
		if(aCandidates[i].m_ClientID == m_CurrentFollow.m_ClientID)
		{
			CurrentTargetPriority = aCandidates[i].m_Priority;
			CurrentTargetFound = true;
			break;
		}
	}

	m_CurrentFollowPriority = CurrentTargetFound ? CurrentTargetPriority : 0.0f;

	int BestTargetID = aCandidates[0].m_ClientID;
	float BestTargetPriority = aCandidates[0].m_Priority;
	EFollowReason BestTargetReason = aCandidates[0].m_Reason;

	if(ShouldSwitchTarget(BestTargetID, BestTargetPriority, BestTargetReason))
	{
		m_CurrentFollow.m_ClientID = BestTargetID;
		m_CurrentFollow.m_Reason = BestTargetReason;
		m_CurrentFollow.m_StartTime = time_get();
	}

	return m_CurrentFollow.m_ClientID;
}

void CSpectator::EvaluateCandidatesCTF(CFollowCandidate *pCandidates, int &NumCandidates)
{
	if(!m_pClient->m_Snap.m_pGameDataFlag || !m_pClient->m_Snap.m_apFlags[0] || !m_pClient->m_Snap.m_apFlags[1])
		return;

	int64 Now = time_get();
	const int64 EventWindow = time_freq() * 15;
	int FlagCarrierRed = m_pClient->m_Snap.m_pGameDataFlag->m_FlagCarrierRed;
	int FlagCarrierBlue = m_pClient->m_Snap.m_pGameDataFlag->m_FlagCarrierBlue;

	vec2 RedStandPos = m_RedFlagStandPos;
	vec2 BlueStandPos = m_BlueFlagStandPos;

	if(!m_HasFlagStandPositions)
	{
		RedStandPos = vec2(m_pClient->m_Snap.m_apFlags[0]->m_X, m_pClient->m_Snap.m_apFlags[0]->m_Y);
		BlueStandPos = vec2(m_pClient->m_Snap.m_apFlags[1]->m_X, m_pClient->m_Snap.m_apFlags[1]->m_Y);
	}

	float MapWidth = fabsf(BlueStandPos.x - RedStandPos.x);
	if(MapWidth < 100.0f) MapWidth = 2000.0f;

	bool RedFlagAtStand = (FlagCarrierRed == FLAG_ATSTAND);
	bool BlueFlagAtStand = (FlagCarrierBlue == FLAG_ATSTAND);

	for(int i = 0; i < NumCandidates; i++)
	{
		float Priority = 0.0f;
		int ClientID = pCandidates[i].m_ClientID;
		int PlayerTeam = pCandidates[i].m_FlagTeam;
		EFollowReason BestReason = REASON_NONE;

		if(ClientID == FlagCarrierRed || ClientID == FlagCarrierBlue)
		{
			Priority += 2000.0f;
			BestReason = REASON_FLAG_CARRIER;

			vec2 CarrierPos = m_pClient->GetCharPos(ClientID);
			bool IsRedCarrier = (ClientID == FlagCarrierRed);

			bool OwnFlagAtStand = IsRedCarrier ? RedFlagAtStand : BlueFlagAtStand;
			vec2 OwnStandPos = IsRedCarrier ? RedStandPos : BlueStandPos;

			if(OwnFlagAtStand)
			{
				float DistToOwnStand = distance(CarrierPos, OwnStandPos);

				if(DistToOwnStand < MapWidth * 0.3f)
				{
					float ScoringBonus = 1000.0f * (1.0f - DistToOwnStand / (MapWidth * 0.3f));
					Priority += ScoringBonus;
					BestReason = REASON_FLAG_CARRIER_SCORING;
				}
			}
		}
		else
		{
			if(PlayerTeam == TEAM_RED && BlueFlagAtStand && pCandidates[i].m_Alive)
			{
				vec2 PlayerPos = m_pClient->GetCharPos(ClientID);
				float DistToBlueFlag = distance(PlayerPos, BlueStandPos);
				if(DistToBlueFlag < MapWidth * 0.3f)
				{
					Priority += 200.0f * (1.0f - DistToBlueFlag / (MapWidth * 0.3f));
					BestReason = REASON_FLAG_AT_STAND;
				}
			}
			else if(PlayerTeam == TEAM_BLUE && RedFlagAtStand && pCandidates[i].m_Alive)
			{
				vec2 PlayerPos = m_pClient->GetCharPos(ClientID);
				float DistToRedFlag = distance(PlayerPos, RedStandPos);
				if(DistToRedFlag < MapWidth * 0.3f)
				{
					Priority += 200.0f * (1.0f - DistToRedFlag / (MapWidth * 0.3f));
					BestReason = REASON_FLAG_AT_STAND;
				}
			}
		}

		int64 TimeSinceKillCarrier = Now - m_aPlayerEvents[ClientID].m_LastKillCarrierTime;
		if(TimeSinceKillCarrier < EventWindow)
		{
			float Bonus = 800.0f * (1.0f - (float)TimeSinceKillCarrier / (float)EventWindow);
			Priority += Bonus;
			if(BestReason == REASON_NONE) BestReason = REASON_KILLED_FLAG_CARRIER;
		}

		int64 TimeSinceKill = Now - m_aPlayerEvents[ClientID].m_LastKillTime;
		if(TimeSinceKill < EventWindow)
		{
			float Bonus = 400.0f * (1.0f - (float)TimeSinceKill / (float)EventWindow);
			Priority += Bonus;
			if(BestReason == REASON_NONE) BestReason = REASON_RECENT_KILL;
		}

		int64 TimeSinceFlagGrab = Now - m_aPlayerEvents[ClientID].m_LastFlagGrabTime;
		if(TimeSinceFlagGrab < EventWindow)
		{
			float Bonus = 300.0f * (1.0f - (float)TimeSinceFlagGrab / (float)EventWindow);
			Priority += Bonus;
		}

		Priority += pCandidates[i].m_Score * 5.0f;
		if(pCandidates[i].m_Score > 0 && BestReason == REASON_NONE)
		{
			BestReason = REASON_HIGH_SCORE;
		}

		if(pCandidates[i].m_Alive)
		{
			Priority += 50.0f;
			if(pCandidates[i].m_Active && BestReason == REASON_NONE)
			{
				BestReason = REASON_ACTIVE_PLAYER;
			}
		}

		pCandidates[i].m_Priority = Priority;
		pCandidates[i].m_Reason = BestReason;
	}
}

void CSpectator::EvaluateCandidatesDM(CFollowCandidate *pCandidates, int &NumCandidates)
{
	int64 Now = time_get();
	const int64 EventWindow = time_freq() * 10;
	int Mode = Config()->m_ClSpecAutoFollowMode;

	for(int i = 0; i < NumCandidates; i++)
	{
		float Priority = 0.0f;
		int ClientID = pCandidates[i].m_ClientID;
		EFollowReason BestReason = REASON_NONE;

		if(Mode == 0 || Mode == 1)
		{
			Priority += pCandidates[i].m_Score * 10.0f;
			if(pCandidates[i].m_Score > 0)
			{
				BestReason = REASON_HIGH_SCORE;
			}
		}

		if(Mode == 0 || Mode == 2)
		{
			int64 TimeSinceKill = Now - m_aPlayerEvents[ClientID].m_LastKillTime;
			if(TimeSinceKill < EventWindow)
			{
				float Bonus = 500.0f * (1.0f - (float)TimeSinceKill / (float)EventWindow);
				Priority += Bonus;
				BestReason = REASON_RECENT_KILL;
			}

			int64 TimeSinceDeath = Now - m_aPlayerEvents[ClientID].m_LastDeathTime;
			if(TimeSinceDeath < EventWindow && TimeSinceDeath < time_freq() * 3)
			{
				float Bonus = 300.0f * (1.0f - (float)TimeSinceDeath / (float)EventWindow);
				Priority += Bonus;
				if(BestReason == REASON_NONE) BestReason = REASON_RECENT_DEATH;
			}

			if(pCandidates[i].m_Alive)
			{
				Priority += 100.0f;
				if(pCandidates[i].m_Active && BestReason == REASON_NONE)
				{
					BestReason = REASON_ACTIVE_PLAYER;
				}
			}
		}

		pCandidates[i].m_Priority = Priority;
		pCandidates[i].m_Reason = BestReason;
	}
}

void CSpectator::SortCandidates(CFollowCandidate *pCandidates, int NumCandidates)
{
	for(int i = 0; i < NumCandidates - 1; i++)
	{
		for(int j = i + 1; j < NumCandidates; j++)
		{
			if(pCandidates[j].m_Priority > pCandidates[i].m_Priority)
			{
				CFollowCandidate Temp = pCandidates[i];
				pCandidates[i] = pCandidates[j];
				pCandidates[j] = Temp;
			}
		}
	}
}
