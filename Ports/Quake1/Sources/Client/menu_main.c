#include "Client/client.h"
#include "Client/menu.h"

static int m_main_cursor;
#define MAIN_ITEMS 5

void M_Main_enter()
{
	if (key_dest != key_menu)
	{
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}
	key_dest = key_menu;
	m_state = m_main;
	m_entersound = true;
}

void M_Main_draw()
{
	M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	qpic_t *p = Draw_CachePic("gfx/ttl_main.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);
	M_DrawTransPic(72, 32, Draw_CachePic("gfx/mainmenu.lmp"));
	int f = (int)(host_time * 10) % 6;
	M_DrawTransPic(54, 32 + m_main_cursor * 20, Draw_CachePic(va("gfx/menudot%i.lmp", f + 1)));
}

void M_Main_onKey(int key)
{
	switch (key)
	{
    case K_GAMEPAD_SELECT:
	case K_ESCAPE:
		key_dest = key_game;
		m_state = m_none;
		cls.demonum = m_save_demonum;
		if (cls.demonum != -1 && !cls.demoplayback && cls.state != ca_connected)
			CL_NextDemo();
		break;

	case K_GAMEPAD_DOWN:
	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_GAMEPAD_UP:
	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_JOY1:
	case K_KP_ENTER:
	case K_ENTER:
		m_entersound = true;

		switch (m_main_cursor)
		{
		case 0:
			M_SinglePlayer_enter();
			break;
		case 1:
			M_MultiPlayer_enter();
			break;
		case 2:
			M_Options_enter();
			break;
		case 3:
			M_Help_enter();
			break;
		case 4:
			M_Quit_enter();
			break;
		}
	}
}
