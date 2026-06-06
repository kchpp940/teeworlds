/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include "lts.h"

CGameControllerLTS::CGameControllerLTS(CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "LTS";
	m_GameFlags = GAMEFLAG_TEAMS|GAMEFLAG_SURVIVAL;
}

// event
void CGameControllerLTS::OnCharacterSpawn(class CCharacter *pChr)
{
	IGameController::OnCharacterSpawn(pChr);

	// give start equipment
	pChr->IncreaseArmor(5);
	pChr->GiveWeapon(WEAPON_SHOTGUN, 10);
	pChr->GiveWeapon(WEAPON_GRENADE, 10);
	pChr->GiveWeapon(WEAPON_LASER, 5);

	// prevent respawn
	pChr->GetPlayer()->m_RespawnDisabled = GetStartRespawnState();
}

// game
void CGameControllerLTS::DoWincheckRound()
{
	CGameResult Result = BuildRoundTimeLimitResult();
	if(Result.m_Result != WIN_RESULT_NONE)
	{
		ApplyRoundResult(Result);
		return;
	}

	Result = BuildSurvivalTeamResult();
	ApplyRoundResult(Result);
}
