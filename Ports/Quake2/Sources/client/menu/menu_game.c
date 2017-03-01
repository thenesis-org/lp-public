#include "client/menu/menu.h"

static int m_game_cursor;

static menuframework_s s_game_menu;
static menuaction_s s_easy_game_action;
static menuaction_s s_medium_game_action;
static menuaction_s s_hard_game_action;
static menuaction_s s_hardp_game_action;
static menuaction_s s_load_game_action;
static menuaction_s s_save_game_action;
static menuaction_s s_credits_action;
static menuseparator_s s_blankline;

static void StartGame(void)
{
	/* disable updates and start the cinematic going */
	cl.servercount = -1;
	M_ForceMenuOff();
	Cvar_SetValue("deathmatch", 0);
	Cvar_SetValue("coop", 0);

	Cbuf_AddText("loading ; killserver ; wait ; newgame\n");
	cls.key_dest = key_game;
}

static void EasyGameFunc(void *data)
{
	Cvar_ForceSet("skill", "0");
	StartGame();
}

static void MediumGameFunc(void *data)
{
	Cvar_ForceSet("skill", "1");
	StartGame();
}

static void HardGameFunc(void *data)
{
	Cvar_ForceSet("skill", "2");
	StartGame();
}

static void HardpGameFunc(void *data)
{
	Cvar_ForceSet("skill", "3");
	StartGame();
}

static void LoadGameFunc(void *unused)
{
	MenuLoadGame_enter();
}

static void SaveGameFunc(void *unused)
{
	MenuSaveGame_enter();
}

static void CreditsFunc(void *unused)
{
	MenuCredits_enter();
}

void Game_MenuInit(void)
{
	menuframework_s *menu = &s_game_menu;
	memset(menu, 0, sizeof(menuframework_s));
	s_game_menu.x = (int)(viddef.width * 0.50f);
	s_game_menu.nitems = 0;

	s_easy_game_action.generic.type = MTYPE_ACTION;
	s_easy_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_easy_game_action.generic.x = 0;
	s_easy_game_action.generic.y = 0;
	s_easy_game_action.generic.name = "easy";
	s_easy_game_action.generic.callback = EasyGameFunc;

	s_medium_game_action.generic.type = MTYPE_ACTION;
	s_medium_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_medium_game_action.generic.x = 0;
	s_medium_game_action.generic.y = 10;
	s_medium_game_action.generic.name = "medium";
	s_medium_game_action.generic.callback = MediumGameFunc;

	s_hard_game_action.generic.type = MTYPE_ACTION;
	s_hard_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_hard_game_action.generic.x = 0;
	s_hard_game_action.generic.y = 20;
	s_hard_game_action.generic.name = "hard";
	s_hard_game_action.generic.callback = HardGameFunc;

	s_hardp_game_action.generic.type = MTYPE_ACTION;
	s_hardp_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_hardp_game_action.generic.x = 0;
	s_hardp_game_action.generic.y = 30;
	s_hardp_game_action.generic.name = "nightmare";
	s_hardp_game_action.generic.callback = HardpGameFunc;

	s_blankline.generic.type = MTYPE_SEPARATOR;

	s_load_game_action.generic.type = MTYPE_ACTION;
	s_load_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_load_game_action.generic.x = 0;
	s_load_game_action.generic.y = 50;
	s_load_game_action.generic.name = "load game";
	s_load_game_action.generic.callback = LoadGameFunc;

	s_save_game_action.generic.type = MTYPE_ACTION;
	s_save_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_save_game_action.generic.x = 0;
	s_save_game_action.generic.y = 60;
	s_save_game_action.generic.name = "save game";
	s_save_game_action.generic.callback = SaveGameFunc;

	s_credits_action.generic.type = MTYPE_ACTION;
	s_credits_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_credits_action.generic.x = 0;
	s_credits_action.generic.y = 70;
	s_credits_action.generic.name = "credits";
	s_credits_action.generic.callback = CreditsFunc;

	Menu_AddItem(&s_game_menu, (void *)&s_easy_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_medium_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_hard_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_hardp_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_blankline);
	Menu_AddItem(&s_game_menu, (void *)&s_load_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_save_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_blankline);
	Menu_AddItem(&s_game_menu, (void *)&s_credits_action);

	Menu_Center(&s_game_menu);
}

static void Game_MenuDraw()
{
	M_Banner("m_banner_game");
	Menu_AdjustCursor(&s_game_menu, 1);
	Menu_Draw(&s_game_menu);
}

static const char* Game_MenuKey(int key)
{
	return Default_MenuKey(&s_game_menu, key);
}

void MenuGame_enter()
{
	Game_MenuInit();
	M_PushMenu(Game_MenuDraw, Game_MenuKey);
	m_game_cursor = 1;
}
