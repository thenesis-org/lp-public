#include "client/menu/menu.h"

/*
 * LOADGAME MENU
 */

#define MAX_SAVESLOTS 16
#define MAX_SAVEPAGES 2

static char m_savestrings[MAX_SAVESLOTS][32];
static qboolean m_savevalid[MAX_SAVESLOTS];

static int m_loadsave_page;
static char m_loadsave_statusbar[32];

static menuframework_s s_loadgame_menu;
static menuaction_s s_loadgame_actions[MAX_SAVESLOTS];

static menuframework_s s_savegame_menu;
static menuaction_s s_savegame_actions[MAX_SAVESLOTS];

static void Create_Savestrings()
{
	int i;
	fileHandle_t f;
	char name[MAX_OSPATH];

	for (i = 0; i < MAX_SAVESLOTS; i++)
	{
		Com_sprintf(name, sizeof(name), "save/save%i/server.ssv", m_loadsave_page * MAX_SAVESLOTS + i);
		FS_FOpenFile(name, &f, true);

		if (!f)
		{
			strcpy(m_savestrings[i], "<empty>");
			m_savevalid[i] = false;
		}
		else
		{
			FS_Read(m_savestrings[i], sizeof(m_savestrings[i]), f);
			FS_FCloseFile(f);
			m_savevalid[i] = true;
		}
	}
}

static void LoadSave_AdjustPage(int dir)
{
	int i;
	char *str;

	m_loadsave_page += dir;

	if (m_loadsave_page >= MAX_SAVEPAGES)
	{
		m_loadsave_page = 0;
	}
	else
	if (m_loadsave_page < 0)
	{
		m_loadsave_page = MAX_SAVEPAGES - 1;
	}

	strcpy(m_loadsave_statusbar, "pages: ");

	for (i = 0; i < MAX_SAVEPAGES; i++)
	{
		str = va("%c%d%c",
				i == m_loadsave_page ? '[' : ' ',
				i + 1,
				i == m_loadsave_page ? ']' : ' ');

		if (Q_strlen(m_loadsave_statusbar) + Q_strlen(str) >=
		    (int)sizeof(m_loadsave_statusbar))
		{
			break;
		}

		strcat(m_loadsave_statusbar, str);
	}
}

static void LoadGameCallback(void *self)
{
	menuaction_s *a = (menuaction_s *)self;

	Cbuf_AddText(va("load save%i\n", a->generic.localdata[0]));
	M_ForceMenuOff();
}

static void LoadGame_MenuInit(void)
{
	int i;
	float scale = SCR_GetMenuScale();

	menuframework_s *menu = &s_loadgame_menu;
	memset(menu, 0, sizeof(menuframework_s));
	s_loadgame_menu.x = viddef.width / 2 - (120 * scale);
	s_loadgame_menu.y = viddef.height / (2 * scale) - 58;
	s_loadgame_menu.nitems = 0;

	Create_Savestrings();

	for (i = 0; i < MAX_SAVESLOTS; i++)
	{
		s_loadgame_actions[i].generic.type = MTYPE_ACTION;
		s_loadgame_actions[i].generic.name = m_savestrings[i];
		s_loadgame_actions[i].generic.x = 0;
		s_loadgame_actions[i].generic.y = i * 10;
		s_loadgame_actions[i].generic.localdata[0] = i + m_loadsave_page * MAX_SAVESLOTS;
		s_loadgame_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		if (!m_savevalid[i])
		{
			s_loadgame_actions[i].generic.callback = NULL;
		}
		else
		{
			s_loadgame_actions[i].generic.callback = LoadGameCallback;
		}

		Menu_AddItem(&s_loadgame_menu, &s_loadgame_actions[i]);
	}

	Menu_SetStatusBar(&s_loadgame_menu, m_loadsave_statusbar);
}

static void LoadGame_MenuDraw(void)
{
	M_Banner("m_banner_load_game");
	Menu_AdjustCursor(&s_loadgame_menu, 1);
	Menu_Draw(&s_loadgame_menu);
}

static const char* LoadGame_MenuKey(int key)
{
	static menuframework_s *m = &s_loadgame_menu;

	switch (key)
	{
	case K_GAMEPAD_UP:
	case K_UPARROW:
		if (m->cursor == 0)
		{
			LoadSave_AdjustPage(-1);
			LoadGame_MenuInit();
		}
		break;
	case K_GAMEPAD_DOWN:
	case K_TAB:
	case K_DOWNARROW:
		if (m->cursor == m->nitems - 1)
		{
			LoadSave_AdjustPage(1);
			LoadGame_MenuInit();
		}
		break;
	case K_GAMEPAD_LEFT:
	case K_LEFTARROW:
		LoadSave_AdjustPage(-1);
		LoadGame_MenuInit();
		return menu_move_sound;

	case K_GAMEPAD_RIGHT:
	case K_RIGHTARROW:
		LoadSave_AdjustPage(1);
		LoadGame_MenuInit();
		return menu_move_sound;

	default:
		s_savegame_menu.cursor = s_loadgame_menu.cursor;
		break;
	}

	return Default_MenuKey(m, key);
}

void MenuLoadGame_enter()
{
	LoadSave_AdjustPage(0);
	LoadGame_MenuInit();
	M_PushMenu(LoadGame_MenuDraw, LoadGame_MenuKey);
}

/*
 * SAVEGAME MENU
 */

static void SaveGameCallback(void *self)
{
	menuaction_s *a = (menuaction_s *)self;

	if (a->generic.localdata[0] == 0)
	{
		m_popup_string = "This slot is reserved for\n"
		        "autosaving, so please select\n"
		        "another one.";
		m_popup_endtime = cls.realtime + 2000;
		M_Popup();
		return;
	}

	Cbuf_AddText(va("save save%i\n", a->generic.localdata[0]));
	M_ForceMenuOff();
}

static void SaveGame_MenuDraw(void)
{
	M_Banner("m_banner_save_game");
	Menu_AdjustCursor(&s_savegame_menu, 1);
	Menu_Draw(&s_savegame_menu);
	M_Popup();
}

static void SaveGame_MenuInit(void)
{
	int i;
	float scale = SCR_GetMenuScale();

	menuframework_s *menu = &s_savegame_menu;
	memset(menu, 0, sizeof(menuframework_s));
	s_savegame_menu.x = viddef.width / 2 - (120 * scale);
	s_savegame_menu.y = viddef.height / (2 * scale) - 58;
	s_savegame_menu.nitems = 0;

	Create_Savestrings();

	/* don't include the autosave slot */
	for (i = 0; i < MAX_SAVESLOTS; i++)
	{
		s_savegame_actions[i].generic.type = MTYPE_ACTION;
		s_savegame_actions[i].generic.name = m_savestrings[i];
		s_savegame_actions[i].generic.x = 0;
		s_savegame_actions[i].generic.y = i * 10;
		s_savegame_actions[i].generic.localdata[0] = i + m_loadsave_page * MAX_SAVESLOTS;
		s_savegame_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_savegame_actions[i].generic.callback = SaveGameCallback;

		Menu_AddItem(&s_savegame_menu, &s_savegame_actions[i]);
	}

	Menu_SetStatusBar(&s_savegame_menu, m_loadsave_statusbar);
}

static const char* SaveGame_MenuKey(int key)
{
	static menuframework_s *m = &s_savegame_menu;

	if (m_popup_string)
	{
		m_popup_string = NULL;
		return NULL;
	}

	switch (key)
	{
	case K_GAMEPAD_UP:
	case K_UPARROW:
		if (m->cursor == 0)
		{
			LoadSave_AdjustPage(-1);
			SaveGame_MenuInit();
		}
		break;
	case K_GAMEPAD_DOWN:
	case K_TAB:
	case K_DOWNARROW:
		if (m->cursor == m->nitems - 1)
		{
			LoadSave_AdjustPage(1);
			SaveGame_MenuInit();
		}
		break;
	case K_GAMEPAD_LEFT:
	case K_LEFTARROW:
		LoadSave_AdjustPage(-1);
		SaveGame_MenuInit();
		return menu_move_sound;

	case K_GAMEPAD_RIGHT:
	case K_RIGHTARROW:
		LoadSave_AdjustPage(1);
		SaveGame_MenuInit();
		return menu_move_sound;

	default:
		s_loadgame_menu.cursor = s_savegame_menu.cursor;
		break;
	}

	return Default_MenuKey(m, key);
}

void MenuSaveGame_enter()
{
	if (!Com_ServerState())
	{
		return; /* not playing a game */
	}

	LoadSave_AdjustPage(0);
	SaveGame_MenuInit();
	M_PushMenu(SaveGame_MenuDraw, SaveGame_MenuKey);
}
