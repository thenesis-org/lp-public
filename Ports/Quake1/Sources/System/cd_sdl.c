/*
   Some of this may not work. I'm not overly familiar with SDL, I just sort
   of podged this together from the SDL headers, and the other cd-rom code.

   Mark Baker <homer1@together.net>
 */

#include <SDL2/SDL.h>

#include "Common/cmd.h"
#include "Common/quakedef.h"

static bool enabled = false;

static void CD_f();

static void CDAudio_Eject()
{
}

void CDAudio_Play(byte track, qboolean looping)
{
}

void CDAudio_Stop()
{
}

void CDAudio_Pause()
{
}

void CDAudio_Resume()
{
}

void CDAudio_Update()
{
}

int CDAudio_Init()
{
    Cmd_AddCommand("cd", CD_f);
	return 0;
}

void CDAudio_Shutdown()
{
}

static void CD_f()
{
	if (Cmd_Argc() < 2)
		return;

	char *command = Cmd_Argv(1);
    
	if (!Q_strcasecmp(command, "on"))
	{
		enabled = true;
	}
	if (!Q_strcasecmp(command, "off"))
	{
		CDAudio_Stop();
		enabled = false;
		return;
	}
	if (!Q_strcasecmp(command, "play"))
	{
		CDAudio_Play(Q_atoi(Cmd_Argv(2)), false);
		return;
	}
	if (!Q_strcasecmp(command, "loop"))
	{
		CDAudio_Play(Q_atoi(Cmd_Argv(2)), true);
		return;
	}
	if (!Q_strcasecmp(command, "stop"))
	{
		CDAudio_Stop();
		return;
	}
	if (!Q_strcasecmp(command, "pause"))
	{
		CDAudio_Pause();
		return;
	}
	if (!Q_strcasecmp(command, "resume"))
	{
		CDAudio_Resume();
		return;
	}
	if (!Q_strcasecmp(command, "eject"))
	{
		CDAudio_Eject();
		return;
	}
	if (!Q_strcasecmp(command, "info"))
	{
		return;
	}
}
