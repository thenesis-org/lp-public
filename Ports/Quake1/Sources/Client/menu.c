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
#include "Client/menu.h"
#include "Client/screen.h"
#include "Client/view.h"
#include "Common/cmd.h"
#include "Common/common.h"
#include "Networking/net.h"
#include "Networking/protocol.h"
#include "Rendering/r_draw.h"
#include "Rendering/r_public.h"
#include "Rendering/r_video.h"
#include "Server/server.h"
#include "Sound/sound.h"

#include <stdlib.h>
#include <string.h>

M_State m_state;
bool m_entersound; // play after drawing a frame, so caching
bool m_recursiveDraw; // won't disrupt the sound

int m_save_demonum;

int m_return_state;
bool m_return_onerror;
char m_return_reason[32];

/*
   Draws one solid graphics character
 */
void M_DrawCharacter(int cx, int line, int num)
{
	Draw_Character(cx + ((vid.width - 320) >> 1), line, num);
}

void M_Print(int cx, int cy, const char *str)
{
    Draw_CharBegin();
	while (*str)
	{
		M_DrawCharacter(cx, cy, (*str) + 128);
		str++;
		cx += 8;
	}
    Draw_CharEnd();
}

void M_PrintWhite(int cx, int cy, char *str)
{
    Draw_CharBegin();
	while (*str)
	{
		M_DrawCharacter(cx, cy, *str);
		str++;
		cx += 8;
	}
    Draw_CharEnd();
}

void M_DrawPic(int x, int y, qpic_t *pic)
{
	Draw_Pic(x + ((vid.width - 320) >> 1), y, pic);
}

void M_DrawTransPic(int x, int y, qpic_t *pic)
{
	Draw_TransPic(x + ((vid.width - 320) >> 1), y, pic);
}

void M_BuildTranslationTable(int top, int bottom, byte *translationTable)
{
	int j;

    byte identityTable[256];
	for (j = 0; j < 256; j++)
		identityTable[j] = j;
	byte *dest = translationTable;
	byte *source = identityTable;
	memcpy(dest, source, 256);

	if (top < 128) // the artists made some backwards ranges.  sigh.
		memcpy(dest + TOP_RANGE, source + top, 16);
	else
		for (j = 0; j < 16; j++)
			dest[TOP_RANGE + j] = source[top + 15 - j];
	if (bottom < 128)
		memcpy(dest + BOTTOM_RANGE, source + bottom, 16);
	else
		for (j = 0; j < 16; j++)
			dest[BOTTOM_RANGE + j] = source[bottom + 15 - j];
}

void M_DrawTransPicTranslate(int x, int y, qpic_t *pic, byte *translationTable)
{
	Draw_TransPicTranslate(x + ((vid.width - 320) >> 1), y, pic, translationTable);
}

void M_DrawTextBox(int x, int y, int width, int lines)
{
	qpic_t *p;
	int cx, cy;
	int n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic("gfx/box_tl.lmp");
	M_DrawTransPic(cx, cy, p);
	p = Draw_CachePic("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic(cx, cy, p);
	}
	p = Draw_CachePic("gfx/box_bl.lmp");
	M_DrawTransPic(cx, cy + 8, p);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = Draw_CachePic("gfx/box_tm.lmp");
		M_DrawTransPic(cx, cy, p);
		p = Draw_CachePic("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			if (n == 1)
				p = Draw_CachePic("gfx/box_mm2.lmp");
			M_DrawTransPic(cx, cy, p);
		}
		p = Draw_CachePic("gfx/box_bm.lmp");
		M_DrawTransPic(cx, cy + 8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic("gfx/box_tr.lmp");
	M_DrawTransPic(cx, cy, p);
	p = Draw_CachePic("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic(cx, cy, p);
	}
	p = Draw_CachePic("gfx/box_br.lmp");
	M_DrawTransPic(cx, cy + 8, p);
}

//=============================================================================

void M_ToggleMenu_f()
{
	m_entersound = true;

	if (key_dest == key_menu)
	{
		if (m_state != m_main)
		{
			M_Main_enter();
			return;
		}
		key_dest = key_game;
		m_state = m_none;
		return;
	}
	if (key_dest == key_console)
		Con_ToggleConsole_f();
	else
		M_Main_enter();
}

//=============================================================================
/* Menu Subsystem */

void M_Init()
{
	Cmd_AddCommand("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand("menu_main", M_Main_enter);
	Cmd_AddCommand("menu_singleplayer", M_SinglePlayer_enter);
	Cmd_AddCommand("menu_load", M_Load_enter);
	Cmd_AddCommand("menu_save", M_Save_enter);
	Cmd_AddCommand("menu_multiplayer", M_MultiPlayer_enter);
	Cmd_AddCommand("menu_setup", M_Setup_enter);
	Cmd_AddCommand("menu_options", M_Options_enter);
	Cmd_AddCommand("menu_keys", M_Keys_enter);
	Cmd_AddCommand("help", M_Help_enter);
	Cmd_AddCommand("menu_quit", M_Quit_enter);
}

void M_Draw()
{
	if (m_state == m_none || key_dest != key_menu)
		return;

	if (!m_recursiveDraw)
	{
		if (scr_con_current)
		{
			Draw_ConsoleBackground(vid.height);
			S_ExtraUpdate();
		}
		else
			Draw_FadeScreen();
	}
	else
		m_recursiveDraw = false;

	switch (m_state)
	{
	case m_none:
		break;
	case m_main:
		M_Main_draw();
		break;
	case m_singleplayer:
		M_SinglePlayer_draw();
		break;
	case m_load:
		M_Load_draw();
		break;
	case m_save:
		M_Save_draw();
		break;
	case m_multiplayer:
		M_MultiPlayer_draw();
		break;
	case m_setup:
		M_Setup_draw();
		break;
    /*
	case m_net:
		M_Network_draw();
		break;
    */
	case m_options:
		M_Options_draw();
		break;
	case m_keys:
		M_Keys_draw();
		break;
	case m_help:
		M_Help_draw();
		break;
	case m_quit:
		M_Quit_draw();
		break;
	case m_lanconfig:
		M_LanConfig_draw();
		break;
	case m_gameoptions:
		M_GameOptions_draw();
		break;
	case m_search:
		M_Search_draw();
		break;
	case m_slist:
		M_ServerList_draw();
		break;
	}

	if (m_entersound)
	{
		S_LocalSound("misc/menu2.wav");
		m_entersound = false;
	}

	S_ExtraUpdate();
}

void M_Keydown(int key, int keyUnmodified)
{
	switch (m_state)
	{
	case m_none:
		return;
	case m_main:
		M_Main_onKey(key);
		return;
	case m_singleplayer:
		M_SinglePlayer_onKey(key);
		return;
	case m_load:
		M_Load_onKey(key);
		return;
	case m_save:
		M_Save_onKey(key);
		return;
	case m_multiplayer:
		M_MultiPlayer_onKey(key);
		return;
	case m_setup:
		M_Setup_onKey(key);
		return;
    /*
	case m_net:
		M_Network_onKey(key);
		return;
    */
	case m_options:
		M_Options_onKey(key);
		return;
	case m_keys:
		M_Keys_onKey(key, keyUnmodified);
		return;
	case m_help:
		M_Help_onKey(key);
		return;
	case m_quit:
		M_Quit_onKey(key);
		return;
	case m_lanconfig:
		M_LanConfig_onKey(key);
		return;
	case m_gameoptions:
		M_GameOptions_onKey(key);
		return;
	case m_search:
		M_Search_onKey(key);
		break;
	case m_slist:
		M_ServerList_onKey(key);
		return;
	}
}
