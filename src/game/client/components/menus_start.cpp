/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/textrender.h>

#include <engine/shared/config.h>

#include <game/version.h>
#include <game/client/render.h>
#include <game/client/ui.h>

#include <generated/client_data.h>

#include "menus.h"

void CMenus::RenderStartMenu(CUIRect MainView)
{
	// render logo
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BANNER].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1,1,1,1);
	IGraphics::CQuadItem QuadItem(MainView.w/2-140, 60, 280, 70);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	const float Rounding = 10.0f;
	const float ButtonHeight = 40.0f;
	const float Spacing = 5.0f;

	CUIRect TopMenu, BottomMenu;
	MainView.VMargin(MainView.w/2-190.0f, &TopMenu);
	TopMenu.HSplitTop(365.0f, &TopMenu, &BottomMenu);
	RenderBackgroundShadow(&TopMenu, false, Rounding);

	TopMenu.HSplitTop(145.0f, 0, &TopMenu);

	CUIRect Button;
	const char *pImage;

	// Settings button
	pImage = Config()->m_ClShowStartMenuImages ? "settings" : 0;
	DoButtons_HSplitColumn(&TopMenu, &Button, ButtonHeight, Spacing);
	static CButtonContainer s_SettingsButton;
	CMenuAction SettingsAction = CMenuAction::Navigate(Localize("Settings"), PAGE_SETTINGS, KEY_S, 0, pImage, CUIRect::CORNER_ALL, Rounding, 0.5f);
	DoMenuActionButton(&s_SettingsButton, &SettingsAction, &Button);

	// Demos button
	pImage = Config()->m_ClShowStartMenuImages ? "demos" : 0;
	DoButtons_HSplitColumn(&TopMenu, &Button, ButtonHeight, Spacing);
	static CButtonContainer s_DemoButton;
	CMenuAction DemosAction = CMenuAction::Navigate(Localize("Demos"), PAGE_DEMOS, KEY_D, &CMenus::DemolistPrepare, pImage, CUIRect::CORNER_ALL, Rounding, 0.5f);
	DoMenuActionButton(&s_DemoButton, &DemosAction, &Button);

	// Editor button (has special hotkey logic)
	static bool EditorHotkeyWasPressed = true;
	static float EditorHotKeyChecktime = 0;
	pImage = Config()->m_ClShowStartMenuImages ? "editor" : 0;
	DoButtons_HSplitColumn(&TopMenu, &Button, ButtonHeight, Spacing);
	static CButtonContainer s_MapEditorButton;
	CMenuAction EditorAction = CMenuAction::Direct(Localize("Editor"), &CMenus::ActionEnterEditor, 0, 0, pImage, CUIRect::CORNER_ALL, Rounding, 0.5f);
	if(DoMenuActionButton(&s_MapEditorButton, &EditorAction, &Button) || (!EditorHotkeyWasPressed && Client()->LocalTime() - EditorHotKeyChecktime < 0.1f && CheckHotKey(KEY_E)))
		EditorHotkeyWasPressed = true;
	if(!Input()->KeyIsPressed(KEY_E))
	{
		EditorHotkeyWasPressed = false;
		EditorHotKeyChecktime = Client()->LocalTime();
	}

	// Play button
	pImage = Config()->m_ClShowStartMenuImages ? "play_game" : 0;
	DoButtons_HSplitColumn(&TopMenu, &Button, ButtonHeight, Spacing);
	static CButtonContainer s_PlayButton;
	CMenuAction PlayAction = CMenuAction::Direct(Localize("Play"), &CMenus::ActionNavigateBrowserPage, KEY_P, CUI::HOTKEY_ENTER, pImage, CUIRect::CORNER_ALL, Rounding, 0.5f);
	DoMenuActionButton(&s_PlayButton, &PlayAction, &Button);

	// Bottom menu with Quit button
	BottomMenu.HSplitTop(90.0f, 0, &BottomMenu);
	RenderBackgroundShadow(&BottomMenu, true, Rounding);
	BottomMenu.HSplitTop(ButtonHeight, &Button, &TopMenu);
	static CButtonContainer s_QuitButton;
	CMenuAction QuitAction = CMenuAction::Direct(Localize("Quit"), &CMenus::ActionShowQuitPopup, KEY_Q, CUI::HOTKEY_ESCAPE, 0, CUIRect::CORNER_ALL, Rounding, 0.5f);
	DoMenuActionButton(&s_QuitButton, &QuitAction, &Button);

	// render version
	CUIRect Version;
	MainView.HSplitBottom(50.0f, 0, &Version);
	Version.VMargin(50.0f, &Version);
	UI()->DoLabel(&Version, GAME_RELEASE_VERSION, 14.0f, TEXTALIGN_TR);

	if(str_comp(Client()->LatestVersion(), "0") != 0)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), Localize("Teeworlds %s is out! Download it at www.teeworlds.com!"), Client()->LatestVersion());
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
		TextRender()->TextSecondaryColor(0.0f, 0.0f, 0.0f, 0.5f);
		UI()->DoLabel(&TopMenu, aBuf, 14.0f, TEXTALIGN_MC, TopMenu.w * 0.9f, true);
		TextRender()->TextColor(CUI::ms_DefaultTextColor);
		TextRender()->TextSecondaryColor(CUI::ms_DefaultTextOutlineColor);
	}
}
