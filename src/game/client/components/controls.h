/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CONTROLS_H
#define GAME_CLIENT_COMPONENTS_CONTROLS_H
#include <base/vmath.h>
#include <game/client/component.h>
#include <game/gamecore.h>

class CControls : public CComponent
{
public:
	vec2 m_MousePos;
	vec2 m_TargetPos;

	CMovementInput m_InputData;
	CMovementInput m_LastData;
	int m_InputDirectionLeft;
	int m_InputDirectionRight;

	CControls();

	virtual void OnReset();
	virtual void OnRelease();
	virtual void OnRender();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual bool OnCursorMove(float x, float y, int CursorType);
	virtual void OnConsoleInit();
	virtual void OnPlayerDeath();

	int SnapInput(int *pData);
	void ClampMousePos();
	float GetMaxMouseDistance() const;
};
#endif
