/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_GAMECORE_H
#define GAME_GAMECORE_H

#include <base/system.h>
#include <base/math.h>

#include <math.h>
#include "collision.h"
#include <engine/console.h>
#include <engine/shared/protocol.h>
#include <generated/protocol.h>

class CTuneParam
{
	int m_Value;
public:
	void Set(int v) { m_Value = v; }
	int Get() const { return m_Value; }
	CTuneParam &operator = (int v) { m_Value = (int)(v*100.0f); return *this; }
	CTuneParam &operator = (float v) { m_Value = (int)(v*100.0f); return *this; }
	operator float() const { return m_Value/100.0f; }
};

class CTuningParams
{
	static const char *s_apNames[];
public:
	CTuningParams()
	{
		const float TicksPerSecond = 50.0f;
		#define MACRO_TUNING_PARAM(Name,ScriptName,Value) m_##Name.Set((int)(Value*100.0f));
		#include "tuning.h"
		#undef MACRO_TUNING_PARAM
	}

	#define MACRO_TUNING_PARAM(Name,ScriptName,Value) CTuneParam m_##Name;
	#include "tuning.h"
	#undef MACRO_TUNING_PARAM

	static int Num() { return sizeof(CTuningParams)/sizeof(CTuneParam); }
	bool Set(int Index, float Value);
	bool Set(const char *pName, float Value);
	bool Get(int Index, float *pValue) const;
	bool Get(const char *pName, float *pValue) const;
	const char *GetName(int Index) const { return s_apNames[Index]; }
	int PossibleTunings(const char *pStr, IConsole::FPossibleCallback pfnCallback = IConsole::EmptyPossibleCommandCallback, void *pUser = 0);
};

inline void StrToInts(int *pInts, int Num, const char *pStr)
{
	int Index = 0;
	while(Num)
	{
		char aBuf[4] = {0,0,0,0};
		for(int c = 0; c < 4 && pStr[Index]; c++, Index++)
			aBuf[c] = pStr[Index];
		*pInts = ((aBuf[0]+128)<<24)|((aBuf[1]+128)<<16)|((aBuf[2]+128)<<8)|(aBuf[3]+128);
		pInts++;
		Num--;
	}

	// null terminate
	pInts[-1] &= 0xffffff00;
}

inline void IntsToStr(const int *pInts, int Num, char *pStr)
{
	while(Num)
	{
		pStr[0] = (((*pInts)>>24)&0xff)-128;
		pStr[1] = (((*pInts)>>16)&0xff)-128;
		pStr[2] = (((*pInts)>>8)&0xff)-128;
		pStr[3] = ((*pInts)&0xff)-128;
		pStr += 4;
		pInts++;
		Num--;
	}

	// null terminate
	pStr[-1] = 0;
}



inline vec2 CalcPos(vec2 Pos, vec2 Velocity, float Curvature, float Speed, float Time)
{
	vec2 n;
	Time *= Speed;
	n.x = Pos.x + Velocity.x*Time;
	n.y = Pos.y + Velocity.y*Time + Curvature/10000*(Time*Time);
	return n;
}


template<typename T>
inline T SaturatedAdd(T Min, T Max, T Current, T Modifier)
{
	if(Modifier < 0)
	{
		if(Current < Min)
			return Current;
		Current += Modifier;
		if(Current < Min)
			Current = Min;
		return Current;
	}
	else
	{
		if(Current > Max)
			return Current;
		Current += Modifier;
		if(Current > Max)
			Current = Max;
		return Current;
	}
}


float VelocityRamp(float Value, float Start, float Range, float Curvature);

// hooking stuff
enum
{
	HOOK_RETRACTED=-1,
	HOOK_IDLE=0,
	HOOK_RETRACT_START=1,
	HOOK_RETRACT_END=3,
	HOOK_FLYING,
	HOOK_GRABBED,
};

enum
{
	MOVEMENTFLAG_ALIVE=1,
	MOVEMENTFLAG_FROZEN=2,
};

class CMovementInput
{
public:
	int m_Direction;
	int m_TargetX;
	int m_TargetY;
	int m_Jump;
	int m_Fire;
	int m_Hook;
	int m_PlayerFlags;
	int m_WantedWeapon;
	int m_NextWeapon;
	int m_PrevWeapon;
	int m_PrevInputNextWeapon;
	int m_PrevInputPrevWeapon;

	void Reset()
	{
		m_Direction = 0;
		m_TargetX = 0;
		m_TargetY = -1;
		m_Jump = 0;
		m_Fire = 0;
		m_Hook = 0;
		m_PlayerFlags = 0;
		m_WantedWeapon = 0;
		m_NextWeapon = 0;
		m_PrevWeapon = 0;
		m_PrevInputNextWeapon = 0;
		m_PrevInputPrevWeapon = 0;
	}

	void FromPlayerInput(const CNetObj_PlayerInput *pInput)
	{
		m_Direction = pInput->m_Direction;
		m_TargetX = pInput->m_TargetX;
		m_TargetY = pInput->m_TargetY;
		m_Jump = pInput->m_Jump;
		m_Fire = pInput->m_Fire;
		m_Hook = pInput->m_Hook;
		m_PlayerFlags = pInput->m_PlayerFlags;
		m_WantedWeapon = pInput->m_WantedWeapon;
		m_NextWeapon = pInput->m_NextWeapon;
		m_PrevWeapon = pInput->m_PrevWeapon;
		if(m_TargetX == 0 && m_TargetY == 0)
			m_TargetY = -1;
	}

	void UpdateFromPlayerInput(const CNetObj_PlayerInput *pInput)
	{
		m_Direction = pInput->m_Direction;
		m_TargetX = pInput->m_TargetX;
		m_TargetY = pInput->m_TargetY;
		m_Jump = pInput->m_Jump;
		m_Fire = pInput->m_Fire;
		m_Hook = pInput->m_Hook;
		m_PlayerFlags = pInput->m_PlayerFlags;
		m_WantedWeapon = pInput->m_WantedWeapon;
		m_NextWeapon = pInput->m_NextWeapon;
		m_PrevWeapon = pInput->m_PrevWeapon;
		if(m_TargetX == 0 && m_TargetY == 0)
			m_TargetY = -1;
	}

	void ToPlayerInput(CNetObj_PlayerInput *pInput) const
	{
		pInput->m_Direction = m_Direction;
		pInput->m_TargetX = m_TargetX;
		pInput->m_TargetY = m_TargetY;
		pInput->m_Jump = m_Jump;
		pInput->m_Fire = m_Fire;
		pInput->m_Hook = m_Hook;
		pInput->m_PlayerFlags = m_PlayerFlags;
		pInput->m_WantedWeapon = m_WantedWeapon;
		pInput->m_NextWeapon = m_NextWeapon;
		pInput->m_PrevWeapon = m_PrevWeapon;
	}
};

class CMovementState
{
public:
	vec2 m_Pos;
	vec2 m_Vel;
	vec2 m_HookDragVel;
	vec2 m_HookPos;
	vec2 m_HookDir;
	int m_HookTick;
	int m_HookState;
	int m_HookedPlayer;
	int m_Jumped;
	int m_Direction;
	int m_Angle;
	bool m_Death;
	int m_TriggeredEvents;

	void Reset()
	{
		m_Pos = vec2(0, 0);
		m_Vel = vec2(0, 0);
		m_HookDragVel = vec2(0, 0);
		m_HookPos = vec2(0, 0);
		m_HookDir = vec2(0, 0);
		m_HookTick = 0;
		m_HookState = HOOK_IDLE;
		m_HookedPlayer = -1;
		m_Jumped = 0;
		m_Direction = 0;
		m_Angle = 0;
		m_Death = false;
		m_TriggeredEvents = 0;
	}
};

class CMovementUpdate
{
public:
	class CWorldCore *m_pWorld;
	class CCollision *m_pCollision;
	const CTuningParams *m_pTuning;

	CMovementUpdate() : m_pWorld(0), m_pCollision(0), m_pTuning(0) {}
	CMovementUpdate(class CWorldCore *pWorld, class CCollision *pCollision, const CTuningParams *pTuning)
		: m_pWorld(pWorld), m_pCollision(pCollision), m_pTuning(pTuning) {}

	void Init(class CWorldCore *pWorld, class CCollision *pCollision, const CTuningParams *pTuning)
	{
		m_pWorld = pWorld;
		m_pCollision = pCollision;
		m_pTuning = pTuning;
	}

	bool IsGrounded(const CMovementState *pState) const;
	void ApplyInput(CMovementState *pState, const CMovementInput *pInput) const;
	void Tick(CMovementState *pState, const CMovementInput *pInput, bool UseInput) const;
	void Move(CMovementState *pState) const;
	void AddDragVelocity(CMovementState *pState) const;
	void ResetDragVelocity(CMovementState *pState) const;
	void Quantize(CMovementState *pState) const;

	void ReadState(CMovementState *pState, const CNetObj_CharacterCore *pObjCore) const;
	void WriteState(const CMovementState *pState, CNetObj_CharacterCore *pObjCore) const;

	void AdvanceSnapshot(CNetObj_Character *pCharacter, int TargetTick) const;

	int ComputeWeaponRequest(CMovementInput *pInput, int CurrentWeapon, const bool *pWeaponsGot) const;

	void AdvanceTickPhase1(CMovementState *pState, const CMovementInput *pInput, bool UseInput) const;
	void AdvanceTickPhase2(CMovementState *pState) const;
	void AdvanceTick(CMovementState *pState, const CMovementInput *pInput, bool UseInput) const;
	void ApplySnapshot(CMovementState *pState, const CNetObj_CharacterCore *pObjCore) const;
	void WriteSnapshot(const CMovementState *pState, CNetObj_CharacterCore *pObjCore) const;

private:
	struct CInputCount
	{
		int m_Presses;
		int m_Releases;
	};
	CInputCount CountInputState(int Prev, int Cur) const;
	void TickHook(CMovementState *pState, const CMovementInput *pInput, bool UseInput, vec2 TargetDirection) const;
	void TickJump(CMovementState *pState, const CMovementInput *pInput, bool UseInput, bool Grounded) const;
	void TickVelocity(CMovementState *pState, bool Grounded) const;
	void TickPlayerCollisions(CMovementState *pState) const;
};

class CWorldCore
{
public:
	CWorldCore()
	{
		mem_zero(m_apCharacters, sizeof(m_apCharacters));
	}

	CTuningParams m_Tuning;
	class CCharacterCore *m_apCharacters[MAX_CLIENTS];
};

class CCharacterCore
{
	CWorldCore *m_pWorld;
	CCollision *m_pCollision;
	CMovementUpdate m_Update;
public:
	static const float PHYS_SIZE;

	CMovementState m_State;
	CMovementInput m_InputState;

	vec2 &m_Pos;
	vec2 &m_Vel;
	vec2 &m_HookDragVel;
	vec2 &m_HookPos;
	vec2 &m_HookDir;
	int &m_HookTick;
	int &m_HookState;
	int &m_HookedPlayer;
	int &m_Jumped;
	int &m_Direction;
	int &m_Angle;
	bool &m_Death;
	CNetObj_PlayerInput m_Input;
	int &m_TriggeredEvents;

	CCharacterCore()
		: m_pWorld(0), m_pCollision(0),
		m_Pos(m_State.m_Pos), m_Vel(m_State.m_Vel),
		m_HookDragVel(m_State.m_HookDragVel),
		m_HookPos(m_State.m_HookPos), m_HookDir(m_State.m_HookDir),
		m_HookTick(m_State.m_HookTick), m_HookState(m_State.m_HookState),
		m_HookedPlayer(m_State.m_HookedPlayer), m_Jumped(m_State.m_Jumped),
		m_Direction(m_State.m_Direction), m_Angle(m_State.m_Angle),
		m_Death(m_State.m_Death), m_TriggeredEvents(m_State.m_TriggeredEvents)
	{
		mem_zero(&m_Input, sizeof(m_Input));
	}

	CCharacterCore(const CCharacterCore &Other)
		: m_pWorld(Other.m_pWorld), m_pCollision(Other.m_pCollision),
		m_State(Other.m_State), m_InputState(Other.m_InputState),
		m_Pos(m_State.m_Pos), m_Vel(m_State.m_Vel),
		m_HookDragVel(m_State.m_HookDragVel),
		m_HookPos(m_State.m_HookPos), m_HookDir(m_State.m_HookDir),
		m_HookTick(m_State.m_HookTick), m_HookState(m_State.m_HookState),
		m_HookedPlayer(m_State.m_HookedPlayer), m_Jumped(m_State.m_Jumped),
		m_Direction(m_State.m_Direction), m_Angle(m_State.m_Angle),
		m_Death(m_State.m_Death), m_TriggeredEvents(m_State.m_TriggeredEvents)
	{
		m_Update.Init(m_pWorld, m_pCollision, m_pWorld ? &m_pWorld->m_Tuning : 0);
		mem_copy(&m_Input, &Other.m_Input, sizeof(m_Input));
	}

	CCharacterCore &operator=(const CCharacterCore &Other)
	{
		if(this != &Other)
		{
			m_pWorld = Other.m_pWorld;
			m_pCollision = Other.m_pCollision;
			m_State = Other.m_State;
			m_InputState = Other.m_InputState;
			m_Update.Init(m_pWorld, m_pCollision, m_pWorld ? &m_pWorld->m_Tuning : 0);
			mem_copy(&m_Input, &Other.m_Input, sizeof(m_Input));
		}
		return *this;
	}

	void Init(CWorldCore *pWorld, CCollision *pCollision);
	void Reset();
	void Tick(bool UseInput);
	void Move();

	void AddDragVelocity();
	void ResetDragVelocity();

	void Read(const CNetObj_CharacterCore *pObjCore);
	void Write(CNetObj_CharacterCore *pObjCore) const;
	void Quantize();

	int ComputeWeaponRequest(int CurrentWeapon, const bool *pWeaponsGot);
	void AdvanceTickPhase1(bool UseInput);
	void AdvanceTickPhase2();
	void AdvanceTick(bool UseInput);
	void ApplySnapshot(const CNetObj_CharacterCore *pObjCore);
	void WriteSnapshot(CNetObj_CharacterCore *pObjCore) const;
};

#endif
