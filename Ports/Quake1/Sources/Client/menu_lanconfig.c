#include "Client/menu.h"
#include "Networking/net.h"
#include "Common/cmd.h"
#include "Common/quakedef.h"

#include <string.h>

#define NUM_LANCONFIG_CMDS 3
static int M_LanConfig_cursor = -1;
static const char M_LanConfig_cursorTable[] = { 72, 92, 124 };

static int M_LanConfig_port;
static char M_LanConfig_portName[6];
static char M_LanConfig_joinName[22];

void M_LanConfig_enter()
{
	key_dest = key_menu;
	m_state = m_lanconfig;
	m_entersound = true;
	if (M_LanConfig_cursor == -1)
	{
		if (m_joiningGameFlag)
			M_LanConfig_cursor = 2;
		else
			M_LanConfig_cursor = 1;
	}
	if (m_startingGameFlag && M_LanConfig_cursor == 2)
		M_LanConfig_cursor = 1;
	M_LanConfig_port = DEFAULTnet_hostport;
	sprintf(M_LanConfig_portName, "%u", M_LanConfig_port);

	m_return_onerror = false;
	m_return_reason[0] = 0;
}

void M_LanConfig_draw()
{
	qpic_t *p;
	int basex;
	char *startJoin;
	char *protocol;

	M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/p_multi.lmp");
	basex = (320 - p->width) / 2;
	M_DrawPic(basex, 4, p);

	if (m_startingGameFlag)
		startJoin = "New Game";
	else
		startJoin = "Join Game";
	protocol = "TCP/IP";
	M_Print(basex, 32, va("%s - %s", startJoin, protocol));
	basex += 8;

	M_Print(basex, 52, "Address:");
	M_Print(basex + 9 * 8, 52, my_tcpip_address);

	M_Print(basex, M_LanConfig_cursorTable[0], "Port");
	M_DrawTextBox(basex + 8 * 8, M_LanConfig_cursorTable[0] - 8, 6, 1);
	M_Print(basex + 9 * 8, M_LanConfig_cursorTable[0], M_LanConfig_portName);

	if (m_joiningGameFlag)
	{
		M_Print(basex, M_LanConfig_cursorTable[1], "Search for local games...");
		M_Print(basex, 108, "Join game at:");
		M_DrawTextBox(basex + 8, M_LanConfig_cursorTable[2] - 8, 22, 1);
		M_Print(basex + 16, M_LanConfig_cursorTable[2], M_LanConfig_joinName);
	}
	else
	{
		M_DrawTextBox(basex, M_LanConfig_cursorTable[1] - 8, 2, 1);
		M_Print(basex + 8, M_LanConfig_cursorTable[1], "OK");
	}

	M_DrawCharacter(basex - 8, M_LanConfig_cursorTable[M_LanConfig_cursor], 12 + ((int)(realtime * 4) & 1));

	if (M_LanConfig_cursor == 0)
		M_DrawCharacter(basex + 9 * 8 + 8 * strlen(M_LanConfig_portName), M_LanConfig_cursorTable[0], 10 + ((int)(realtime * 4) & 1));

	if (M_LanConfig_cursor == 2)
		M_DrawCharacter(basex + 16 + 8 * strlen(M_LanConfig_joinName), M_LanConfig_cursorTable[2], 10 + ((int)(realtime * 4) & 1));

	if (*m_return_reason)
		M_PrintWhite(basex, 148, m_return_reason);
}

static void M_LanConfig_configureNetSubsystem()
{
	// enable/disable net systems to match desired config

	Cbuf_AddText("stopdemo\n");
    net_hostport = M_LanConfig_port;
}

void M_LanConfig_onKey(int key)
{
	int l;

	switch (key)
	{
    case K_GAMEPAD_SELECT:
	case K_ESCAPE:
        M_MultiPlayer_enter();
		//M_Network_enter();
		break;

	case K_GAMEPAD_UP:
	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		M_LanConfig_cursor--;
		if (M_LanConfig_cursor < 0)
			M_LanConfig_cursor = NUM_LANCONFIG_CMDS - 1;
		break;

	case K_GAMEPAD_DOWN:
	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		M_LanConfig_cursor++;
		if (M_LanConfig_cursor >= NUM_LANCONFIG_CMDS)
			M_LanConfig_cursor = 0;
		break;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_JOY1:
	case K_KP_ENTER:
	case K_ENTER:
		if (M_LanConfig_cursor == 0)
			break;

		m_entersound = true;

		M_LanConfig_configureNetSubsystem();

		if (M_LanConfig_cursor == 1)
		{
			if (m_startingGameFlag)
			{
				M_Menu_GameOptions_f();
				break;
			}
			M_Search_enter();
			break;
		}

		if (M_LanConfig_cursor == 2)
		{
			m_return_state = m_state;
			m_return_onerror = true;
			key_dest = key_game;
			m_state = m_none;
			Cbuf_AddText(va("connect \"%s\"\n", M_LanConfig_joinName));
			break;
		}

		break;

	case K_GAMEPAD_Y:
	case K_JOY3:
	case K_BACKSPACE:
	case K_DEL:
		if (M_LanConfig_cursor == 0)
		{
			if (strlen(M_LanConfig_portName))
				M_LanConfig_portName[strlen(M_LanConfig_portName) - 1] = 0;
		}

		if (M_LanConfig_cursor == 2)
		{
			if (strlen(M_LanConfig_joinName))
				M_LanConfig_joinName[strlen(M_LanConfig_joinName) - 1] = 0;
		}
		break;

	default:
		if (key < 32 || key > 127)
			break;

		if (M_LanConfig_cursor == 2)
		{
			l = strlen(M_LanConfig_joinName);
			if (l < 21)
			{
				M_LanConfig_joinName[l + 1] = 0;
				M_LanConfig_joinName[l] = key;
			}
		}

		if (key < '0' || key > '9')
			break;
		if (M_LanConfig_cursor == 0)
		{
			l = strlen(M_LanConfig_portName);
			if (l < 5)
			{
				M_LanConfig_portName[l + 1] = 0;
				M_LanConfig_portName[l] = key;
			}
		}
	}

	if (m_startingGameFlag && M_LanConfig_cursor == 2)
	{
		if (key == K_UPARROW || key == K_GAMEPAD_UP)
			M_LanConfig_cursor = 1;
		else
			M_LanConfig_cursor = 0;
	}

	l = Q_atoi(M_LanConfig_portName);
	if (l > 65535)
		l = M_LanConfig_port;
	else
		M_LanConfig_port = l;
	sprintf(M_LanConfig_portName, "%u", M_LanConfig_port);
}
