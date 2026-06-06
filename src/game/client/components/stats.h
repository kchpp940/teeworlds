/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_STATS_H
#define GAME_CLIENT_COMPONENTS_STATS_H

#include <game/client/component.h>
#include <game/client/components/match_events.h>

enum
{
	TC_STATS_KILLS = 1,
	TC_STATS_DEATHS = 2,
	TC_STATS_SUICIDES = 4,
	TC_STATS_RATIO = 8,
	TC_STATS_NET = 16,
	TC_STATS_KPM = 32,
	TC_STATS_SPREE = 64,
	TC_STATS_BESTSPREE = 128,
	TC_STATS_FLAGGRABS = 256,
	TC_STATS_WEAPS = 512,
	TC_STATS_FLAGCAPTURES = 1024,
	NUM_TC_STATS = 11
};

class CStats: public CComponent
{
private:
	typedef class CMatchEvents::CPlayerMatchStats CPlayerStats;

	const CPlayerStats *PlayerStats(int ClientID) const;

	bool m_Active;
	bool m_Activate;

	bool m_ScreenshotTaken;
	int64 m_ScreenshotTime;
	static void ConKeyStats(IConsole::IResult *pResult, void *pUserData);
	void AutoStatScreenshot();

public:
	CStats();
	bool IsActive() const;
	virtual void OnReset();
	void OnStartGame();
	virtual void OnConsoleInit();
	virtual void OnRender();
	virtual void OnRelease();

	const CPlayerStats *GetPlayerStats(int ClientID) const;
};

#endif
