/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include "lms.h"

CGameControllerLMS::CGameControllerLMS(CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "LMS";
	m_GameFlags = GAMEFLAG_SURVIVAL;
}

// event
void CGameControllerLMS::OnCharacterSpawn(CCharacter *pChr)
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
void CGameControllerLMS::DoWincheckRound()
{
	ApplyRoundResult(BuildRoundTimeLimitResult());
	ApplyRoundResult(BuildSurvivalSoloResult());
}
