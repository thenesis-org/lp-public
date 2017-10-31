#include "Client/menu.h"
#include "Client/screen.h"
#include "Common/cmd.h"
#include "Server/server.h"

static int m_singleplayer_cursor;
#define SINGLEPLAYER_ITEMS 3

void M_SinglePlayer_enter()
{
	key_dest = key_menu;
	m_state = m_singleplayer;
	m_entersound = true;
}

void M_SinglePlayer_draw()
{
	M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	qpic_t *p = Draw_CachePic("gfx/ttl_sgl.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);
	M_DrawTransPic(72, 32, Draw_CachePic("gfx/sp_menu.lmp"));
	int f = (int)(host_time * 10) % 6;
	M_DrawTransPic(54, 32 + m_singleplayer_cursor * 20, Draw_CachePic(va("gfx/menudot%i.lmp", f + 1)));
}

void M_SinglePlayer_onKey(int key)
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
		if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
			m_singleplayer_cursor = 0;
		break;

	case K_GAMEPAD_UP:
	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		if (--m_singleplayer_cursor < 0)
			m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_JOY1:
	case K_KP_ENTER:
	case K_ENTER:
		m_entersound = true;

		switch (m_singleplayer_cursor)
		{
		case 0:
			if (sv.active)
				if (!SCR_ModalMessage("Are you sure you want to\nstart a new game?\n"))
					break;

			key_dest = key_game;
			if (sv.active)
				Cbuf_AddText("disconnect\n");
			Cbuf_AddText("maxplayers 1\n");
			Cbuf_AddText("map start\n");
			break;

		case 1:
			M_Load_enter();
			break;

		case 2:
			M_Save_enter();
			break;
		}
	}
}
