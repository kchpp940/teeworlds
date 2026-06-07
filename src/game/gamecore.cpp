/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "gamecore.h"

const char *CTuningParams::s_apNames[] =
{
	#define MACRO_TUNING_PARAM(Name,ScriptName,Value) #ScriptName,
	#include "tuning.h"
	#undef MACRO_TUNING_PARAM
};


bool CTuningParams::Set(int Index, float Value)
{
	if(Index < 0 || Index >= Num())
		return false;
	((CTuneParam *)this)[Index] = Value;
	return true;
}

bool CTuningParams::Get(int Index, float *pValue) const
{
	if(Index < 0 || Index >= Num())
		return false;
	*pValue = (float)((CTuneParam *)this)[Index];
	return true;
}

bool CTuningParams::Set(const char *pName, float Value)
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, GetName(i)) == 0)
			return Set(i, Value);
	return false;
}

bool CTuningParams::Get(const char *pName, float *pValue) const
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, GetName(i)) == 0)
			return Get(i, pValue);
	return false;
}

int CTuningParams::PossibleTunings(const char *pStr, IConsole::FPossibleCallback pfnCallback, void *pUser)
{
	int Index = 0;
	for(int i = 0; i < Num(); i++)
	{
		if(str_find_nocase(GetName(i), pStr))
		{
			pfnCallback(Index, GetName(i), pUser);
			Index++;
		}
	}
	return Index;
}


float VelocityRamp(float Value, float Start, float Range, float Curvature)
{
	if(Value < Start)
		return 1.0f;
	return 1.0f/powf(Curvature, (Value-Start)/Range);
}

const float CCharacterCore::PHYS_SIZE = 28.0f;

bool CMovementUpdate::IsGrounded(const CMovementState *pState) const
{
	return m_pCollision && (
		m_pCollision->CheckPoint(pState->m_Pos.x+CCharacterCore::PHYS_SIZE/2, pState->m_Pos.y+CCharacterCore::PHYS_SIZE/2+5)
		|| m_pCollision->CheckPoint(pState->m_Pos.x-CCharacterCore::PHYS_SIZE/2, pState->m_Pos.y+CCharacterCore::PHYS_SIZE/2+5));
}

void CMovementUpdate::ApplyInput(CMovementState *pState, const CMovementInput *pInput) const
{
	pState->m_Direction = pInput->m_Direction;
	pState->m_Angle = (int)(angle(vec2(pInput->m_TargetX, pInput->m_TargetY))*256.0f);
}

void CMovementUpdate::TickJump(CMovementState *pState, const CMovementInput *pInput, bool UseInput, bool Grounded) const
{
	if(Grounded)
		pState->m_Jumped &= ~2;

	if(!UseInput)
		return;

	if(pInput->m_Jump)
	{
		if(!(pState->m_Jumped&1))
		{
			if(Grounded)
			{
				pState->m_TriggeredEvents |= COREEVENTFLAG_GROUND_JUMP;
				pState->m_Vel.y = -m_pTuning->m_GroundJumpImpulse;
				pState->m_Jumped |= 1;
			}
			else if(!(pState->m_Jumped&2))
			{
				pState->m_TriggeredEvents |= COREEVENTFLAG_AIR_JUMP;
				pState->m_Vel.y = -m_pTuning->m_AirJumpImpulse;
				pState->m_Jumped |= 3;
			}
		}
	}
	else
		pState->m_Jumped &= ~1;
}

void CMovementUpdate::TickVelocity(CMovementState *pState, bool Grounded) const
{
	float MaxSpeed = Grounded ? m_pTuning->m_GroundControlSpeed : m_pTuning->m_AirControlSpeed;
	float Accel = Grounded ? m_pTuning->m_GroundControlAccel : m_pTuning->m_AirControlAccel;
	float Friction = Grounded ? m_pTuning->m_GroundFriction : m_pTuning->m_AirFriction;

	if(pState->m_Direction < 0)
		pState->m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, pState->m_Vel.x, -Accel);
	if(pState->m_Direction > 0)
		pState->m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, pState->m_Vel.x, Accel);
	if(pState->m_Direction == 0)
		pState->m_Vel.x *= Friction;
}

void CMovementUpdate::TickHook(CMovementState *pState, const CMovementInput *pInput, bool UseInput, vec2 TargetDirection) const
{
	if(UseInput)
	{
		if(pInput->m_Hook)
		{
			if(pState->m_HookState == HOOK_IDLE)
			{
				pState->m_HookState = HOOK_FLYING;
				pState->m_HookPos = pState->m_Pos+TargetDirection*CCharacterCore::PHYS_SIZE*1.5f;
				pState->m_HookDir = TargetDirection;
				pState->m_HookedPlayer = -1;
				pState->m_HookTick = 0;
			}
		}
		else
		{
			pState->m_HookedPlayer = -1;
			pState->m_HookState = HOOK_IDLE;
			pState->m_HookPos = pState->m_Pos;
		}
	}

	if(pState->m_HookState == HOOK_IDLE)
	{
		pState->m_HookedPlayer = -1;
		pState->m_HookState = HOOK_IDLE;
		pState->m_HookPos = pState->m_Pos;
	}
	else if(pState->m_HookState >= HOOK_RETRACT_START && pState->m_HookState < HOOK_RETRACT_END)
	{
		pState->m_HookState++;
	}
	else if(pState->m_HookState == HOOK_RETRACT_END)
	{
		pState->m_HookState = HOOK_RETRACTED;
	}
	else if(pState->m_HookState == HOOK_FLYING)
	{
		vec2 NewPos = pState->m_HookPos+pState->m_HookDir*m_pTuning->m_HookFireSpeed;
		if(distance(pState->m_Pos, NewPos) > m_pTuning->m_HookLength)
		{
			pState->m_HookState = HOOK_RETRACT_START;
			NewPos = pState->m_Pos + normalize(NewPos-pState->m_Pos) * m_pTuning->m_HookLength;
		}

		bool GoingToHitGround = false;
		bool GoingToRetract = false;
		int Hit = m_pCollision->IntersectLine(pState->m_HookPos, NewPos, &NewPos, 0);
		if(Hit)
		{
			if(Hit&CCollision::COLFLAG_NOHOOK)
				GoingToRetract = true;
			else
				GoingToHitGround = true;
		}

		if(m_pWorld && m_pTuning->m_PlayerHooking)
		{
			float Distance = 0.0f;
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];
				if(!pCharCore || &pCharCore->m_State == pState)
					continue;

				vec2 ClosestPoint = closest_point_on_line(pState->m_HookPos, NewPos, pCharCore->m_Pos);
				if(distance(pCharCore->m_Pos, ClosestPoint) < CCharacterCore::PHYS_SIZE+2.0f)
				{
					if(pState->m_HookedPlayer == -1 || distance(pState->m_HookPos, pCharCore->m_Pos) < Distance)
					{
						pState->m_TriggeredEvents |= COREEVENTFLAG_HOOK_ATTACH_PLAYER;
						pState->m_HookState = HOOK_GRABBED;
						pState->m_HookedPlayer = i;
						Distance = distance(pState->m_HookPos, pCharCore->m_Pos);
					}
				}
			}
		}

		if(pState->m_HookState == HOOK_FLYING)
		{
			if(GoingToHitGround)
			{
				pState->m_TriggeredEvents |= COREEVENTFLAG_HOOK_ATTACH_GROUND;
				pState->m_HookState = HOOK_GRABBED;
			}
			else if(GoingToRetract)
			{
				pState->m_TriggeredEvents |= COREEVENTFLAG_HOOK_HIT_NOHOOK;
				pState->m_HookState = HOOK_RETRACT_START;
			}

			pState->m_HookPos = NewPos;
		}
	}

	if(pState->m_HookState == HOOK_GRABBED)
	{
		if(pState->m_HookedPlayer != -1)
		{
			CCharacterCore *pCharCore = m_pWorld ? m_pWorld->m_apCharacters[pState->m_HookedPlayer] : 0;
			if(pCharCore)
				pState->m_HookPos = pCharCore->m_Pos;
			else
			{
				pState->m_HookedPlayer = -1;
				pState->m_HookState = HOOK_RETRACTED;
				pState->m_HookPos = pState->m_Pos;
			}
		}

		if(pState->m_HookedPlayer == -1 && distance(pState->m_HookPos, pState->m_Pos) > 46.0f)
		{
			vec2 HookVel = normalize(pState->m_HookPos-pState->m_Pos)*m_pTuning->m_HookDragAccel;
			if(HookVel.y > 0)
				HookVel.y *= 0.3f;

			if((HookVel.x < 0 && pState->m_Direction < 0) || (HookVel.x > 0 && pState->m_Direction > 0))
				HookVel.x *= 0.95f;
			else
				HookVel.x *= 0.75f;

			vec2 NewVel = pState->m_Vel+HookVel;

			if(length(NewVel) < m_pTuning->m_HookDragSpeed || length(NewVel) < length(pState->m_Vel))
				pState->m_Vel = NewVel;
		}

		pState->m_HookTick++;
		if(pState->m_HookedPlayer != -1 && (pState->m_HookTick > SERVER_TICK_SPEED+SERVER_TICK_SPEED/5 || (m_pWorld && !m_pWorld->m_apCharacters[pState->m_HookedPlayer])))
		{
			pState->m_HookedPlayer = -1;
			pState->m_HookState = HOOK_RETRACTED;
			pState->m_HookPos = pState->m_Pos;
		}
	}
}

void CMovementUpdate::TickPlayerCollisions(CMovementState *pState) const
{
	if(!m_pWorld)
		return;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];
		if(!pCharCore || &pCharCore->m_State == pState)
			continue;

		float Distance = distance(pState->m_Pos, pCharCore->m_Pos);
		vec2 Dir = normalize(pState->m_Pos - pCharCore->m_Pos);
		if(m_pTuning->m_PlayerCollision && Distance < CCharacterCore::PHYS_SIZE*1.25f && Distance > 0.0f)
		{
			float a = (CCharacterCore::PHYS_SIZE*1.45f - Distance);
			float Velocity = 0.5f;

			if(length(pState->m_Vel) > 0.0001)
				Velocity = 1-(dot(normalize(pState->m_Vel), Dir)+1)/2;

			pState->m_Vel += Dir*a*(Velocity*0.75f);
			pState->m_Vel *= 0.85f;
		}

		if(pState->m_HookedPlayer == i && m_pTuning->m_PlayerHooking)
		{
			if(Distance > CCharacterCore::PHYS_SIZE*1.50f)
			{
				float Accel = m_pTuning->m_HookDragAccel * (Distance/m_pTuning->m_HookLength);
				pCharCore->m_HookDragVel += Dir*Accel*1.5f;
				pState->m_HookDragVel -= Dir*Accel*0.25f;
			}
		}
	}
}

void CMovementUpdate::Tick(CMovementState *pState, const CMovementInput *pInput, bool UseInput) const
{
	pState->m_TriggeredEvents = 0;

	const bool Grounded = IsGrounded(pState);
	vec2 TargetDirection = normalize(vec2(pInput->m_TargetX, pInput->m_TargetY));

	pState->m_Vel.y += m_pTuning->m_Gravity;

	if(UseInput)
		ApplyInput(pState, pInput);

	TickJump(pState, pInput, UseInput, Grounded);
	TickHook(pState, pInput, UseInput, TargetDirection);
	TickVelocity(pState, Grounded);
	TickPlayerCollisions(pState);

	if(length(pState->m_Vel) > 6000)
		pState->m_Vel = normalize(pState->m_Vel) * 6000;
}

void CMovementUpdate::Move(CMovementState *pState) const
{
	if(!m_pWorld)
		return;

	float RampValue = VelocityRamp(length(pState->m_Vel)*50, m_pTuning->m_VelrampStart, m_pTuning->m_VelrampRange, m_pTuning->m_VelrampCurvature);

	pState->m_Vel.x = pState->m_Vel.x*RampValue;

	vec2 OldPos = pState->m_Pos;
	vec2 NewPos = pState->m_Pos;
	m_pCollision->MoveBox(&NewPos, &pState->m_Vel, vec2(CCharacterCore::PHYS_SIZE, CCharacterCore::PHYS_SIZE), 0, &pState->m_Death);

	pState->m_Vel.x = pState->m_Vel.x*(1.0f/RampValue);

	if(m_pTuning->m_PlayerCollision)
	{
		float Distance = distance(OldPos, NewPos);
		int End = (int)Distance+1;
		vec2 LastPos = OldPos;
		for(int i = 0; i < End; i++)
		{
			float a = Distance > 0.0001f ? i/Distance : 0.0f;
			vec2 Pos = mix(OldPos, NewPos, a);
			for(int p = 0; p < MAX_CLIENTS; p++)
			{
				CCharacterCore *pCharCore = m_pWorld->m_apCharacters[p];
				if(!pCharCore || &pCharCore->m_State == pState)
					continue;
				float D = distance(Pos, pCharCore->m_Pos);
				if(D < CCharacterCore::PHYS_SIZE && D >= 0.0f)
				{
					if(a > 0.0f)
					{
						pState->m_Pos = LastPos;
						return;
					}
					else if(distance(NewPos, pCharCore->m_Pos) > D)
					{
						pState->m_Pos = NewPos;
						return;
					}
				}
			}
			LastPos = Pos;
		}
	}

	pState->m_Pos = NewPos;
}

void CMovementUpdate::AddDragVelocity(CMovementState *pState) const
{
	float DragSpeed = m_pTuning->m_HookDragSpeed;
	pState->m_Vel.x = SaturatedAdd(-DragSpeed, DragSpeed, pState->m_Vel.x, pState->m_HookDragVel.x);
	pState->m_Vel.y = SaturatedAdd(-DragSpeed, DragSpeed, pState->m_Vel.y, pState->m_HookDragVel.y);
}

void CMovementUpdate::ResetDragVelocity(CMovementState *pState) const
{
	pState->m_HookDragVel = vec2(0, 0);
}

void CMovementUpdate::ReadState(CMovementState *pState, const CNetObj_CharacterCore *pObjCore) const
{
	pState->m_Pos.x = pObjCore->m_X;
	pState->m_Pos.y = pObjCore->m_Y;
	pState->m_Vel.x = pObjCore->m_VelX/256.0f;
	pState->m_Vel.y = pObjCore->m_VelY/256.0f;
	pState->m_HookState = pObjCore->m_HookState;
	pState->m_HookTick = pObjCore->m_HookTick;
	pState->m_HookPos.x = pObjCore->m_HookX;
	pState->m_HookPos.y = pObjCore->m_HookY;
	pState->m_HookDir.x = pObjCore->m_HookDx/256.0f;
	pState->m_HookDir.y = pObjCore->m_HookDy/256.0f;
	pState->m_HookedPlayer = pObjCore->m_HookedPlayer;
	pState->m_Jumped = pObjCore->m_Jumped;
	pState->m_Direction = pObjCore->m_Direction;
	pState->m_Angle = pObjCore->m_Angle;
}

void CMovementUpdate::WriteState(const CMovementState *pState, CNetObj_CharacterCore *pObjCore) const
{
	pObjCore->m_X = round_to_int(pState->m_Pos.x);
	pObjCore->m_Y = round_to_int(pState->m_Pos.y);
	pObjCore->m_VelX = round_to_int(pState->m_Vel.x*256.0f);
	pObjCore->m_VelY = round_to_int(pState->m_Vel.y*256.0f);
	pObjCore->m_HookState = pState->m_HookState;
	pObjCore->m_HookTick = pState->m_HookTick;
	pObjCore->m_HookX = round_to_int(pState->m_HookPos.x);
	pObjCore->m_HookY = round_to_int(pState->m_HookPos.y);
	pObjCore->m_HookDx = round_to_int(pState->m_HookDir.x*256.0f);
	pObjCore->m_HookDy = round_to_int(pState->m_HookDir.y*256.0f);
	pObjCore->m_HookedPlayer = pState->m_HookedPlayer;
	pObjCore->m_Jumped = pState->m_Jumped;
	pObjCore->m_Direction = pState->m_Direction;
	pObjCore->m_Angle = pState->m_Angle;
}

void CMovementUpdate::Quantize(CMovementState *pState) const
{
	CNetObj_CharacterCore Core;
	WriteState(pState, &Core);
	ReadState(pState, &Core);
}

void CMovementUpdate::AdvanceSnapshot(CNetObj_Character *pCharacter, int TargetTick) const
{
	CWorldCore TempWorld;
	TempWorld.m_Tuning = *m_pTuning;
	CMovementUpdate TempUpdate(&TempWorld, m_pCollision, m_pTuning);
	CMovementState TempState;
	TempState.Reset();
	TempUpdate.ReadState(&TempState, pCharacter);

	CMovementInput DummyInput;
	DummyInput.Reset();

	while(pCharacter->m_Tick < TargetTick)
	{
		pCharacter->m_Tick++;
		TempUpdate.Tick(&TempState, &DummyInput, false);
		TempUpdate.Move(&TempState);
		TempUpdate.Quantize(&TempState);
	}

	TempUpdate.WriteState(&TempState, pCharacter);
}

void CCharacterCore::Init(CWorldCore *pWorld, CCollision *pCollision)
{
	m_pWorld = pWorld;
	m_pCollision = pCollision;
	m_Update.Init(pWorld, pCollision, pWorld ? &pWorld->m_Tuning : 0);
}

void CCharacterCore::Reset()
{
	m_State.Reset();
	m_InputState.Reset();
	mem_zero(&m_Input, sizeof(m_Input));
}

void CCharacterCore::Tick(bool UseInput)
{
	if(UseInput)
		m_InputState.FromPlayerInput(&m_Input);
	m_Update.Tick(&m_State, &m_InputState, UseInput);
}

void CCharacterCore::Move()
{
	m_Update.Move(&m_State);
}

void CCharacterCore::AddDragVelocity()
{
	m_Update.AddDragVelocity(&m_State);
}

void CCharacterCore::ResetDragVelocity()
{
	m_Update.ResetDragVelocity(&m_State);
}

void CCharacterCore::Write(CNetObj_CharacterCore *pObjCore) const
{
	m_Update.WriteState(&m_State, pObjCore);
}

void CCharacterCore::Read(const CNetObj_CharacterCore *pObjCore)
{
	m_Update.ReadState(&m_State, pObjCore);
}

void CCharacterCore::Quantize()
{
	m_Update.Quantize(&m_State);
}

CMovementUpdate::CInputCount CMovementUpdate::CountInputState(int Prev, int Cur) const
{
	CInputCount c = {0, 0};
	Prev &= INPUT_STATE_MASK;
	Cur &= INPUT_STATE_MASK;
	int i = Prev;

	while(i != Cur)
	{
		i = (i+1)&INPUT_STATE_MASK;
		if(i&1)
			c.m_Presses++;
		else
			c.m_Releases++;
	}

	return c;
}

int CMovementUpdate::ComputeRequestedWeapon(CMovementInput *pInput, int CurrentWeapon, const bool *pWeaponsGot) const
{
	int WantedWeapon = CurrentWeapon;

	int Next = CountInputState(pInput->m_PrevInputNextWeapon, pInput->m_NextWeapon).m_Presses;
	int Prev = CountInputState(pInput->m_PrevInputPrevWeapon, pInput->m_PrevWeapon).m_Presses;

	if(Next < 128)
	{
		while(Next)
		{
			WantedWeapon = (WantedWeapon+1)%NUM_WEAPONS;
			if(pWeaponsGot[WantedWeapon])
				Next--;
		}
	}

	if(Prev < 128)
	{
		while(Prev)
		{
			WantedWeapon = (WantedWeapon-1)<0?NUM_WEAPONS-1:WantedWeapon-1;
			if(pWeaponsGot[WantedWeapon])
				Prev--;
		}
	}

	if(pInput->m_WantedWeapon)
		WantedWeapon = pInput->m_WantedWeapon-1;

	pInput->m_PrevInputNextWeapon = pInput->m_NextWeapon;
	pInput->m_PrevInputPrevWeapon = pInput->m_PrevWeapon;

	if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != CurrentWeapon && pWeaponsGot[WantedWeapon])
		return WantedWeapon;
	return -1;
}

void CMovementUpdate::AdvanceTick(CMovementState *pState, const CMovementInput *pInput, bool UseInput) const
{
	Tick(pState, pInput, UseInput);
	AddDragVelocity(pState);
	ResetDragVelocity(pState);
	Move(pState);
	Quantize(pState);
}

void CMovementUpdate::ApplySnapshot(CMovementState *pState, const CNetObj_CharacterCore *pObjCore) const
{
	ReadState(pState, pObjCore);
}

void CMovementUpdate::WriteSnapshot(const CMovementState *pState, CNetObj_CharacterCore *pObjCore) const
{
	WriteState(pState, pObjCore);
}

int CCharacterCore::ComputeRequestedWeapon(int CurrentWeapon, const bool *pWeaponsGot)
{
	m_InputState.FromPlayerInput(&m_Input);
	return m_Update.ComputeRequestedWeapon(&m_InputState, CurrentWeapon, pWeaponsGot);
}

void CCharacterCore::AdvanceTick(bool UseInput)
{
	if(UseInput)
		m_InputState.FromPlayerInput(&m_Input);
	m_Update.AdvanceTick(&m_State, &m_InputState, UseInput);
}

void CCharacterCore::ApplySnapshot(const CNetObj_CharacterCore *pObjCore)
{
	m_Update.ApplySnapshot(&m_State, pObjCore);
}

void CCharacterCore::WriteSnapshot(CNetObj_CharacterCore *pObjCore) const
{
	m_Update.WriteSnapshot(&m_State, pObjCore);
}
