#include "client/menu/menu.h"

static menuframework_s s_downloadoptions_menu;

static menuseparator_s s_download_title;
static menulist_s s_allow_download_box;
static menulist_s s_allow_download_maps_box;
static menulist_s s_allow_download_models_box;
static menulist_s s_allow_download_players_box;
static menulist_s s_allow_download_sounds_box;

static void DownloadCallback(void *self)
{
	menulist_s *f = (menulist_s *)self;

	if (f == &s_allow_download_box)
	{
		Cvar_SetValue("allow_download", (float)f->curvalue);
	}
	else
	if (f == &s_allow_download_maps_box)
	{
		Cvar_SetValue("allow_download_maps", (float)f->curvalue);
	}
	else
	if (f == &s_allow_download_models_box)
	{
		Cvar_SetValue("allow_download_models", (float)f->curvalue);
	}
	else
	if (f == &s_allow_download_players_box)
	{
		Cvar_SetValue("allow_download_players", (float)f->curvalue);
	}
	else
	if (f == &s_allow_download_sounds_box)
	{
		Cvar_SetValue("allow_download_sounds", (float)f->curvalue);
	}
}

static void DownloadOptions_MenuInit()
{
	static const char *yes_no_names[] =
	{
		"no", "yes", 0
	};
	int y = 0;
	float scale = SCR_GetMenuScale();

	menuframework_s *menu = &s_downloadoptions_menu;
	memset(menu, 0, sizeof(menuframework_s));
	s_downloadoptions_menu.x = (int)(viddef.width * 0.50f);
	s_downloadoptions_menu.nitems = 0;

	s_download_title.generic.type = MTYPE_SEPARATOR;
	s_download_title.generic.name = "Download Options";
	s_download_title.generic.x = 48 * scale;
	s_download_title.generic.y = y;

	s_allow_download_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_box.generic.x = 0;
	s_allow_download_box.generic.y = y += 20;
	s_allow_download_box.generic.name = "allow downloading";
	s_allow_download_box.generic.callback = DownloadCallback;
	s_allow_download_box.itemnames = yes_no_names;
	s_allow_download_box.curvalue = (Cvar_VariableValue("allow_download") != 0);

	s_allow_download_maps_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_maps_box.generic.x = 0;
	s_allow_download_maps_box.generic.y = y += 20;
	s_allow_download_maps_box.generic.name = "maps";
	s_allow_download_maps_box.generic.callback = DownloadCallback;
	s_allow_download_maps_box.itemnames = yes_no_names;
	s_allow_download_maps_box.curvalue =
	        (Cvar_VariableValue("allow_download_maps") != 0);

	s_allow_download_players_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_players_box.generic.x = 0;
	s_allow_download_players_box.generic.y = y += 10;
	s_allow_download_players_box.generic.name = "player models/skins";
	s_allow_download_players_box.generic.callback = DownloadCallback;
	s_allow_download_players_box.itemnames = yes_no_names;
	s_allow_download_players_box.curvalue =
	        (Cvar_VariableValue("allow_download_players") != 0);

	s_allow_download_models_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_models_box.generic.x = 0;
	s_allow_download_models_box.generic.y = y += 10;
	s_allow_download_models_box.generic.name = "models";
	s_allow_download_models_box.generic.callback = DownloadCallback;
	s_allow_download_models_box.itemnames = yes_no_names;
	s_allow_download_models_box.curvalue =
	        (Cvar_VariableValue("allow_download_models") != 0);

	s_allow_download_sounds_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_sounds_box.generic.x = 0;
	s_allow_download_sounds_box.generic.y = y += 10;
	s_allow_download_sounds_box.generic.name = "sounds";
	s_allow_download_sounds_box.generic.callback = DownloadCallback;
	s_allow_download_sounds_box.itemnames = yes_no_names;
	s_allow_download_sounds_box.curvalue =
	        (Cvar_VariableValue("allow_download_sounds") != 0);

	Menu_AddItem(&s_downloadoptions_menu, &s_download_title);
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_box);
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_maps_box);
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_players_box);
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_models_box);
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_sounds_box);

	Menu_Center(&s_downloadoptions_menu);

	/* skip over title */
	if (s_downloadoptions_menu.cursor == 0)
	{
		s_downloadoptions_menu.cursor = 1;
	}
}

static void DownloadOptions_MenuDraw()
{
	Menu_Draw(&s_downloadoptions_menu);
}

static const char* DownloadOptions_MenuKey(int key)
{
	return Default_MenuKey(&s_downloadoptions_menu, key);
}

void MenuDownloadOptions_enter()
{
	DownloadOptions_MenuInit();
	M_PushMenu(DownloadOptions_MenuDraw, DownloadOptions_MenuKey);
}
