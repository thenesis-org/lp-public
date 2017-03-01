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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * =======================================================================
 *
 * This file implements the non generic part of the menu system, e.g.
 * the menu shown to the player. Beware! This code is very fragile and
 * should only be touched with great care and exessive testing.
 * Otherwise strange things and hard to track down bugs can occure. In a
 * better world someone would rewrite this file to something more like
 * Quake III Team Arena.
 *
 * =======================================================================
 */

#include "client/client.h"
#include "client/keyboard.h"
#include "client/menu/menu.h"

#include <ctype.h>

float ClampCvar(float min, float max, float value)
{
	if (value < min)
		value = min;
	if (value > max)
		value = max;
	return value;
}

char *menu_in_sound = "misc/menu1.wav";
char *menu_move_sound = "misc/menu2.wav";
char *menu_out_sound = "misc/menu3.wav";
qboolean m_entersound;

static MenuDrawFunc m_drawfunc;
static MenuKeyFunc m_keyfunc;

/* Maximal number of submenus */
#define MAX_MENU_DEPTH 8

typedef struct
{
	void (*draw)(void);
	const char *(*key)(int k);
} menulayer_t;

static menulayer_t m_layers[MAX_MENU_DEPTH];
static int m_menudepth;

void M_Banner(char *name)
{
	int w, h;
	Draw_GetPicSize(&w, &h, name);
	float scale = SCR_GetMenuScale();
	float x = viddef.width / 2 - (w * scale) / 2, y = viddef.height / 2 - (110 * scale);
	Draw_PicScaled(x, y, name, scale);
}

void M_ForceMenuOff()
{
	m_drawfunc = NULL;
	m_keyfunc = NULL;
	cls.key_dest = key_game;
	m_menudepth = 0;
	Key_MarkAllUp();
	Cvar_Set("paused", "0");
}

void M_PopMenu()
{
	S_StartLocalSound(menu_out_sound);

	if (m_menudepth < 1)
	{
		Com_Error(ERR_FATAL, "M_PopMenu: depth < 1");
	}

	m_menudepth--;

	m_drawfunc = m_layers[m_menudepth].draw;
	m_keyfunc = m_layers[m_menudepth].key;

	if (!m_menudepth)
	{
		M_ForceMenuOff();
	}
}

/*
 * This crappy function maintaines a stack of opened menus.
 * The steps in this horrible mess are:
 *
 * 1. But the game into pause if a menu is opened
 *
 * 2. If the requested menu is already open, close it.
 *
 * 3. If the requested menu is already open but not
 *    on top, close all menus above it and the menu
 *    itself. This is necessary since an instance of
 *    the reqeuested menu is in flight and will be
 *    displayed.
 *
 * 4. Save the previous menu on top (which was in flight)
 *    to the stack and make the requested menu the menu in
 *    flight.
 */
void M_PushMenu(MenuDrawFunc draw, MenuKeyFunc key)
{
	int i;
	int alreadyPresent = 0;

	if ((Cvar_VariableValue("maxclients") == 1) &&
	    Com_ServerState())
	{
		Cvar_Set("paused", "1");
	}

	/* if this menu is already open (and on top),
	   close it => toggling behaviour */
	if ((m_drawfunc == draw) && (m_keyfunc == key))
	{
		M_PopMenu();
		return;
	}

	/* if this menu is already present, drop back to
	   that level to avoid stacking menus by hotkeys */
	for (i = 0; i < m_menudepth; i++)
	{
		if ((m_layers[i].draw == draw) &&
		    (m_layers[i].key == key))
		{
			alreadyPresent = 1;
			break;
		}
	}

	/* menu was already opened further down the stack */
	while (alreadyPresent && i <= m_menudepth)
	{
		M_PopMenu(); /* decrements m_menudepth */
	}

	if (m_menudepth >= MAX_MENU_DEPTH)
	{
		Com_Printf("Too many open menus!\n");
		return;
	}

	m_layers[m_menudepth].draw = m_drawfunc;
	m_layers[m_menudepth].key = m_keyfunc;
	m_menudepth++;

	m_drawfunc = draw;
	m_keyfunc = key;

	m_entersound = true;

	cls.key_dest = key_menu;
}

const char* Default_MenuKey(menuframework_s *m, int key)
{
	if (m)
	{
		menucommon_s *item = Menu_ItemAtCursor(m);
		if (item != 0)
		{
			if (item->type == MTYPE_FIELD)
			{
				if (Field_Key((menufield_s *)item, key))
				{
					return NULL;
				}
			}
		}
	}

	const char *sound = NULL;
	switch (key)
	{
	case K_GAMEPAD_SELECT:
	case K_GAMEPAD_B:
	case K_JOY2:
	case K_ESCAPE:
		M_PopMenu();
		sound = menu_out_sound;
		break;
	case K_GAMEPAD_UP:
	case K_UPARROW:
		if (m)
		{
			m->cursor--;
			Menu_AdjustCursor(m, -1);
			sound = menu_move_sound;
		}
		break;
	case K_TAB:
	case K_GAMEPAD_DOWN:
	case K_DOWNARROW:
		if (m)
		{
			m->cursor++;
			Menu_AdjustCursor(m, 1);
			sound = menu_move_sound;
		}
		break;
	case K_GAMEPAD_LEFT:
	case K_LEFTARROW:
		if (m)
		{
			Menu_SlideItem(m, -1);
			sound = menu_move_sound;
		}
		break;
	case K_GAMEPAD_RIGHT:
	case K_RIGHTARROW:
		if (m)
		{
			Menu_SlideItem(m, 1);
			sound = menu_move_sound;
		}
		break;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_JOY1:
	case K_MOUSE1:
	case K_MOUSE2:
	case K_MOUSE3:
	case K_MOUSE4:
	case K_MOUSE5:
	case K_AUX1:
	case K_AUX2:
	case K_AUX3:
	case K_AUX4:
	case K_AUX5:
	case K_AUX6:
	case K_AUX7:
	case K_AUX8:
	case K_AUX9:
	case K_AUX10:
	case K_AUX11:
	case K_AUX12:
	case K_AUX13:
	case K_AUX14:
	case K_AUX15:
	case K_AUX16:
	case K_AUX17:
	case K_AUX18:
	case K_AUX19:
	case K_AUX20:
	case K_AUX21:
	case K_AUX22:
	case K_AUX23:
	case K_AUX24:
	case K_AUX25:
	case K_AUX26:
	case K_AUX27:
	case K_AUX28:
	case K_AUX29:
	case K_AUX30:
	case K_AUX31:
	case K_AUX32:
	case K_KP_ENTER:
	case K_ENTER:
		if (m)
		{
			Menu_SelectItem(m);
		}

		sound = menu_move_sound;
		break;
	}

	return sound;
}

/*
 * Draws one solid graphics character cx and cy are in 320*240
 * coordinates, and will be centered on higher res screens.
 */
static void M_DrawCharacter(int cx, int cy, int num)
{
	float scale = SCR_GetMenuScale();
	Draw_CharScaled(cx + ((int)(viddef.width - 320 * scale) >> 1), cy + ((int)(viddef.height - 240 * scale) >> 1), num, scale);
}

static void M_Print(int x, int y, char *str)
{
	int cx, cy;
	float scale = SCR_GetMenuScale();

	Draw_CharBegin();

	cx = x;
	cy = y;
	while (*str)
	{
		if (*str == '\n')
		{
			cx = x;
			cy += 8;
		}
		else
		{
			M_DrawCharacter(cx * scale, cy * scale, (*str) + 128);
			cx += 8;
		}
		str++;
	}

	Draw_CharEnd();
}

/*
   static void M_DrawPic(int x, int y, char *pic)
   {
        float scale = SCR_GetMenuScale();
    Draw_PicScaled((x + ((viddef.width - 320) >> 1)) * scale,
             (y + ((viddef.height - 240) >> 1)) * scale, pic, scale);
   }
 */

/*
 * Draws an animating cursor with the point at
 * x,y. The pic will extend to the left of x,
 * and both above and below y.
 */
void M_DrawCursor(int x, int y, int f)
{
	char cursorname[80];
	static qboolean cached;
	float scale = SCR_GetMenuScale();

	if (!cached)
	{
		int i;

		for (i = 0; i < NUM_CURSOR_FRAMES; i++)
		{
			Com_sprintf(cursorname, sizeof(cursorname), "m_cursor%d", i);

			Draw_FindPic(cursorname);
		}

		cached = true;
	}

	Com_sprintf(cursorname, sizeof(cursorname), "m_cursor%d", f);
	Draw_PicScaled(x * scale, y * scale, cursorname, scale);
}

void M_DrawTextBox(int x, int y, int width, int lines)
{
	int cx, cy;
	int n;
	float scale = SCR_GetMenuScale();

	Draw_CharBegin();

	/* draw left side */
	cx = x;
	cy = y;
	M_DrawCharacter(cx * scale, cy * scale, 1);

	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter(cx * scale, cy * scale, 4);
	}

	M_DrawCharacter(cx * scale, cy * scale + 8 * scale, 7);

	/* draw middle */
	cx += 8;

	while (width > 0)
	{
		cy = y;
		M_DrawCharacter(cx * scale, cy * scale, 2);

		for (n = 0; n < lines; n++)
		{
			cy += 8;
			M_DrawCharacter(cx * scale, cy * scale, 5);
		}

		M_DrawCharacter(cx * scale, cy * scale + 8 * scale, 8);
		width -= 1;
		cx += 8;
	}

	/* draw right side */
	cy = y;
	M_DrawCharacter(cx * scale, cy * scale, 3);

	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter(cx * scale, cy * scale, 6);
	}

	M_DrawCharacter(cx * scale, cy * scale + 8 * scale, 9);

	Draw_CharEnd();
}

char *m_popup_string;
int m_popup_endtime;

void M_Popup()
{
	int x, y, width, lines;
	int n;
	char *str;

	if (!m_popup_string)
	{
		return;
	}

	if (m_popup_endtime && m_popup_endtime < cls.realtime)
	{
		m_popup_string = NULL;
		return;
	}

	width = lines = n = 0;
	for (str = m_popup_string; *str; str++)
	{
		if (*str == '\n')
		{
			lines++;
			n = 0;
		}
		else
		{
			n++;
			if (n > width)
			{
				width = n;
			}
		}
	}
	if (n)
	{
		lines++;
	}

	if (width)
	{
		width += 2;

		x = (320 - (width + 2) * 8) / 2;
		y = (240 - (lines + 2) * 8) / 3;

		M_DrawTextBox(x, y, width, lines);
		M_Print(x + 16, y + 8, m_popup_string);
	}
}

void M_Init(void)
{
	Cmd_AddCommand("menu_main", MenuMain_enter);
	Cmd_AddCommand("menu_game", MenuGame_enter);
	Cmd_AddCommand("menu_loadgame", MenuLoadGame_enter);
	Cmd_AddCommand("menu_savegame", MenuSaveGame_enter);
	Cmd_AddCommand("menu_joinserver", MenuJoinServer_enter);
	Cmd_AddCommand("menu_addressbook", MenuAddressBook_enter);
	Cmd_AddCommand("menu_startserver", MenuStartServer_enter);
	Cmd_AddCommand("menu_dmoptions", MenuDMOptions_enter);
	Cmd_AddCommand("menu_playerconfig", MenuPlayerConfig_enter);
	Cmd_AddCommand("menu_downloadoptions", MenuDownloadOptions_enter);
	Cmd_AddCommand("menu_credits", MenuCredits_enter);
	Cmd_AddCommand("menu_multiplayer", MenuMultiplayer_enter);
	Cmd_AddCommand("menu_video", MenuVideo_enter);
	Cmd_AddCommand("menu_options", MenuOptions_enter);
	Cmd_AddCommand("menu_keys", MenuKeys_enter);
	Cmd_AddCommand("menu_quit", MenuQuit_enter);
}

void M_Draw()
{
	if (cls.key_dest != key_menu)
	{
		return;
	}

	/* repaint everything next frame */
	SCR_DirtyScreen();

	/* dim everything behind it down */
	if (cl.cinematictime > 0)
	{
		Draw_Fill(0, 0, viddef.width, viddef.height, 0);
	}
	else
	{
		Draw_FadeScreen();
	}

	m_drawfunc();

	/* delay playing the enter sound until after the
	   menu has been drawn, to avoid delay while
	   caching images */
	if (m_entersound)
	{
		S_StartLocalSound(menu_in_sound);
		m_entersound = false;
	}
}

void M_Keydown(int key)
{
	if (m_keyfunc)
	{
		const char *s = m_keyfunc(key);
		if (s != 0)
		{
			S_StartLocalSound((char *)s);
		}
	}
}
