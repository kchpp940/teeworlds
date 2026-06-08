/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>

#include <engine/preflight.h>

#include "SDL.h"

static int PreflightSDLGraphicsCheckImpl(IPreflight *pPreflight, void *pUser)
{
	dbg_msg("preflight", "running graphics dependency check...");

	if(SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SDL video subsystem initialization failed: %s", SDL_GetError());
		pPreflight->AddResult(PRECHECK_GRAPHICS, PRESEVERITY_ERROR, aBuf,
			"1. Install SDL 2.0 or later: https://www.libsdl.org/download-2.0.php\n"
			"   macOS:   brew install sdl2\n"
			"   Ubuntu:  sudo apt-get install libsdl2-dev\n"
			"   Windows: Download SDL2 development libraries and set PATH\n"
			"2. Verify your GPU drivers are up to date\n"
			"3. Try running with HEADLESS_CLIENT=ON if graphics are not required"
		);
		return -1;
	}

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	pPreflight->AddResult(PRECHECK_GRAPHICS, PRESEVERITY_INFO,
		"Graphics subsystem (SDL2) is available.", 0);
	return 0;
}

static int PreflightSDLAudioCheckImpl(IPreflight *pPreflight, void *pUser)
{
	dbg_msg("preflight", "running audio dependency check...");

	if(SDL_Init(SDL_INIT_AUDIO) != 0)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SDL audio subsystem initialization failed: %s", SDL_GetError());
		pPreflight->AddResult(PRECHECK_AUDIO, PRESEVERITY_WARNING, aBuf,
			"1. Check that your audio device is working and not muted\n"
			"2. Verify SDL2 was compiled with audio support\n"
			"3. Install audio development libraries:\n"
			"   Ubuntu:  sudo apt-get install libasound2-dev libpulse-dev\n"
			"   macOS:   Audio should work natively with CoreAudio\n"
			"4. The game will still run but without sound"
		);
		return -1;
	}

	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	pPreflight->AddResult(PRECHECK_AUDIO, PRESEVERITY_INFO,
		"Audio subsystem (SDL2) is available.", 0);
	return 0;
}

FPreflightCustomCheck g_pfnPreflightSDLGraphicsCheck = PreflightSDLGraphicsCheckImpl;
FPreflightCustomCheck g_pfnPreflightSDLAudioCheck = PreflightSDLAudioCheckImpl;
