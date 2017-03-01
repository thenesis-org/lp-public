#include "client/menu/menu.h"

static menuframework_s MenuSound_menu;

//----------------------------------------
// Sound quality.
//----------------------------------------
static menulist_s MenuSound_soundQuality_list;

static void MenuSound_soundQuality_apply()
{
	menulist_s *list = &MenuSound_soundQuality_list;
	if (list->curvalue == 0)
	{
		Cvar_SetValue("s_khz", 22);
		Cvar_SetValue("s_loadas8bit", false);
	}
	else
	{
		Cvar_SetValue("s_khz", 44);
		Cvar_SetValue("s_loadas8bit", false);
	}

	m_popup_string = "Restarting the sound system. This\n"
	        "could take up to a minute, so\n"
	        "please be patient.";
	m_popup_endtime = cls.realtime + 2000;
	M_Popup();

	/* the text box won't show up unless we do a buffer swap */
	Renderer_EndFrame();

	CL_Snd_Restart_f();
}

static void MenuSound_soundQuality_callback(void *unused)
{
	MenuSound_soundQuality_apply();
}

int MenuSound_soundQuality_init(int y)
{
	static const char *listItems[] =
	{
		"normal", "high", 0
	};

	menulist_s *list = &MenuSound_soundQuality_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.name = "sound quality";
	list->generic.callback = MenuSound_soundQuality_callback;
	list->itemnames = listItems;
	list->curvalue = (Cvar_VariableValue("s_loadas8bit") == 0);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuSound_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Sound FX volume.
//----------------------------------------
static menuslider_s MenuSound_sfxVolume_slider;

static void MenuSound_sfxVolume_apply()
{
	menuslider_s *slider = &MenuSound_sfxVolume_slider;
	Cvar_SetValue("s_volume", slider->curvalue / 10);
}

static void MenuSound_sfxVolume_callback(void *unused)
{
	MenuSound_sfxVolume_apply();
}

static int MenuSound_sfxVolume_init(int y)
{
	menuslider_s *slider = &MenuSound_sfxVolume_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = "effects volume";
	slider->generic.callback = MenuSound_sfxVolume_callback;
	slider->minvalue = 0;
	slider->maxvalue = 10;
	slider->curvalue = Cvar_VariableValue("s_volume") * 10;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuSound_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// Music enable.
//----------------------------------------
#if defined(OGG)

static menulist_s MenuSound_musicEnabled_list;

static void MenuSound_musicEnabled_apply()
{
	menulist_s *list = &MenuSound_musicEnabled_list;
	Cvar_SetValue("ogg_enable", (float)list->curvalue);

	if (list->curvalue)
	{
		OGG_Init();
		OGG_Stop();

		if ((int)strtol(cl.configstrings[CS_CDTRACK], (char **)NULL, 10) < 10)
		{
			char tmp[3] = "0";
			OGG_ParseCmd(strcat(tmp, cl.configstrings[CS_CDTRACK]));
		}
		else
		{
			OGG_ParseCmd(cl.configstrings[CS_CDTRACK]);
		}
	}
	else
	{
		OGG_Shutdown();
	}
}

static void MenuSound_musicEnabled_callback(void *unused)
{
	MenuSound_musicEnabled_apply();
}

static int MenuSound_musicEnabled_init(int y)
{
	static const char *listItems[] =
	{
		"disabled",
		"enabled",
		0
	};

	menulist_s *list = &MenuSound_musicEnabled_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.name = "music";
	list->generic.callback = MenuSound_musicEnabled_callback;
	list->itemnames = listItems;
	list->curvalue = (Cvar_VariableValue("ogg_enable") != 0);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuSound_menu, (void *)list);
	y += 10;
	return y;
}

#endif

//----------------------------------------
// Music shuffle.
//----------------------------------------
#if defined(OGG)

static menulist_s MenuSound_musicShuffling_list;

static void MenuSound_musicShuffling_apply()
{
	menulist_s *list = MenuSound_musicShuffling_list;
	Cvar_SetValue("cd_shuffle", list->curvalue);

	cvar_t *ogg;
	ogg = Cvar_Get("ogg_enable", "1", CVAR_ARCHIVE);

	if (list->curvalue)
	{
		Cvar_Set("ogg_sequence", "random");

		if (ogg->value)
		{
			OGG_ParseCmd("?");
			OGG_Stop();
		}
	}
	else
	{
		Cvar_Set("ogg_sequence", "loop");

		if (ogg->value)
		{
			if ((int)strtol(cl.configstrings[CS_CDTRACK], (char **)NULL,
				    10) < 10)
			{
				char tmp[2] = "0";
				OGG_ParseCmd(strcat(tmp, cl.configstrings[CS_CDTRACK]));
			}
			else
			{
				OGG_ParseCmd(cl.configstrings[CS_CDTRACK]);
			}
		}
	}
}

static void MenuSound_musicShuffling_callback(void *unused)
{
	MenuSound_musicShuffling_apply();
}

static int MenuSound_musicShuffling_init(int y)
{
	static const char *listItems[] =
	{
		"disabled",
		"enabled",
		0
	};

	menulist_s *list = MenuSound_musicShuffling_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.name = "music shuffle";
	list->generic.callback = MenuSound_musicShuffling_callback;
	list->itemnames = listItems;

	list->curvalue = (Cvar_VariableValue("cd_shuffle") != 0);
	cvar_t *ogg;
	ogg = Cvar_Get("ogg_sequence", "loop", CVAR_ARCHIVE);
	if (!strcmp(ogg->string, "random"))
	{
		list->curvalue = 1;
	}
	else
	{
		list->curvalue = 0;
	}
	list->savedValue = list->curvalue;

	Menu_AddItem(&MenuSound_menu, (void *)list);
	y += 10;
	return y;
}

#endif

//----------------------------------------
// Music volume.
//----------------------------------------
#if defined(OGG)

static menuslider_s MenuSound_musicVolume_slider;

static void MenuSound_musicVolume_apply()
{
	menuslider_s *slider = MenuSound_musicVolume_slider;
	Cvar_SetValue("ogg_volume", slider->curvalue / 10);
}

static void MenuSound_musicVolume_callback(void *unused)
{
	MenuSound_musicVolume_apply();
}

static int MenuSound_musicVolume_init(int y)
{
	menuslider_s *slider = MenuSound_musicVolume_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = "music volume";
	slider->generic.callback = MenuSound_musicVolume_callback;
	slider->minvalue = 0;
	slider->maxvalue = 10;
	slider->curvalue = Cvar_VariableValue("ogg_volume") * 10;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuSound_menu, (void *)slider);
	y += 10;
	return y;
}

#endif

//----------------------------------------
// MenuSound.
//----------------------------------------
static void MenuSound_init()
{
	int y = 0;

	menuframework_s *menu = &MenuSound_menu;
	memset(menu, 0, sizeof(menuframework_s));
	menu->x = viddef.width / 2;
	menu->nitems = 0;

	y = MenuSound_soundQuality_init(y);
	y = MenuSound_sfxVolume_init(y);
	#if defined(OGG)
	y = MenuSound_musicEnabled_init(y);
	y = MenuSound_musicShuffling_init(y);
	y = MenuSound_musicVolume_init(y);
	#endif

	Menu_CenterWithBanner(&MenuSound_menu, "m_banner_options");
}

static void MenuSound_draw()
{
	M_Banner("m_banner_options");
	Menu_AdjustCursor(&MenuSound_menu, 1);
	Menu_Draw(&MenuSound_menu);
	M_Popup();
}

static const char* MenuSound_key(int key)
{
	if (m_popup_string)
	{
		m_popup_string = NULL;
		return NULL;
	}
	return Default_MenuKey(&MenuSound_menu, key);
}

void MenuSound_enter()
{
	MenuSound_init();
	M_PushMenu(MenuSound_draw, MenuSound_key);
}
