#include "client/menu/menu.h"

#define MAIN_ITEMS 5

static int m_main_cursor;

static void M_Main_Draw()
{
	int i;
	int w, h;
	int ystart;
	int xoffset;
	int widest = -1;
	int totalheight = 0;
	char litname[80];
	float scale = SCR_GetMenuScale();
	char *names[] =
	{
		"m_main_game",
		"m_main_multiplayer",
		"m_main_options",
		"m_main_video",
		"m_main_quit",
		0
	};

	for (i = 0; names[i] != 0; i++)
	{
		Draw_GetPicSize(&w, &h, names[i]);

		if (w > widest)
		{
			widest = w;
		}

		totalheight += (h + 12);
	}

	ystart = (viddef.height / (2 * scale) - 110);
	xoffset = (viddef.width / scale - widest + 70) / 2;

	for (i = 0; names[i] != 0; i++)
	{
		if (i != m_main_cursor)
		{
			Draw_PicScaled(xoffset * scale, (ystart + i * 40 + 13) * scale, names[i], scale);
		}
	}

	strcpy(litname, names[m_main_cursor]);
	strcat(litname, "_sel");
	Draw_PicScaled(xoffset * scale, (ystart + m_main_cursor * 40 + 13) * scale, litname, scale);

	M_DrawCursor(xoffset - 25, ystart + m_main_cursor * 40 + 11,
		(int)(cls.realtime / 100) % NUM_CURSOR_FRAMES);

	Draw_GetPicSize(&w, &h, "m_main_plaque");
	Draw_PicScaled((xoffset - 30 - w) * scale, ystart * scale, "m_main_plaque", scale);

	Draw_PicScaled((xoffset - 30 - w) * scale, (ystart + h + 5) * scale, "m_main_logo", scale);
}

static const char* M_Main_Key(int key)
{
	const char *sound = menu_move_sound;

	switch (key)
	{
	case K_GAMEPAD_SELECT:
	case K_GAMEPAD_B:
	case K_JOY2:
	case K_ESCAPE:
		M_PopMenu();
		break;
	case K_GAMEPAD_DOWN:
	case K_DOWNARROW:
		if (++m_main_cursor >= MAIN_ITEMS)
		{
			m_main_cursor = 0;
		}
		return sound;

	case K_GAMEPAD_UP:
	case K_UPARROW:
		if (--m_main_cursor < 0)
		{
			m_main_cursor = MAIN_ITEMS - 1;
		}
		return sound;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_JOY1:
	case K_KP_ENTER:
	case K_ENTER:
		m_entersound = true;
		switch (m_main_cursor)
		{
		case 0:
			MenuGame_enter();
			break;
		case 1:
			MenuMultiplayer_enter();
			break;
		case 2:
			MenuOptions_enter();
			break;
		case 3:
			MenuVideo_enter();
			break;
		case 4:
			MenuQuit_enter();
			break;
		}
	}

	return NULL;
}

void MenuMain_enter()
{
	M_PushMenu(M_Main_Draw, M_Main_Key);
}
