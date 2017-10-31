#include "Client/menu.h"
#include "Common/cmd.h"
#include "Common/quakedef.h"
#include "Networking/net.h"

#include <string.h>

static int M_ServerList_cursor;
static bool M_ServerList_sorted;

void M_ServerList_enter()
{
	key_dest = key_menu;
	m_state = m_slist;
	m_entersound = true;
    
	m_return_onerror = false;
	m_return_reason[0] = 0;

	M_ServerList_cursor = 0;
	M_ServerList_sorted = false;
}

void M_ServerList_draw()
{
	if (!M_ServerList_sorted)
	{
		if (hostCacheCount > 1)
		{
			for (int i = 0; i < hostCacheCount; i++)
				for (int j = i + 1; j < hostCacheCount; j++)
					if (strcmp(hostcache[j].name, hostcache[i].name) < 0)
					{
                        hostcache_t temp;
						Q_memcpy(&temp, &hostcache[j], sizeof(hostcache_t));
						Q_memcpy(&hostcache[j], &hostcache[i], sizeof(hostcache_t));
						Q_memcpy(&hostcache[i], &temp, sizeof(hostcache_t));
					}
		}
		M_ServerList_sorted = true;
	}

	qpic_t *p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);
	for (int n = 0; n < hostCacheCount; n++)
	{
        char string[64];
		if (hostcache[n].maxusers)
			snprintf(string, 64, "%-15.15s %-15.15s %2u/%2u\n", hostcache[n].name, hostcache[n].map, hostcache[n].users, hostcache[n].maxusers);
		else
			snprintf(string, 64, "%-15.15s %-15.15s\n", hostcache[n].name, hostcache[n].map);
        string[63] = 0;
		M_Print(16, 32 + 8 * n, string);
	}
	M_DrawCharacter(0, 32 + M_ServerList_cursor * 8, 12 + ((int)(realtime * 4) & 1));

	if (*m_return_reason)
		M_PrintWhite(16, 148, m_return_reason);
}

void M_ServerList_onKey(int k)
{
	switch (k)
	{
    case K_GAMEPAD_SELECT:
	case K_ESCAPE:
		M_LanConfig_enter();
		break;

	case K_SPACE:
		M_Search_enter();
		break;

    case K_GAMEPAD_UP:
	case K_UPARROW:
    case K_GAMEPAD_LEFT:
	case K_LEFTARROW:
		S_LocalSound("misc/menu1.wav");
		M_ServerList_cursor--;
		if (M_ServerList_cursor < 0)
			M_ServerList_cursor = hostCacheCount - 1;
		break;

    case K_GAMEPAD_DOWN:
	case K_DOWNARROW:
    case K_GAMEPAD_RIGHT:
	case K_RIGHTARROW:
		S_LocalSound("misc/menu1.wav");
		M_ServerList_cursor++;
		if (M_ServerList_cursor >= hostCacheCount)
			M_ServerList_cursor = 0;
		break;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_JOY1:
	case K_KP_ENTER:
	case K_ENTER:
		S_LocalSound("misc/menu2.wav");
		m_return_state = m_state;
		m_return_onerror = true;
		M_ServerList_sorted = false;
		key_dest = key_game;
		m_state = m_none;
		Cbuf_AddText(va("connect \"%s\"\n", hostcache[M_ServerList_cursor].cname));
		break;

	default:
		break;
	}
}
