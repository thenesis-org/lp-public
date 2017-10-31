#include "Client/menu.h"
#include "Common/quakedef.h"

#include <stdlib.h>

static int M_Quit_messageIndex;
static int M_Quit_previousState;
static bool M_Quit_wasInMenus;

#ifndef _WIN32
static const char *M_Quit_messages[] =
{
	/* .........1.........2.... */
	"  Are you gonna quit    ",
	"  this game just like   ",
	"   everything else?     ",
	"                        ",

	" Milord, methinks that  ",
	"   thou art a lowly     ",
	" quitter. Is this true? ",
	"                        ",

	" Do I need to bust your ",
	"  face open for trying  ",
	"        to quit?        ",
	"                        ",

	" Man, I oughta smack you",
	"   for trying to quit!  ",
	"     Press Y to get     ",
	"      smacked out.      ",

	" Press Y to quit like a ",
	"   big loser in life.   ",
	"  Press N to stay proud ",
	"    and successful!     ",

	"   If you press Y to    ",
	"  quit, I will summon   ",
	"  Satan all over your   ",
	"      hard drive!       ",

	"  Um, Asmodeus dislikes ",
	" his children trying to ",
	" quit. Press Y to return",
	"   to your Tinkertoys.  ",

	"  If you quit now, I'll ",
	"  throw a blanket-party ",
	"   for you next time!   ",
	"                        "
};
#endif

void M_Quit_enter()
{
	if (m_state == m_quit)
		return;

	M_Quit_wasInMenus = (key_dest == key_menu);
	M_Quit_previousState = m_state;
	M_Quit_messageIndex = rand() & 7;

	key_dest = key_menu;
	m_state = m_quit;
	m_entersound = true;
}

void M_Quit_onKey(int key)
{
	switch (key)
	{
	case K_GAMEPAD_SELECT:
	case K_GAMEPAD_B:
	case K_ESCAPE:
	case 'n':
	case 'N':
		if (M_Quit_wasInMenus)
		{
			m_state = M_Quit_previousState;
			m_entersound = true;
		}
		else
		{
			key_dest = key_game;
			m_state = m_none;
		}
		break;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_KP_ENTER:
	case K_ENTER:
	case 'Y':
	case 'y':
		key_dest = key_console;
		Host_Quit_f();
		break;

	default:
		break;
	}
}

void M_Quit_draw()
{
	if (M_Quit_wasInMenus)
	{
		m_state = M_Quit_previousState;
		m_recursiveDraw = true;
		M_Draw();
		m_state = m_quit;
	}

	#ifdef _WIN32
	M_DrawTextBox(0, 0, 38, 23);
	M_PrintWhite(16, 12, "  Quake version 1.09 by id Software\n\n");
	M_PrintWhite(16, 28, "Programming        Art \n");
	M_Print(16, 36, " John Carmack       Adrian Carmack\n");
	M_Print(16, 44, " Michael Abrash     Kevin Cloud\n");
	M_Print(16, 52, " John Cash          Paul Steed\n");
	M_Print(16, 60, " Dave 'Zoid' Kirsch\n");
	M_PrintWhite(16, 68, "Design             Biz\n");
	M_Print(16, 76, " John Romero        Jay Wilbur\n");
	M_Print(16, 84, " Sandy Petersen     Mike Wilson\n");
	M_Print(16, 92, " American McGee     Donna Jackson\n");
	M_Print(16, 100, " Tim Willits        Todd Hollenshead\n");
	M_PrintWhite(16, 108, "Support            Projects\n");
	M_Print(16, 116, " Barrett Alexander  Shawn Green\n");
	M_PrintWhite(16, 124, "Sound Effects\n");
	M_Print(16, 132, " Trent Reznor and Nine Inch Nails\n\n");
	M_PrintWhite(16, 140, "Quake is a trademark of Id Software,\n");
	M_PrintWhite(16, 148, "inc., (c)1996 Id Software, inc. All\n");
	M_PrintWhite(16, 156, "rights reserved. NIN logo is a\n");
	M_PrintWhite(16, 164, "registered trademark licensed to\n");
	M_PrintWhite(16, 172, "Nothing Interactive, Inc. All rights\n");
	M_PrintWhite(16, 180, "reserved. Press y to exit\n");
	#else
	M_DrawTextBox(56, 76, 24, 4);
	M_Print(64, 84, M_Quit_messages[M_Quit_messageIndex * 4 + 0]);
	M_Print(64, 92, M_Quit_messages[M_Quit_messageIndex * 4 + 1]);
	M_Print(64, 100, M_Quit_messages[M_Quit_messageIndex * 4 + 2]);
	M_Print(64, 108, M_Quit_messages[M_Quit_messageIndex * 4 + 3]);
	#endif
}
