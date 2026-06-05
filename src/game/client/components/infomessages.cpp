/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <generated/client_data.h>

#include <game/client/gameclient.h>
#include <game/client/animstate.h>
#include <game/client/components/match_events.h>
#include "infomessages.h"

#include "chat.h"
#include "skins.h"

void CInfoMessages::OnReset()
{
	m_InfoMsgCurrent = 0;
	for(int i = 0; i < MAX_INFOMSGS; i++)
		m_aInfoMsgs[i].m_Tick = -100000;
	m_LastKillEventGeneration = 0;
	m_LastRaceEventGeneration = 0;
}

void CInfoMessages::AddInfoMsg(int Type, CInfoMsg NewMsg)
{
	NewMsg.m_Type = Type;
	NewMsg.m_Tick = Client()->GameTick();

	m_InfoMsgCurrent = (m_InfoMsgCurrent+1)%MAX_INFOMSGS;
	m_aInfoMsgs[m_InfoMsgCurrent] = NewMsg;
}

void CInfoMessages::PrepareKillMsgFromEvent(const struct CMatchEvents::CKillEvent *pEvent)
{
	float Width = 400*3.0f*Graphics()->ScreenAspect();
	float Height = 400*3.0f;
	Graphics()->MapScreen(0, 0, Width*1.5f, Height*1.5f);

	bool Race = m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_RACE;

	if(Race && m_pClient->m_Snap.m_pGameDataRace && m_pClient->m_Snap.m_pGameDataRace->m_RaceFlags&RACEFLAG_HIDE_KILLMSG)
		return;

	CInfoMsg Kill;
	Kill.m_Player1ID = pEvent->m_VictimID;
	if(Config()->m_ClShowsocial)
	{
		Kill.m_Player1NameCursor.m_FontSize = 36.0f;
		TextRender()->TextDeferred(&Kill.m_Player1NameCursor, m_pClient->m_aClients[Kill.m_Player1ID].m_aName, -1);
	}

	Kill.m_Player1RenderInfo = m_pClient->m_aClients[Kill.m_Player1ID].m_RenderInfo;

	Kill.m_Player2ID = pEvent->m_KillerID;
	if(Kill.m_Player2ID >= 0)
	{
		if(Config()->m_ClShowsocial)
		{
			Kill.m_Player2NameCursor.m_FontSize = 36.0f;
			TextRender()->TextDeferred(&Kill.m_Player2NameCursor, m_pClient->m_aClients[Kill.m_Player2ID].m_aName, -1);
		}

		Kill.m_Player2RenderInfo = m_pClient->m_aClients[Kill.m_Player2ID].m_RenderInfo;
	}
	else
	{
		bool IsTeamplay = (m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_TEAMS) != 0;
		int KillerTeam = - 1 - Kill.m_Player2ID;
		int Skin = m_pClient->m_pSkins->Find("dummy", false);
		if(Skin != -1)
		{
			const CSkins::CSkin *pDummy = m_pClient->m_pSkins->Get(Skin);
			for(int p = 0; p < NUM_SKINPARTS; p++)
			{
				Kill.m_Player2RenderInfo.m_aTextures[p] = pDummy->m_apParts[p]->m_OrgTexture;
				if(IsTeamplay)
				{
					int ColorVal = m_pClient->m_pSkins->GetTeamColor(0, 0x000000, KillerTeam, p);
					Kill.m_Player2RenderInfo.m_aColors[p] = m_pClient->m_pSkins->GetColorV4(ColorVal, p==SKINPART_MARKING);
				}
				else
					Kill.m_Player2RenderInfo.m_aColors[p] = m_pClient->m_pSkins->GetColorV4(0x000000, p==SKINPART_MARKING);
				Kill.m_Player2RenderInfo.m_aColors[p].a *= .5f;
			}
			Kill.m_Player2RenderInfo.m_Size = 64.0f;
		}
	}

	Kill.m_Weapon = pEvent->m_Weapon;
	Kill.m_ModeSpecial = pEvent->m_ModeSpecial;
	Kill.m_FlagCarrierBlue = m_pClient->m_pMatchEvents->GetFlagCarrierBlue();

	AddInfoMsg(INFOMSG_KILL, Kill);
}

void CInfoMessages::PrepareFinishMsgFromEvent(const struct CMatchEvents::CRaceFinishEvent *pEvent)
{
	float Width = 400*3.0f*Graphics()->ScreenAspect();
	float Height = 400*3.0f;
	Graphics()->MapScreen(0, 0, Width*1.5f, Height*1.5f);

	char aBuf[256];
	char aTime[32];
	char aLabel[64];

	FormatTime(aTime, sizeof(aTime), pEvent->m_Time, m_pClient->RacePrecision());
	m_pClient->GetPlayerLabel(aLabel, sizeof(aLabel), pEvent->m_ClientID, m_pClient->m_aClients[pEvent->m_ClientID].m_aName);

	str_format(aBuf, sizeof(aBuf), "%2d: %s: finished in %s", pEvent->m_ClientID, m_pClient->m_aClients[pEvent->m_ClientID].m_aName, aTime);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "race", aBuf);

	if(pEvent->m_RecordPersonal || pEvent->m_RecordServer)
	{
		if(pEvent->m_RecordServer)
			str_format(aBuf, sizeof(aBuf), Localize("'%s' has set a new map record: %s"), aLabel, aTime);
		else
			str_format(aBuf, sizeof(aBuf), Localize("'%s' has set a new personal record: %s"), aLabel, aTime);
		
		if(pEvent->m_Diff < 0)
		{
			char aImprovement[64];
			char aDiff[32];
			FormatTimeDiff(aDiff, sizeof(aDiff), absolute(pEvent->m_Diff), m_pClient->RacePrecision(), false);
			str_format(aImprovement, sizeof(aImprovement), Localize(" (%s seconds faster)"), aDiff);
			str_append(aBuf, aImprovement, sizeof(aBuf));
		}

		m_pClient->m_pChat->AddLine(aBuf);
	}

	if(m_pClient->m_Snap.m_pGameDataRace && m_pClient->m_Snap.m_pGameDataRace->m_RaceFlags&RACEFLAG_FINISHMSG_AS_CHAT)
	{
		if(!pEvent->m_RecordPersonal && !pEvent->m_RecordServer)
		{
			str_format(aBuf, sizeof(aBuf), Localize("'%s' finished in: %s"), aLabel, aTime);
			m_pClient->m_pChat->AddLine(aBuf);
		}
	}
	else
	{
		CInfoMsg Finish;
		Finish.m_Player1ID = pEvent->m_ClientID;
		Finish.m_Player1RenderInfo = m_pClient->m_aClients[Finish.m_Player1ID].m_RenderInfo;
		
		Finish.m_TimeCursor.m_FontSize = 36.0f;
		if(pEvent->m_RecordServer)
			TextRender()->TextColor(1.0f, 0.5f, 0.0f, 1.0f);
		else if(pEvent->m_RecordPersonal)
			TextRender()->TextColor(0.2f, 0.6f, 1.0f, 1.0f);
		TextRender()->TextDeferred(&Finish.m_TimeCursor, aTime, -1);

		if(Config()->m_ClShowsocial)
		{
			Finish.m_Player1NameCursor.m_FontSize = 36.0f;
			TextRender()->TextDeferred(&Finish.m_Player1NameCursor, m_pClient->m_aClients[pEvent->m_ClientID].m_aName, -1);
		}

		FormatTimeDiff(aTime, sizeof(aTime), pEvent->m_Diff, m_pClient->RacePrecision());
		str_format(aBuf, sizeof(aBuf), "(%s)", aTime);
		Finish.m_DiffCursor.m_FontSize = 36.0f;
		if(pEvent->m_Diff < 0)
			TextRender()->TextColor(0.5f, 1.0f, 0.5f, 1.0f);
		else
			TextRender()->TextColor(1.0f, 0.5f, 0.5f, 1.0f);
		TextRender()->TextDeferred(&Finish.m_DiffCursor, aBuf, -1);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

		Finish.m_Time = pEvent->m_Time;
		Finish.m_Diff = pEvent->m_Diff;
		Finish.m_RecordPersonal = pEvent->m_RecordPersonal;
		Finish.m_RecordServer = pEvent->m_RecordServer;

		AddInfoMsg(INFOMSG_FINISH, Finish);
	}
}

void CInfoMessages::ProcessNewEvents()
{
	CMatchEvents *pME = m_pClient->m_pMatchEvents;
	bool Race = m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_RACE;

	int CurKillGen = pME->KillEventGeneration();
	if(CurKillGen != m_LastKillEventGeneration)
	{
		int Diff = CurKillGen - m_LastKillEventGeneration;
		int Start = pME->NumKillEvents() - Diff;
		if(Start < 0) Start = 0;
		for(int i = Start; i < pME->NumKillEvents(); i++)
		{
			const CMatchEvents::CKillEvent *pEvent = pME->GetKillEvent(i);
			if(pEvent)
				PrepareKillMsgFromEvent(pEvent);
		}
		m_LastKillEventGeneration = CurKillGen;
	}

	if(Race)
	{
		int CurRaceGen = pME->RaceFinishEventGeneration();
		if(CurRaceGen != m_LastRaceEventGeneration)
		{
			int Diff = CurRaceGen - m_LastRaceEventGeneration;
			int Start = pME->NumRaceFinishEvents() - Diff;
			if(Start < 0) Start = 0;
			for(int i = Start; i < pME->NumRaceFinishEvents(); i++)
			{
				const CMatchEvents::CRaceFinishEvent *pEvent = pME->GetRaceFinishEvent(i);
				if(pEvent)
					PrepareFinishMsgFromEvent(pEvent);
			}
			m_LastRaceEventGeneration = CurRaceGen;
		}
	}
}

void CInfoMessages::OnRender()
{
	if(!Config()->m_ClShowhud || Client()->State() < IClient::STATE_ONLINE)
		return;

	ProcessNewEvents();

	float Width = 400*3.0f*Graphics()->ScreenAspect();
	float Height = 400*3.0f;

	Graphics()->MapScreen(0, 0, Width*1.5f, Height*1.5f);
	float StartX = Width*1.5f-10.0f;
	float y = 20.0f;

	for(int i = 1; i <= MAX_INFOMSGS; i++)
	{
		CInfoMsg *pInfoMsg = &m_aInfoMsgs[(m_InfoMsgCurrent+i)%MAX_INFOMSGS];
		if(Client()->GameTick() > pInfoMsg->m_Tick + SERVER_TICK_SPEED * 10)
			continue;

		if(pInfoMsg->m_Type == INFOMSG_KILL)
			RenderKillMsg(pInfoMsg, StartX, y);
		else if(pInfoMsg->m_Type == INFOMSG_FINISH)
			RenderFinishMsg(pInfoMsg, StartX, y);

		y += 46.0f;
	}
}

void CInfoMessages::RenderKillMsg(CInfoMsg *pInfoMsg, float x, float y) const
{
	float FontSize = 36.0f;
	float KillerNameW = pInfoMsg->m_Player2NameCursor.Width() + UI()->GetClientIDRectWidth(FontSize);
	float VictimNameW = pInfoMsg->m_Player1NameCursor.Width() + UI()->GetClientIDRectWidth(FontSize);

	x -= VictimNameW;
	float AdvanceID = UI()->DrawClientID(pInfoMsg->m_Player1NameCursor.m_FontSize, vec2(x, y), pInfoMsg->m_Player1ID);
	pInfoMsg->m_Player1NameCursor.MoveTo(x + AdvanceID, y);
	TextRender()->DrawTextOutlined(&pInfoMsg->m_Player1NameCursor);

	x -= 24.0f;

	if(m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_FLAGS)
	{
		if(pInfoMsg->m_ModeSpecial&1)
		{
			Graphics()->BlendNormal();
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
			Graphics()->QuadsBegin();

			if(pInfoMsg->m_Player1ID == pInfoMsg->m_FlagCarrierBlue)
				RenderTools()->SelectSprite(SPRITE_FLAG_BLUE);
			else
				RenderTools()->SelectSprite(SPRITE_FLAG_RED);

			float Size = 56.0f;
			IGraphics::CQuadItem QuadItem(x, y-16, Size/2, Size);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}
	}

	RenderTools()->RenderTee(CAnimState::GetIdle(), &pInfoMsg->m_Player1RenderInfo, EMOTE_PAIN, vec2(-1,0), vec2(x, y+28));
	x -= 32.0f;

	x -= 44.0f;
	if(pInfoMsg->m_Weapon >= 0)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
		Graphics()->QuadsBegin();
		RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[pInfoMsg->m_Weapon].m_pSpriteBody);
		RenderTools()->DrawSprite(x, y+28, 96);
		Graphics()->QuadsEnd();
	}
	x -= 52.0f;

	if(pInfoMsg->m_Player1ID != pInfoMsg->m_Player2ID)
	{
		if(m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_FLAGS)
		{
			if(pInfoMsg->m_ModeSpecial&2)
			{
				Graphics()->BlendNormal();
				Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
				Graphics()->QuadsBegin();

				if(pInfoMsg->m_Player2ID == pInfoMsg->m_FlagCarrierBlue)
					RenderTools()->SelectSprite(SPRITE_FLAG_BLUE, SPRITE_FLAG_FLIP_X);
				else
					RenderTools()->SelectSprite(SPRITE_FLAG_RED, SPRITE_FLAG_FLIP_X);

				float Size = 56.0f;
				IGraphics::CQuadItem QuadItem(x-56, y-16, Size/2, Size);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
				Graphics()->QuadsEnd();
			}
		}

		x -= 24.0f;
		RenderTools()->RenderTee(CAnimState::GetIdle(), &pInfoMsg->m_Player2RenderInfo, EMOTE_ANGRY, vec2(1,0), vec2(x, y+28));
		x -= 32.0f;

		if(pInfoMsg->m_Player2ID >= 0)
		{
			x -= KillerNameW;
			float AdvanceID = UI()->DrawClientID(pInfoMsg->m_Player2NameCursor.m_FontSize, vec2(x, y), pInfoMsg->m_Player2ID);
			pInfoMsg->m_Player2NameCursor.MoveTo(x + AdvanceID, y);
			TextRender()->DrawTextOutlined(&pInfoMsg->m_Player2NameCursor);
		}
	}
}

void CInfoMessages::RenderFinishMsg(CInfoMsg *pInfoMsg, float x, float y) const
{
	float FontSize = 36.0f;
	float PlayerNameW = pInfoMsg->m_Player1NameCursor.Width() + UI()->GetClientIDRectWidth(FontSize);
	
	if(pInfoMsg->m_Diff != 0)
	{
		float DiffW = pInfoMsg->m_DiffCursor.Width();

		x -= DiffW;

		pInfoMsg->m_DiffCursor.MoveTo(x, y);
		TextRender()->DrawTextOutlined(&pInfoMsg->m_DiffCursor);

		x -= 16.0f;
	}

	float TimeW = pInfoMsg->m_TimeCursor.Width();
	x -= TimeW;
	pInfoMsg->m_TimeCursor.MoveTo(x, y);
	TextRender()->DrawTextOutlined(&pInfoMsg->m_TimeCursor);
	x -= 52.0f + 10.0f;

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_RACEFLAG].m_Id);
	Graphics()->QuadsBegin();
	IGraphics::CQuadItem QuadItem(x, y, 52, 52);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	x -= 10.0f;

	x -= PlayerNameW;

	float AdvanceID = UI()->DrawClientID(pInfoMsg->m_Player1NameCursor.m_FontSize, vec2(x, y), pInfoMsg->m_Player1ID);
	pInfoMsg->m_Player1NameCursor.MoveTo(x + AdvanceID, y);
	TextRender()->DrawTextOutlined(&pInfoMsg->m_Player1NameCursor);

	x -= 28.0f;

	int Emote = (pInfoMsg->m_RecordPersonal || pInfoMsg->m_RecordServer) ? EMOTE_HAPPY : EMOTE_NORMAL;
	RenderTools()->RenderTee(CAnimState::GetIdle(), &pInfoMsg->m_Player1RenderInfo, Emote, vec2(-1,0), vec2(x, y+28));
}
