#include "Client/menu.h"

#define M_Help_PageNb 6
static int M_Help_pageIndex;

void M_Help_enter()
{
	key_dest = key_menu;
	m_state = m_help;
	m_entersound = true;
    
	M_Help_pageIndex = 0;
}

void M_Help_draw()
{
	M_DrawPic(0, 0, Draw_CachePic(va("gfx/help%i.lmp", M_Help_pageIndex)));
}

void M_Help_onKey(int key)
{
	switch (key)
	{
    case K_GAMEPAD_SELECT:
	case K_ESCAPE:
		M_Main_enter();
		break;

	case K_GAMEPAD_UP:
	case K_UPARROW:
	case K_GAMEPAD_RIGHT:
	case K_RIGHTARROW:
		m_entersound = true;
		if (++M_Help_pageIndex >= M_Help_PageNb)
			M_Help_pageIndex = 0;
		break;

	case K_GAMEPAD_DOWN:
	case K_DOWNARROW:
	case K_GAMEPAD_LEFT:
	case K_LEFTARROW:
		m_entersound = true;
		if (--M_Help_pageIndex < 0)
			M_Help_pageIndex = M_Help_PageNb - 1;
		break;
	}
}
