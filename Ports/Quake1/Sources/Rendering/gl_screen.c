/*
   Copyright (C) 1996-1997 Id Software, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 */

// Master for refresh, status bar, console, chat, notify, etc

#include "Client/client.h"
#include "Client/console.h"
#include "Client/input.h"
#include "Client/keys.h"
#include "Client/menu.h"
#include "Client/sbar.h"
#include "Client/screen.h"
#include "Common/cmd.h"
#include "Common/cvar.h"
#include "Common/quakedef.h"
#include "Common/sys.h"
#include "Rendering/draw.h"
#include "Rendering/glquake.h"
#include "Sound/sound.h"

#include "OpenGLES/OpenGLWrapper.h"

#include <stdlib.h>
#include <string.h>

void GL_Set2D();

/*

   background clear
   rendering
   turtle/net/ram icons
   sbar
   centerprint / slow centerprint
   notify lines
   intermission / finale overlay
   loading plaque
   console
   menu

   required background clears
   required update regions


   syncronous draw mode or async
   One off screen buffer, with updates either copied or xblited
   Need to double buffer?


   async draw will require the refresh area to be cleared, because it will be
   xblited, but sync draw can just ignore it.

   sync
   draw

   CenterPrint ()
   SlowPrint ()
   Screen_Update ();
   Con_Printf ();

   net
   turn off messages option

   the refresh is allways rendered, unless the console is full screen


   console is:
        notify lines
        half
        full


 */

int glx, gly, glwidth, glheight;

float scr_con_current;
float scr_conlines; // lines of console to display

float oldscreensize, oldfov;
cvar_t scr_viewsize = { "viewsize", "100", true };
cvar_t scr_fov = { "fov", "90" }; // 10 - 170
cvar_t scr_conspeed = { "scr_conspeed", "1" };
cvar_t scr_centertime = { "scr_centertime", "2" };
cvar_t scr_showram = { "showram", "1" };
cvar_t scr_showturtle = { "showturtle", "0" };
cvar_t scr_showpause = { "showpause", "1" };
cvar_t scr_printspeed = { "scr_printspeed", "8" };
cvar_t gl_triplebuffer = { "gl_triplebuffer", "1", true };
cvar_t cl_drawfps = { "cl_drawfps", "0", true };

extern cvar_t crosshair;

qboolean scr_initialized; // ready to draw

qpic_t *scr_ram;
qpic_t *scr_net;
qpic_t *scr_turtle;

int clearconsole;
int clearnotify;

int sb_lines;

viddef_t vid; // global video state

vrect_t scr_vrect;

qboolean scr_disabled_for_loading;
qboolean scr_drawloading;
float scr_disabled_time;

qboolean block_drawing;

void SCR_ScreenShot_f();

/*
   ===============================================================================

   CENTER PRINTING

   ===============================================================================
 */

char scr_centerstring[1024];
float scr_centertime_start; // for slow victory printing
float scr_centertime_off;
int scr_center_lines;
int scr_erase_lines;
int scr_erase_center;

/*
   Called for important messages that should stay in the center of the screen
   for a few moments
 */
void SCR_CenterPrint(char *str)
{
	strncpy(scr_centerstring, str, sizeof(scr_centerstring) - 1);
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}

void SCR_DrawCenterString()
{
	char *start;
	int l;
	int j;
	int x, y;
	int remaining;

	// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.height * 0.35f;
	else
		y = 48;

	do
	{
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;

		x = (vid.width - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8)
		{
			Draw_Character(x, y, start[j]);
			if (!remaining--)
				return;
		}

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++; // skip the \n
	}
	while (1);
}

void SCR_CheckDrawCenterString()
{
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;

	if (scr_centertime_off <= 0 && !cl.intermission)
		return;

	if (key_dest != key_game)
		return;

	SCR_DrawCenterString();
}

float CalcFov(float fov_x, float width, float height)
{
	float a;
	float x;

	if (fov_x < 1 || fov_x > 179)
		Sys_Error("Bad fov: %f", fov_x);

	x = width / tan(fov_x / 360 * Q_PI);

	a = atan(height / x);

	a = a * 360 / Q_PI;

	return a;
}

/*
   Must be called whenever vid changes
   Internal use only
 */
static void SCR_CalcRefdef()
{
	float size;
	int h;
	qboolean full = false;

	vid.recalc_refdef = 0;

	// force the status bar to redraw
	Sbar_Changed();

	//========================================

	// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_Set("viewsize", "30");
	if (scr_viewsize.value > 120)
		Cvar_Set("viewsize", "120");

	// bound field of view
	if (scr_fov.value < 10)
		Cvar_Set("fov", "10");
	if (scr_fov.value > 170)
		Cvar_Set("fov", "170");

	// intermission is always full screen
	if (cl.intermission)
		size = 120;
	else
		size = scr_viewsize.value;

	if (size >= 120)
		sb_lines = 0;           // no status bar at all
	else
	if (size >= 110)
		sb_lines = 24;                // no inventory
	else
		sb_lines = 24 + 16 + 8;

	if (scr_viewsize.value >= 100.0f)
	{
		full = true;
		size = 100.0f;
	}
	else
		size = scr_viewsize.value;
	if (cl.intermission)
	{
		full = true;
		size = 100;
		sb_lines = 0;
	}
	size /= 100.0f;

	h = vid.height - sb_lines;

	r_refdef.vrect.width = vid.width * size;
	if (r_refdef.vrect.width < 96)
	{
		size = 96.0f / r_refdef.vrect.width;
		r_refdef.vrect.width = 96; // min for icons
	}

	r_refdef.vrect.height = vid.height * size;
	if (r_refdef.vrect.height > vid.height - sb_lines)
		r_refdef.vrect.height = vid.height - sb_lines;
	if (r_refdef.vrect.height > vid.height)
		r_refdef.vrect.height = vid.height;
	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width) / 2;
	if (full)
		r_refdef.vrect.y = 0;
	else
		r_refdef.vrect.y = (h - r_refdef.vrect.height) / 2;

	r_refdef.fov_x = scr_fov.value;
	r_refdef.fov_y = CalcFov(r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

	scr_vrect = r_refdef.vrect;
}

/*
   Keybinding command
 */
void SCR_SizeUp_f()
{
	Cvar_SetValue("viewsize", scr_viewsize.value + 10);
	vid.recalc_refdef = 1;
}

/*
   Keybinding command
 */
void SCR_SizeDown_f()
{
	Cvar_SetValue("viewsize", scr_viewsize.value - 10);
	vid.recalc_refdef = 1;
}

//============================================================================

void SCR_Init()
{
	Cvar_RegisterVariable(&scr_fov);
	Cvar_RegisterVariable(&scr_viewsize);
	Cvar_RegisterVariable(&scr_conspeed);
	Cvar_RegisterVariable(&scr_showram);
	Cvar_RegisterVariable(&scr_showturtle);
	Cvar_RegisterVariable(&scr_showpause);
	Cvar_RegisterVariable(&scr_centertime);
	Cvar_RegisterVariable(&scr_printspeed);
	Cvar_RegisterVariable(&gl_triplebuffer);
	Cvar_RegisterVariable(&cl_drawfps);

	//
	// register our commands
	//
	Cmd_AddCommand("screenshot", SCR_ScreenShot_f);
	Cmd_AddCommand("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand("sizedown", SCR_SizeDown_f);

	scr_ram = Draw_PicFromWad("ram");
	scr_net = Draw_PicFromWad("net");
	scr_turtle = Draw_PicFromWad("turtle");

	scr_initialized = true;
}

void SCR_DrawRam()
{
	if (!scr_showram.value)
		return;

	// Draw_Pic(scr_vrect.x + 32, scr_vrect.y, scr_ram); // Not used anymore
}

void SCR_DrawTurtle()
{
	static int count;

	if (!scr_showturtle.value)
		return;

	if (host_frametime < 0.1f)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_Pic(scr_vrect.x, scr_vrect.y, scr_turtle);
}

void SCR_DrawNet()
{
	if (realtime - cl.last_received_message < 0.3f)
		return;
	if (cls.demoplayback)
		return;
	Draw_Pic(scr_vrect.x + 64, scr_vrect.y, scr_net);
}

void SCR_DrawPause()
{
	if (!scr_showpause.value) // turn off for screenshots
		return;
	if (!cl.paused)
		return;
	qpic_t *pic = Draw_CachePic("gfx/pause.lmp");
	Draw_Pic((vid.width - pic->width) / 2, (vid.height - 48 - pic->height) / 2, pic);
}

void SCR_DrawLoading()
{
	if (!scr_drawloading)
		return;
	qpic_t *pic = Draw_CachePic("gfx/loading.lmp");
	Draw_Pic((vid.width - pic->width) / 2, (vid.height - 48 - pic->height) / 2, pic);
}

void SCR_SetUpToDrawConsole()
{
	Con_CheckResize();

	if (scr_drawloading)
		return; // never a console with loading plaque

	// decide on the height of the console
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

	if (con_forcedup)
	{
		scr_conlines = vid.height; // full screen
		scr_con_current = scr_conlines;
	}
    else if (key_dest == key_console)
		scr_conlines = vid.height / 2; // half screen
	else
		scr_conlines = 0; // none visible

    float step = scr_conspeed.value > 0.0f ? vid.height / scr_conspeed.value * host_frametime : vid.height;
	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= step;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;
	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += step;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (clearconsole++ < vid.numpages)
		Sbar_Changed();
	else if (clearnotify++ < vid.numpages)
        ;
	else
		con_notifylines = 0;
}

void SCR_DrawConsole()
{
	if (scr_con_current)
	{
		Con_DrawConsole(scr_con_current, true);
		clearconsole = 0;
	}
	else
	{
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify();                                               // only draw notify in game
	}
}

typedef struct _TargaHeader
{
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
} TargaHeader;

void SCR_ScreenShot_f()
{
	//
	// find a file name to save it to
	//
	char pcxname[80];
	strcpy(pcxname, "quake00.tga");

    int i;
	for (i = 0; i <= 99; i++)
	{
		pcxname[5] = i / 10 + '0';
		pcxname[6] = i % 10 + '0';
        char checkname[MAX_OSPATH];
		sprintf(checkname, "%s/%s", com_writableGamedir, pcxname);
		if (Sys_FileTime(checkname) == -1)
			break;                             // file doesn't exist
	}
	if (i == 100)
	{
		Con_Printf("SCR_ScreenShot_f: Couldn't create a TGA file\n");
		return;
	}

	byte *buffer = malloc(glwidth * glheight * 3 + 18);
	memset(buffer, 0, 18);
	buffer[2] = 2; // uncompressed type
	buffer[12] = glwidth & 255;
	buffer[13] = glwidth >> 8;
	buffer[14] = glheight & 255;
	buffer[15] = glheight >> 8;
	buffer[16] = 24; // pixel size

	glReadPixels(glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer + 18);

	// swap rgb to bgr
	int c = 18 + glwidth * glheight * 3;
	for (i = 18; i < c; i += 3)
	{
		int temp = buffer[i];
		buffer[i] = buffer[i + 2];
		buffer[i + 2] = temp;
	}
	COM_WriteFile(pcxname, buffer, glwidth * glheight * 3 + 18);

	free(buffer);
	Con_Printf("Wrote %s\n", pcxname);
}

void SCR_BeginLoadingPlaque()
{
	S_StopAllSounds(true);

	if (cls.state != ca_connected)
		return;

	if (cls.signon != SIGNONS)
		return;

	// redraw with no console and the loading plaque
	Con_ClearNotify();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	Sbar_Changed();
	SCR_UpdateScreen();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;
}

void SCR_EndLoadingPlaque()
{
	scr_disabled_for_loading = false;
	Con_ClearNotify();
}

char *scr_notifystring;
qboolean scr_drawdialog;

void SCR_DrawNotifyString()
{
	char *start;
	int l;
	int j;
	int x, y;

	start = scr_notifystring;

	y = vid.height * 0.35f;

	do
	{
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;

		x = (vid.width - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8)
			Draw_Character(x, y, start[j]);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++; // skip the \n
	}
	while (1);
}

/*
   Displays a text string in the center of the screen and waits for a Y or N
   keypress.
 */
int SCR_ModalMessage(char *text)
{
	if (cls.state == ca_dedicated)
		return true;

	scr_notifystring = text;

	// draw a fresh screen
	scr_drawdialog = true;
	SCR_UpdateScreen();
	scr_drawdialog = false;

	do
	{
		key_count = -1; // wait for a key down and up
		Sys_SendKeyEvents();
	}
	while (key_lastpress != 'y' && key_lastpress != 'n' && key_lastpress != K_ESCAPE);

	SCR_UpdateScreen();

	return key_lastpress == 'y';
}

/*
   Brings the console down and fades the palettes back to normal
 */
void SCR_BringDownConsole()
{
	scr_centertime_off = 0;

	for (int i = 0; i < 20 && scr_conlines != scr_con_current; i++)
		SCR_UpdateScreen();

	cl.cshifts[0].percent = 0; // no area contents palette on next frame
}

void SCR_TileClear()
{
	if (r_refdef.vrect.x > 0)
	{
		// left
		Draw_TileClear(0, 0, r_refdef.vrect.x, vid.height - sb_lines);
		// right
		Draw_TileClear(r_refdef.vrect.x + r_refdef.vrect.width, 0,
			vid.width - r_refdef.vrect.x + r_refdef.vrect.width,
			vid.height - sb_lines);
	}
	if (r_refdef.vrect.y > 0)
	{
		// top
		Draw_TileClear(r_refdef.vrect.x, 0,
			r_refdef.vrect.x + r_refdef.vrect.width,
			r_refdef.vrect.y);
		// bottom
		Draw_TileClear(r_refdef.vrect.x,
			r_refdef.vrect.y + r_refdef.vrect.height,
			r_refdef.vrect.width,
			vid.height - sb_lines -
			(r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

/*
   This is called every frame, and can also be called explicitly to flush
   text to the screen.

   WARNING: be very careful calling this from elsewhere, because the refresh
   needs almost the entire 256k of stack space!
 */
void SCR_UpdateScreen()
{
	if (block_drawing)
		return;

	vid.numpages = 2 + gl_triplebuffer.value;

	if (scr_disabled_for_loading)
	{
		if (realtime - scr_disabled_time > 60)
		{
			scr_disabled_for_loading = false;
			Con_Printf("load failed.\n");
		}
		else
			return;
	}

	if (!scr_initialized || !con_initialized)
		return;                                    // not initialized yet

	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);

	//
	// determine size of refresh window
	//
	if (oldfov != scr_fov.value)
	{
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}

	if (oldscreensize != scr_viewsize.value)
	{
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = true;
	}

	if (vid.recalc_refdef)
		SCR_CalcRefdef();

	//
	// do 3D refresh drawing, and then update the screen
	//
	SCR_SetUpToDrawConsole();

	V_RenderView();

	GL_Set2D();

	//
	// draw any areas not covered by the refresh
	//
	SCR_TileClear();

	if (scr_drawdialog)
	{
		Sbar_Draw();
		Draw_FadeScreen();
		SCR_DrawNotifyString();
	}
	else
	if (scr_drawloading)
	{
		SCR_DrawLoading();
		Sbar_Draw();
	}
	else
	if (cl.intermission == 1 && key_dest == key_game)
	{
		Sbar_IntermissionOverlay();
	}
	else
	if (cl.intermission == 2 && key_dest == key_game)
	{
		Sbar_FinaleOverlay();
		SCR_CheckDrawCenterString();
	}
	else
	{
		if (crosshair.value)
			Draw_Character(scr_vrect.x + scr_vrect.width / 2, scr_vrect.y + scr_vrect.height / 2, '+');

		SCR_DrawRam();
		SCR_DrawNet();
		SCR_DrawTurtle();
		SCR_DrawPause();
		SCR_CheckDrawCenterString();
		Sbar_Draw();
		SCR_DrawConsole();
		M_Draw();
	}

    if (cl_drawfps.value)
    {
        char string[32];
        double dt = real_frametime;
        snprintf(string, 32, "%5.1f fps", dt > 0.0f ? 1.0f / dt : 0.0f);
        string[31] = 0;
        Draw_String(30*8, 0, string);
    }

	V_UpdatePalette();

	GL_EndRendering();
}
