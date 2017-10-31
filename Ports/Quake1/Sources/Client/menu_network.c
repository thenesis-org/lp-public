#include "Client/menu.h"

static int M_Network_cursor;
static int M_Network_items;

#define SerialConfig (M_Network_cursor == 0)
#define DirectConfig (M_Network_cursor == 1)
#define IPXConfig (M_Network_cursor == 2)
#define TCPIPConfig (M_Network_cursor == 3)

static const char *M_Network_helpMessage[] =
{
	/* .........1.........2.... */
	"                        ",
	" Two computers connected",
	"   through two modems.  ",
	"                        ",

	"                        ",
	" Two computers connected",
	" by a null-modem cable. ",
	"                        ",

	" Novell network LANs    ",
	" or Windows 95 DOS-box. ",
	"                        ",
	"(LAN=Local Area Network)",

	" Commonly used to play  ",
	" over the Internet, but ",
	" also used on a Local   ",
	" Area Network.          "
};

void M_Network_enter()
{
	key_dest = key_menu;
	m_state = m_net;
	m_entersound = true;

	M_Network_items = 4;
	if (M_Network_cursor >= M_Network_items)
		M_Network_cursor = 0;
	M_Network_cursor--;
	M_Network_onKey(K_DOWNARROW);
}

void M_Network_draw()
{
	int f;
	qpic_t *p;

	M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

	f = 32;

	f += 19;
	p = Draw_CachePic("gfx/netmen4.lmp");
	M_DrawTransPic(72, f, p);

	f = (320 - 26 * 8) / 2;
	M_DrawTextBox(f, 134, 24, 4);
	f += 8;
	M_Print(f, 142, M_Network_helpMessage[M_Network_cursor * 4 + 0]);
	M_Print(f, 150, M_Network_helpMessage[M_Network_cursor * 4 + 1]);
	M_Print(f, 158, M_Network_helpMessage[M_Network_cursor * 4 + 2]);
	M_Print(f, 166, M_Network_helpMessage[M_Network_cursor * 4 + 3]);

	f = (int)(host_time * 10) % 6;
	M_DrawTransPic(54, 32 + M_Network_cursor * 20, Draw_CachePic(va("gfx/menudot%i.lmp", f + 1)));
}

void M_Network_onKey(int k)
{
	switch (k)
	{
    case K_GAMEPAD_SELECT:
	case K_ESCAPE:
		M_MultiPlayer_enter();
		break;

	case K_GAMEPAD_DOWN:
	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		if (++M_Network_cursor >= M_Network_items)
			M_Network_cursor = 0;
		break;

	case K_GAMEPAD_UP:
	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		if (--M_Network_cursor < 0)
			M_Network_cursor = M_Network_items - 1;
		break;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_JOY1:
	case K_KP_ENTER:
	case K_ENTER:
		m_entersound = true;
		M_LanConfig_enter();
	}
}
