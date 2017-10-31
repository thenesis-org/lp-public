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

#ifndef menu_h
#define menu_h

#include "Client/keys.h"
#include "Common/wad.h"
#include "Rendering/r_draw.h"
#include "Sound/sound.h"

extern double host_time;

void M_Init();
void M_Keydown(int key, int keyUnmodified);
void M_Draw();
void M_ToggleMenu_f();

void M_DrawPic(int x, int y, qpic_t *pic);
void M_DrawTransPic(int x, int y, qpic_t *pic);
void M_BuildTranslationTable(int top, int bottom, byte *translationTable);
void M_DrawTransPicTranslate(int x, int y, qpic_t *pic, byte *ranslationTable);
void M_Print(int cx, int cy, const char *str);
void M_PrintWhite(int cx, int cy, char *str);
void M_DrawCharacter(int cx, int line, int num);
void M_DrawTextBox(int x, int y, int width, int lines);

typedef enum
{
    m_none, m_main, m_singleplayer, m_load, m_save, m_multiplayer, m_setup, m_net, m_options, m_video, m_keys, m_help, m_quit, m_lanconfig, m_gameoptions, m_search, m_slist
} M_State;

extern M_State m_state;
extern bool m_return_onerror;
extern int m_return_state;
extern char m_return_reason[32];

extern int m_save_demonum;
extern bool m_entersound;
extern bool m_recursiveDraw;
extern bool m_joiningGameFlag;
extern bool m_startingGameFlag;

void M_Main_enter();
void M_Main_draw();
void M_Main_onKey(int key);

void M_SinglePlayer_enter();
void M_SinglePlayer_draw();
void M_SinglePlayer_onKey(int key);

void M_Load_enter();
void M_Load_draw();
void M_Load_onKey(int key);

void M_Save_enter();
void M_Save_draw();
void M_Save_onKey(int key);

void M_MultiPlayer_enter();
void M_MultiPlayer_draw();
void M_MultiPlayer_onKey(int key);

void M_Setup_enter();
void M_Setup_draw();
void M_Setup_onKey(int key);

void M_Options_enter();
void M_Options_draw();
void M_Options_onKey(int key);

void M_Keys_enter();
void M_Keys_draw();
void M_Keys_onKey(int key, int keyUnmodified);

void M_Help_enter();
void M_Help_draw();
void M_Help_onKey(int key);

void M_Quit_enter();
void M_Quit_draw();
void M_Quit_onKey(int key);

void M_LanConfig_enter();
void M_LanConfig_draw();
void M_LanConfig_onKey(int key);

void M_Menu_GameOptions_f();
void M_GameOptions_draw();
void M_GameOptions_onKey(int key);

void M_Search_enter();
void M_Search_draw();
void M_Search_onKey(int key);

void M_ServerList_enter();
void M_ServerList_draw();
void M_ServerList_onKey(int key);

void M_Network_draw();
void M_Network_onKey(int key);

#endif
