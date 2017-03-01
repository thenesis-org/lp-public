#include "client/menu/menu.h"

menuframework_s s_multiplayer_menu;
static menuaction_s s_join_network_server_action;
static menuaction_s s_start_network_server_action;
static menuaction_s s_player_setup_action;

static void PlayerSetupFunc(void *unused)
{
	MenuPlayerConfig_enter();
}

static void JoinNetworkServerFunc(void *unused)
{
	MenuJoinServer_enter();
}

static void StartNetworkServerFunc(void *unused)
{
	MenuStartServer_enter();
}

static void Multiplayer_MenuInit()
{
	float scale = SCR_GetMenuScale();

	int y = 0;

	menuframework_s *menu = &s_multiplayer_menu;
	memset(menu, 0, sizeof(menuframework_s));
	s_multiplayer_menu.x = (int)(viddef.width * 0.50f) - 64 * scale;
	s_multiplayer_menu.nitems = 0;

	s_join_network_server_action.generic.type = MTYPE_ACTION;
	s_join_network_server_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_join_network_server_action.generic.x = 0;
	s_join_network_server_action.generic.y = y;
	s_join_network_server_action.generic.name = " join network server";
	s_join_network_server_action.generic.callback = JoinNetworkServerFunc;
	Menu_AddItem(&s_multiplayer_menu, (void *)&s_join_network_server_action);
	y += 10;

	s_start_network_server_action.generic.type = MTYPE_ACTION;
	s_start_network_server_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_start_network_server_action.generic.x = 0;
	s_start_network_server_action.generic.y = y;
	s_start_network_server_action.generic.name = " start network server";
	s_start_network_server_action.generic.callback = StartNetworkServerFunc;
	Menu_AddItem(&s_multiplayer_menu, (void *)&s_start_network_server_action);
	y += 10;

	s_player_setup_action.generic.type = MTYPE_ACTION;
	s_player_setup_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_setup_action.generic.x = 0;
	s_player_setup_action.generic.y = y;
	s_player_setup_action.generic.name = " player setup";
	s_player_setup_action.generic.callback = PlayerSetupFunc;
	Menu_AddItem(&s_multiplayer_menu, (void *)&s_player_setup_action);
	y += 10;

	Menu_SetStatusBar(&s_multiplayer_menu, NULL);
	Menu_Center(&s_multiplayer_menu);
}

static void Multiplayer_MenuDraw()
{
	M_Banner("m_banner_multiplayer");

	Menu_AdjustCursor(&s_multiplayer_menu, 1);
	Menu_Draw(&s_multiplayer_menu);
}

static const char* Multiplayer_MenuKey(int key)
{
	return Default_MenuKey(&s_multiplayer_menu, key);
}

void MenuMultiplayer_enter()
{
	Multiplayer_MenuInit();
	M_PushMenu(Multiplayer_MenuDraw, Multiplayer_MenuKey);
}
