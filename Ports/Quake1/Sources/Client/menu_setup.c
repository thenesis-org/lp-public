#include "Client/client.h"
#include "Client/menu.h"
#include "Common/cmd.h"
#include "Common/quakedef.h"
#include "Networking/net.h"

#include <string.h>

#define NUM_SETUP_CMDS 5
static int M_Setup_cursor = 4;
static const int M_Setup_cursorTable[] = { 40, 56, 80, 104, 140 };

static char M_Setup_hostName[16];
static char M_Setup_myName[16];
static int M_Setup_oldTop;
static int M_Setup_oldBottom;
static int M_Setup_top;
static int M_Setup_bottom;

void M_Setup_enter()
{
	key_dest = key_menu;
	m_state = m_setup;
	m_entersound = true;
    
	Q_strncpy(M_Setup_myName, cl_name.string, 16);
	Q_strncpy(M_Setup_hostName, hostname.string, 16);
	M_Setup_top = M_Setup_oldTop = ((int)cl_color.value) >> 4;
	M_Setup_bottom = M_Setup_oldBottom = ((int)cl_color.value) & 15;
}

void M_Setup_draw()
{
	qpic_t *p;

	M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

	M_Print(64, 40, "Hostname");
	M_DrawTextBox(160, 32, 16, 1);
	M_Print(168, 40, M_Setup_hostName);

	M_Print(64, 56, "Your name");
	M_DrawTextBox(160, 48, 16, 1);
	M_Print(168, 56, M_Setup_myName);

	M_Print(64, 80, "Shirt color");
	M_Print(64, 104, "Pants color");

	M_DrawTextBox(64, 140 - 8, 14, 1);
	M_Print(72, 140, "Accept Changes");

	p = Draw_CachePic("gfx/bigbox.lmp");
	M_DrawTransPic(160, 64, p);

	p = Draw_CachePic("gfx/menuplyr.lmp");
    byte translationTable[256];
	M_BuildTranslationTable(M_Setup_top * 16, M_Setup_bottom * 16, translationTable);
	M_DrawTransPicTranslate(172, 72, p, translationTable);

	M_DrawCharacter(56, M_Setup_cursorTable[M_Setup_cursor], 12 + ((int)(realtime * 4) & 1));

	if (M_Setup_cursor == 0)
		M_DrawCharacter(168 + 8 * strlen(M_Setup_hostName), M_Setup_cursorTable[M_Setup_cursor], 10 + ((int)(realtime * 4) & 1));

	if (M_Setup_cursor == 1)
		M_DrawCharacter(168 + 8 * strlen(M_Setup_myName), M_Setup_cursorTable[M_Setup_cursor], 10 + ((int)(realtime * 4) & 1));
}

void M_Setup_onKey(int k)
{
	int l;

	switch (k)
	{
    case K_GAMEPAD_SELECT:
	case K_ESCAPE:
		M_MultiPlayer_enter();
		break;

	case K_GAMEPAD_UP:
	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		M_Setup_cursor--;
		if (M_Setup_cursor < 0)
			M_Setup_cursor = NUM_SETUP_CMDS - 1;
		break;

	case K_GAMEPAD_DOWN:
	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		M_Setup_cursor++;
		if (M_Setup_cursor >= NUM_SETUP_CMDS)
			M_Setup_cursor = 0;
		break;

	case K_GAMEPAD_LEFT:
	case K_LEFTARROW:
		if (M_Setup_cursor < 2)
			return;

		S_LocalSound("misc/menu3.wav");
		if (M_Setup_cursor == 2)
			M_Setup_top = M_Setup_top - 1;
		if (M_Setup_cursor == 3)
			M_Setup_bottom = M_Setup_bottom - 1;
		break;
	case K_GAMEPAD_RIGHT:
	case K_RIGHTARROW:
		if (M_Setup_cursor < 2)
			return;

forward:
		S_LocalSound("misc/menu3.wav");
		if (M_Setup_cursor == 2)
			M_Setup_top = M_Setup_top + 1;
		if (M_Setup_cursor == 3)
			M_Setup_bottom = M_Setup_bottom + 1;
		break;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_JOY1:
	case K_KP_ENTER:
	case K_ENTER:
		if (M_Setup_cursor == 0 || M_Setup_cursor == 1)
			return;

		if (M_Setup_cursor == 2 || M_Setup_cursor == 3)
			goto forward;

		// M_Setup_cursor == 4 (OK)
		if (Q_strcmp(cl_name.string, M_Setup_myName) != 0)
			Cbuf_AddText(va("name \"%s\"\n", M_Setup_myName));
		if (Q_strcmp(hostname.string, M_Setup_hostName) != 0)
			Cvar_Set("hostname", M_Setup_hostName);
		if (M_Setup_top != M_Setup_oldTop || M_Setup_bottom != M_Setup_oldBottom)
			Cbuf_AddText(va("color %i %i\n", M_Setup_top, M_Setup_bottom));
		m_entersound = true;
		M_MultiPlayer_enter();
		break;

	case K_GAMEPAD_Y:
	case K_JOY3:
	case K_BACKSPACE:
	case K_DEL:
		if (M_Setup_cursor == 0)
		{
			if (strlen(M_Setup_hostName))
				M_Setup_hostName[strlen(M_Setup_hostName) - 1] = 0;
		}

		if (M_Setup_cursor == 1)
		{
			if (strlen(M_Setup_myName))
				M_Setup_myName[strlen(M_Setup_myName) - 1] = 0;
		}
		break;

	default:
		if (k < 32 || k > 127)
			break;
		if (M_Setup_cursor == 0)
		{
			l = strlen(M_Setup_hostName);
			if (l < 15)
			{
				M_Setup_hostName[l + 1] = 0;
				M_Setup_hostName[l] = k;
			}
		}
		if (M_Setup_cursor == 1)
		{
			l = strlen(M_Setup_myName);
			if (l < 15)
			{
				M_Setup_myName[l + 1] = 0;
				M_Setup_myName[l] = k;
			}
		}
	}

	if (M_Setup_top > 13)
		M_Setup_top = 0;
	if (M_Setup_top < 0)
		M_Setup_top = 13;
	if (M_Setup_bottom > 13)
		M_Setup_bottom = 0;
	if (M_Setup_bottom < 0)
		M_Setup_bottom = 13;
}
