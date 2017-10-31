#include "Client/menu.h"

bool m_joiningGameFlag = false;
bool m_startingGameFlag = false;

#define MULTIPLAYER_ITEMS 3
static int m_multiplayer_cursor;

void M_MultiPlayer_enter()
{
	key_dest = key_menu;
	m_state = m_multiplayer;
	m_entersound = true;
    
    m_joiningGameFlag = false;
    m_startingGameFlag = false;
}

void M_MultiPlayer_draw()
{
	M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	qpic_t *p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);
	M_DrawTransPic(72, 32, Draw_CachePic("gfx/mp_menu.lmp"));
	int f = (int)(host_time * 10) % 6;
	M_DrawTransPic(54, 32 + m_multiplayer_cursor * 20, Draw_CachePic(va("gfx/menudot%i.lmp", f + 1)));
}

void M_MultiPlayer_onKey(int key)
{
	switch (key)
	{
    case K_GAMEPAD_SELECT:
	case K_ESCAPE:
		M_Main_enter();
		break;

	case K_GAMEPAD_DOWN:
	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
			m_multiplayer_cursor = 0;
		break;

	case K_GAMEPAD_UP:
	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		if (--m_multiplayer_cursor < 0)
			m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
		break;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_JOY1:
	case K_KP_ENTER:
	case K_ENTER:
		m_entersound = true;
		switch (m_multiplayer_cursor)
		{
		case 0:
            m_joiningGameFlag = true;
            M_LanConfig_enter();
			break;
		case 1:
            m_startingGameFlag = true;
            M_LanConfig_enter();
			break;
		case 2:
			M_Setup_enter();
			break;
		}
	}
}
