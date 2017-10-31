#include "Client/menu.h"
#include "Common/quakedef.h"
#include "Networking/net.h"

static bool m_searchComplete = false;
static double m_searchCompleteTime;

void M_Search_enter()
{
	key_dest = key_menu;
	m_state = m_search;
	m_entersound = false;
	slistSilent = true;
	slistLocal = false;
	m_searchComplete = false;
	NET_Slist_f();
}

void M_Search_draw()
{
	qpic_t *p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);
	int x = (320 / 2) - ((12 * 8) / 2) + 4;
	M_DrawTextBox(x - 8, 32, 12, 1);
	M_Print(x, 40, "Searching...");

	if (slistInProgress)
	{
		NET_Poll();
		return;
	}

	if (!m_searchComplete)
	{
		m_searchComplete = true;
		m_searchCompleteTime = realtime;
	}

	if (hostCacheCount)
	{
		M_ServerList_enter();
		return;
	}

	M_PrintWhite((320 / 2) - ((22 * 8) / 2), 64, "No Quake servers found");
	if ((realtime - m_searchCompleteTime) < 3.0)
		return;

	M_LanConfig_enter();
}

void M_Search_onKey(int key)
{
}
