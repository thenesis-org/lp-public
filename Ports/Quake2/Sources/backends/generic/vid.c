/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * This is the "heart" of the id Tech 2 refresh engine. This file
 * implements the main window in which Quake II is running. The window
 * itself is created by the SDL backend, but here the refresh module is
 * loaded, initialized and it's interaction with the operating system
 * implemented. This code is also the interconnect between the input
 * system (the mouse) and the keyboard system, both are here tied
 * together with the refresher. The direct interaction between the
 * refresher and those subsystems are the main cause for the very
 * acurate and precise input controls of the id Tech 2.
 *
 * This implementation works for Windows and unixoid systems, but
 * other platforms may need an own implementation!
 *
 * =======================================================================
 */

#include "client/client.h"
#include "client/keyboard.h"

#include <assert.h>
#include <errno.h>

/* Console variables that we need to access from this module */
cvar_t *vid_gamma;
cvar_t *vid_fullscreen;

/* Global variables used internally by this module */
viddef_t viddef; /* global video state; used by other modules */
qboolean ref_active = false; /* Is the refresher being used? */

#define MAXPRINTMSG 4096

void VID_Printf(int print_level, char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end(argptr);

	if (print_level == PRINT_ALL)
	{
		Com_Printf("%s", msg);
	}
	else
	{
		Com_DPrintf("%s", msg);
	}
}

void VID_Error(int err_level, char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end(argptr);

	Com_Error(err_level, "%s", msg);
}

/*
 * Console command to re-start the video mode and refresh. We do this
 * simply by setting the modified flag for the vid_fullscreen variable, which will
 * cause the entire video mode and refreshto be reset on the next frame.
 */
void VID_Restart_f()
{
	vid_fullscreen->modified = true;
}

void VID_NewWindow(int width, int height)
{
	viddef.width = width;
	viddef.height = height;
}

qboolean VID_LoadRefresh()
{
	VID_Shutdown();

	Com_Printf("----- renderer initialization -----\n");

	// Declare the renderer as active
	ref_active = true;

	if (R_Init() == -1)
	{
		VID_Shutdown();
		return false;
	}

	// Ensure that all key states are cleared.
	Key_MarkAllUp();

	Com_Printf("------------------------------------\n\n");
	return true;
}

/*
 * This function gets called once just before drawing each frame, and
 * it's sole purpose in life is to check to see if any of the video mode
 * parameters have changed, and if they have to update the refresh
 * and/or video mode to match.
 */
void VID_CheckChanges()
{
	if (vid_fullscreen->modified)
	{
		S_StopAllSounds();

		/* refresh has changed */
		cl.refresh_prepped = false;
		cl.cinematicpalette_active = false;
		cls.disable_screen = true;

		// Proceed to reboot the refresher
		if (!VID_LoadRefresh())
		{
			VID_Error(ERR_FATAL, "Could not initialize rendering");
		}
		cls.disable_screen = false;
	}
}

void VID_Init()
{
	/* Create the video variables so we know how to start the graphics drivers */
	vid_fullscreen = Cvar_Get("vid_fullscreen", GL_FULLSCREEN_DEFAULT_STRING, CVAR_ARCHIVE);
	vid_gamma = Cvar_Get("vid_gamma", "1", CVAR_ARCHIVE);

	/* Add some console commands that we want to handle */
	Cmd_AddCommand("vid_restart", VID_Restart_f);

	/* Start the graphics mode and load refresh DLL */
	VID_CheckChanges();
}

void VID_Shutdown()
{
	if (ref_active)
	{
		R_Shutdown();
	}

	// Declare the refresher as inactive
	ref_active = false;
}
