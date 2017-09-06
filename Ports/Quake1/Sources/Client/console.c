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
#include "Client/client.h"
#include "Client/console.h"
#include "Client/keys.h"
#include "Client/screen.h"
#include "Common/common.h"
#include "Common/cmd.h"
#include "Common/cvar.h"
#include "Common/quakedef.h"
#include "Common/sys.h"
#include "Rendering/draw.h"
#include "Sound/sound.h"

#include <stdarg.h>
#include <string.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <fcntl.h>

int con_linewidth;

float con_cursorspeed = 4;

#define         CON_TEXTSIZE 16384

qboolean con_forcedup; // because no entities to refresh

int con_totallines; // total lines in console scrollback
int con_backscroll; // lines up from bottom to display
int con_current; // where next message will be printed
int con_x; // offset in current line for next print
char *con_text = 0;

cvar_t con_notifytime = { "con_notifytime", "3" }; //seconds

#define NUM_CON_TIMES 4
float con_times[NUM_CON_TIMES]; // realtime time the line was generated
// for transparent notify lines

int con_vislines;

qboolean con_debuglog;

#define MAXCMDLINE 256
extern char key_lines[32][MAXCMDLINE];
extern int edit_line;
extern int key_linepos;

qboolean con_initialized;

int con_notifylines; // scan lines to clear for notify lines

extern void M_Menu_Main_f();

void Con_ToggleConsole_f()
{
	if (key_dest == key_console)
	{
		if (cls.state == ca_connected)
		{
			key_dest = key_game;
			key_lines[edit_line][1] = 0; // clear any typing
			key_linepos = 1;
		}
		else
		{
			M_Menu_Main_f();
		}
	}
	else
		key_dest = key_console;

	SCR_EndLoadingPlaque();
	memset(con_times, 0, sizeof(con_times));
}

void Con_Clear_f()
{
	if (con_text)
		Q_memset(con_text, ' ', CON_TEXTSIZE);
}

void Con_ClearNotify()
{
	int i;

	for (i = 0; i < NUM_CON_TIMES; i++)
		con_times[i] = 0;
}

extern qboolean team_message;

void Con_MessageMode_f()
{
	key_dest = key_message;
	team_message = false;
}

void Con_MessageMode2_f()
{
	key_dest = key_message;
	team_message = true;
}

/*
   If the line width has changed, reformat the buffer.
 */
void Con_CheckResize()
{
	int width = (vid.width >> 3) - 2;
	if (width == con_linewidth)
		return;

	if (width < 1) // video hasn't been initialized yet
	{
		width = 38;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		Q_memset(con_text, ' ', CON_TEXTSIZE);
	}
	else
	{
		int oldwidth = con_linewidth;
		con_linewidth = width;
		int oldtotallines = con_totallines;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		int numlines = oldtotallines;
		if (con_totallines < numlines)
			numlines = con_totallines;

		int numchars = oldwidth;
		if (con_linewidth < numchars)
			numchars = con_linewidth;

        char tbuf[CON_TEXTSIZE];
		Q_memcpy(tbuf, con_text, CON_TEXTSIZE);
		Q_memset(con_text, ' ', CON_TEXTSIZE);

		for (int i = 0; i < numlines; i++)
		{
			for (int j = 0; j < numchars; j++)
			{
				con_text[(con_totallines - 1 - i) * con_linewidth + j] =
				        tbuf[((con_current - i + oldtotallines) %
				              oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify();
	}

	con_backscroll = 0;
	con_current = con_totallines - 1;
}

#define MAXGAMEDIRLEN 1000

void Con_Init()
{
	con_debuglog = COM_CheckParm("-condebug");
	if (con_debuglog)
	{
        const char *t2 = "/qconsole.log";
		if (strlen(com_writableGamedir) < (MAXGAMEDIRLEN - strlen(t2)))
		{
            char temp[MAXGAMEDIRLEN + 1];
			sprintf(temp, "%s%s", com_writableGamedir, t2);
			unlink(temp);
		}
	}

	con_text = Hunk_AllocName(CON_TEXTSIZE, "context");
	Q_memset(con_text, ' ', CON_TEXTSIZE);
	con_linewidth = -1;
	Con_CheckResize();

	Con_Printf("Console initialized.\n");

	//
	// register our commands
	//
	Cvar_RegisterVariable(&con_notifytime);

	Cmd_AddCommand("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand("messagemode", Con_MessageMode_f);
	Cmd_AddCommand("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand("clear", Con_Clear_f);
	con_initialized = true;
}

void Con_Linefeed()
{
	con_x = 0;
	con_current++;
	Q_memset(&con_text[(con_current % con_totallines) * con_linewidth]
		, ' ', con_linewidth);
}

/*
   Handles cursor positioning, line wrapping, etc
   All console printing must go through this in order to be logged to disk
   If no console is visible, the notify window will pop up.
 */
void Con_Print(char *txt)
{
	int y;
	int c, l;
	static int cr;
	int mask;

	con_backscroll = 0;

	if (txt[0] == 1)
	{
		mask = 128; // go to colored text
		S_LocalSound("misc/talk.wav");
		// play talk wav
		txt++;
	}
	else
	if (txt[0] == 2)
	{
		mask = 128; // go to colored text
		txt++;
	}
	else
		mask = 0;

	while ((c = *txt))
	{
		// count word length
		for (l = 0; l < con_linewidth; l++)
			if (txt[l] <= ' ')
				break;


		// word wrap
		if (l != con_linewidth && (con_x + l > con_linewidth))
			con_x = 0;

		txt++;

		if (cr)
		{
			con_current--;
			cr = false;
		}

		if (!con_x)
		{
			Con_Linefeed();
			// mark time for transparent overlay
			if (con_current >= 0)
				con_times[con_current % NUM_CON_TIMES] = realtime;
		}

		switch (c)
		{
		case '\n':
			con_x = 0;
			break;

		case '\r':
			con_x = 0;
			cr = 1;
			break;

		default: // display character and advance
			y = con_current % con_totallines;
			con_text[y * con_linewidth + con_x] = c | mask;
			con_x++;
			if (con_x >= con_linewidth)
				con_x = 0;
			break;
		}
	}
}

void Con_DebugLog(char *file, char *fmt, ...)
{
	char data[4096];

	va_list argptr;
	va_start(argptr, fmt);
	vsnprintf(data, 4096, fmt, argptr);
    data[4095] = 0;
	va_end(argptr);
	int fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0666);
	write(fd, data, strlen(data));
	close(fd);
}

/*
   Handles cursor positioning, line wrapping, etc
 */
#define MAXPRINTMSG 4096
// FIXME: make a buffer size safe vsprintf?
void Con_Printf(char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];
	static qboolean inupdate;

	va_start(argptr, fmt);
	vsprintf(msg, fmt, argptr);
	va_end(argptr);

	// also echo to debugging console
	Sys_Printf("%s", msg); // also echo to debugging console

	// log all messages to file
	if (con_debuglog)
		Con_DebugLog(va("%s/qconsole.log", com_writableGamedir), "%s", msg);

	if (!con_initialized)
		return;

	if (cls.state == ca_dedicated)
		return;                         // no graphics mode

	// write it to the scrollable buffer
	Con_Print(msg);

	// update the screen if the console is displayed
	if (cls.signon != SIGNONS && !scr_disabled_for_loading)
	{
		// protect against infinite loop if something in SCR_UpdateScreen calls
		// Con_Printd
		if (!inupdate)
		{
			inupdate = true;
			SCR_UpdateScreen();
			inupdate = false;
		}
	}
}

/*
   A Con_Printf that only shows up if the "developer" cvar is set
 */
void Con_DPrintf(char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	if (!developer.value)
		return;                // don't confuse non-developers with techie stuff...

	va_start(argptr, fmt);
	vsprintf(msg, fmt, argptr);
	va_end(argptr);

	Con_Printf("%s", msg);
}

/*
   Okay to call even when the screen can't be updated
 */
void Con_SafePrintf(char *fmt, ...)
{
	va_list argptr;
	char msg[1024];
	int temp;

	va_start(argptr, fmt);
	vsprintf(msg, fmt, argptr);
	va_end(argptr);

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;
	Con_Printf("%s", msg);
	scr_disabled_for_loading = temp;
}

/*
   ==============================================================================

   DRAWING

   ==============================================================================
 */

/*
   The input line scrolls horizontally if typing goes beyond the right edge
 */
static void Con_DrawInput()
{
	if (key_dest != key_console && !con_forcedup)
		return;                                        // don't draw anything

	char *text = key_lines[edit_line];

	// add the cursor frame
	text[key_linepos] = 10 + ((int)(realtime * con_cursorspeed) & 1);

	// fill out remainder with spaces
	for (int i = key_linepos + 1; i < con_linewidth; i++)
		text[i] = ' ';

	//	prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;

	for (int i = 0; i < con_linewidth; i++)
		Draw_Character((i + 1) << 3, con_vislines - 16, text[i]);

	// remove cursor
	key_lines[edit_line][key_linepos] = 0;
}

/*
   Draws the last few lines of output transparently over the game top
 */
void Con_DrawNotify()
{
    Draw_CharBegin();

	int x, v;
	char *text;
	int i;
	float time;
	extern char chat_buffer[];

	v = 0;
	for (i = con_current - NUM_CON_TIMES + 1; i <= con_current; i++)
	{
		if (i < 0)
			continue;
		time = con_times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = realtime - time;
		if (time > con_notifytime.value)
			continue;
		text = con_text + (i % con_totallines) * con_linewidth;

		clearnotify = 0;

		for (x = 0; x < con_linewidth; x++)
			Draw_Character((x + 1) << 3, v, text[x]);

		v += 8;
	}

	if (key_dest == key_message)
	{
		clearnotify = 0;

		x = 0;

		Draw_String(8, v, "say:");
		while (chat_buffer[x])
		{
			Draw_Character((x + 5) << 3, v, chat_buffer[x]);
			x++;
		}
		Draw_Character((x + 5) << 3, v, 10 + ((int)(realtime * con_cursorspeed) & 1));
		v += 8;
	}

	if (v > con_notifylines)
		con_notifylines = v;

    Draw_CharEnd();
}

/*
   Draws the console with the solid background
   The typing input line at the bottom should only be drawn if typing is allowed
 */
void Con_DrawConsole(int lines, qboolean drawinput)
{
	if (lines <= 0)
		return;

	// draw the background
	Draw_ConsoleBackground(lines);

    Draw_CharBegin();

	// draw the text
	con_vislines = lines;

	int rows = (lines - 16) >> 3; // rows of text to draw
	int y = lines - 16 - (rows << 3); // may start slightly negative

	for (int i = con_current - rows + 1; i <= con_current; i++, y += 8)
	{
		int j = i - con_backscroll;
		if (j < 0)
			j = 0;
		char *text = con_text + (j % con_totallines) * con_linewidth;

		for (int x = 0; x < con_linewidth; x++)
			Draw_Character((x + 1) << 3, y, text[x]);
	}

	// draw the input prompt, user text, and cursor if desired
	if (drawinput)
		Con_DrawInput();

    Draw_CharEnd();
}

void Con_NotifyBox(char *text)
{
	// during startup for sound / cd warnings
	Con_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	Con_Printf(text);

	Con_Printf("Press a key.\n");
	Con_Printf("\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	key_count = -2; // wait for a key down and up
	key_dest = key_console;

	do
	{
		double t1 = Sys_FloatTime();
		SCR_UpdateScreen();
		Sys_SendKeyEvents();
		double t2 = Sys_FloatTime();
		realtime += t2 - t1; // make the cursor blink
	}
	while (key_count < 0);

	Con_Printf("\n");
	key_dest = key_game;
	realtime = 0; // put the cursor back to invisible
}
