/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_INFOMESSAGES_H
#define GAME_CLIENT_COMPONENTS_INFOMESSAGES_H
#include <game/client/component.h>
#include <game/client/components/match_events.h>

class CInfoMessages : public CComponent
{
	struct CInfoMsg
	{
		int m_Type;
		int m_Tick;
	
		int m_Player1ID;
		CTextCursor m_Player1NameCursor;
		CTeeRenderInfo m_Player1RenderInfo;

		int m_Player2ID;
		CTextCursor m_Player2NameCursor;
		CTeeRenderInfo m_Player2RenderInfo;

		int m_Weapon;
		int m_ModeSpecial;
		int m_FlagCarrierBlue;

		int m_Time;
		int m_Diff;
		CTextCursor m_TimeCursor;
		CTextCursor m_DiffCursor;
		int m_RecordPersonal;
		int m_RecordServer;
	};

	enum
	{
		MAX_INFOMSGS = 5,

		INFOMSG_KILL = 0,
		INFOMSG_FINISH
	};

	CInfoMsg m_aInfoMsgs[MAX_INFOMSGS];
	int m_InfoMsgCurrent;

	int m_LastKillEventGeneration;
	int m_LastRaceEventGeneration;

	void AddInfoMsg(int Type, CInfoMsg NewMsg);

	void PrepareKillMsgFromEvent(const struct CMatchEvents::CKillEvent *pEvent);
	void PrepareFinishMsgFromEvent(const struct CMatchEvents::CRaceFinishEvent *pEvent);
	void ProcessNewEvents();

	void RenderKillMsg(CInfoMsg *pInfoMsg, float x, float y) const;
	void RenderFinishMsg(CInfoMsg *pInfoMsg, float x, float y) const;

public:
	virtual void OnReset();
	virtual void OnRender();
};

#endif
