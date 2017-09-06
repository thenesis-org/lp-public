#include "client/menu/menu.h"

extern int Developer_searchpath();

static menuframework_s s_startserver_menu;
static char **mapnames = NULL;
static int nummaps = 0;

static menuaction_s s_startserver_start_action;
static menuaction_s s_startserver_dmoptions_action;
static menufield_s s_timelimit_field;
static menufield_s s_fraglimit_field;
static menufield_s s_maxclients_field;
static menufield_s s_hostname_field;
static menulist_s s_startmap_list;
static menulist_s s_rules_box;

static void DMOptionsFunc(void *self)
{
	MenuDMOptions_enter();
}

static void RulesChangeFunc(void *self)
{
	/* Deathmatch */
	if (s_rules_box.curvalue == 0)
	{
		s_maxclients_field.generic.statusbar = NULL;
		s_startserver_dmoptions_action.generic.statusbar = NULL;
	}
	/* Ground Zero game modes */
	else
	if (Developer_searchpath() == 2)
	{
		if (s_rules_box.curvalue == 2)
		{
			s_maxclients_field.generic.statusbar = NULL;
			s_startserver_dmoptions_action.generic.statusbar = NULL;
		}
	}
}

static void StartServerActionFunc(void *self)
{
	char startmap[1024];
	float timelimit;
	float fraglimit;
	float maxclients;
	char *spot;

	strcpy(startmap, strchr(mapnames[s_startmap_list.curvalue], '\n') + 1);

	maxclients = (float)strtod(s_maxclients_field.buffer, (char **)NULL);
	timelimit = (float)strtod(s_timelimit_field.buffer, (char **)NULL);
	fraglimit = (float)strtod(s_fraglimit_field.buffer, (char **)NULL);

	Cvar_SetValue("maxclients", ClampCvar(0, maxclients, maxclients));
	Cvar_SetValue("timelimit", ClampCvar(0, timelimit, timelimit));
	Cvar_SetValue("fraglimit", ClampCvar(0, fraglimit, fraglimit));
	Cvar_Set("hostname", s_hostname_field.buffer);

	if ((s_rules_box.curvalue < 2) || (Developer_searchpath() != 2))
	{
		Cvar_SetValue("deathmatch", (float)!s_rules_box.curvalue);
		Cvar_SetValue("coop", (float)s_rules_box.curvalue);
	}
	else
	{
		Cvar_SetValue("deathmatch", 1); /* deathmatch is always true for rogue games */
		Cvar_SetValue("coop", 0); /* This works for at least the main game and both addons */
	}

	spot = NULL;

	if (s_rules_box.curvalue == 1)
	{
		if (Q_stricmp(startmap, "bunk1") == 0)
		{
			spot = "start";
		}
		else
		if (Q_stricmp(startmap, "mintro") == 0)
		{
			spot = "start";
		}
		else
		if (Q_stricmp(startmap, "fact1") == 0)
		{
			spot = "start";
		}
		else
		if (Q_stricmp(startmap, "power1") == 0)
		{
			spot = "pstart";
		}
		else
		if (Q_stricmp(startmap, "biggun") == 0)
		{
			spot = "bstart";
		}
		else
		if (Q_stricmp(startmap, "hangar1") == 0)
		{
			spot = "unitstart";
		}
		else
		if (Q_stricmp(startmap, "city1") == 0)
		{
			spot = "unitstart";
		}
		else
		if (Q_stricmp(startmap, "boss1") == 0)
		{
			spot = "bosstart";
		}
	}

	if (spot)
	{
		if (Com_ServerState())
		{
			Cbuf_AddText("disconnect\n");
		}

		Cbuf_AddText(va("gamemap \"*%s$%s\"\n", startmap, spot));
	}
	else
	{
		Cbuf_AddText(va("map %s\n", startmap));
	}

	M_ForceMenuOff();
}

static void StartServer_MenuInit()
{
	static const char *dm_coop_names[] =
	{
		"deathmatch",
		"cooperative",
		0
	};
	static const char *dm_coop_names_rogue[] =
	{
		"deathmatch",
		"cooperative",
		"tag",
		0
	};
	char *buffer;
	char mapsname[1024];
	char *s;
	int length;
	int i;
	FILE *fp;
	float scale = SCR_GetMenuScale();

	/* initialize list of maps once, reuse it afterwards (=> it isn't freed) */
	if (mapnames == NULL)
	{
		/* load the list of map names */
		Com_sprintf(mapsname, sizeof(mapsname), "%s/maps.lst", FS_WritableGamedir());

		if ((fp = fopen(mapsname, "rb")) == 0)
		{
			if ((length = FS_LoadFile("maps.lst", (void **)&buffer)) == -1)
			{
				Com_Error(ERR_DROP, "couldn't find maps.lst\n");
			}
		}
		else
		{
			fseek(fp, 0, SEEK_END);
			length = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			buffer = malloc(length);
			fread(buffer, length, 1, fp);
		}

		s = buffer;

		i = 0;

		while (i < length)
		{
			if (s[i] == '\r')
			{
				nummaps++;
			}

			i++;
		}

		if (nummaps == 0)
		{
			Com_Error(ERR_DROP, "no maps in maps.lst\n");
		}

		mapnames = malloc(sizeof(char *) * (nummaps + 1));
		memset(mapnames, 0, sizeof(char *) * (nummaps + 1));

		s = buffer;

		for (i = 0; i < nummaps; i++)
		{
			char shortname[MAX_TOKEN_CHARS];
			char longname[MAX_TOKEN_CHARS];
			char scratch[200];
			int j, l;

			strcpy(shortname, COM_Parse(&s));
			l = Q_strlen(shortname);

			for (j = 0; j < l; j++)
			{
				shortname[j] = toupper(shortname[j]);
			}

			strcpy(longname, COM_Parse(&s));
			Com_sprintf(scratch, sizeof(scratch), "%s\n%s", longname, shortname);

			mapnames[i] = malloc(Q_strlen(scratch) + 1);
			strcpy(mapnames[i], scratch);
		}

		mapnames[nummaps] = 0;

		if (fp != 0)
		{
			fclose(fp);
			fp = 0;
			free(buffer);
		}
		else
		{
			FS_FreeFile(buffer);
		}
	}

	/* initialize the menu stuff */
	menuframework_s *menu = &s_startserver_menu;
	memset(menu, 0, sizeof(menuframework_s));
	s_startserver_menu.x = (int)(viddef.width * 0.50f);
	s_startserver_menu.nitems = 0;

	s_startmap_list.generic.type = MTYPE_SPINCONTROL;
	s_startmap_list.generic.x = 0;
	s_startmap_list.generic.y = 0;
	s_startmap_list.generic.name = "initial map";
	s_startmap_list.itemnames = (const char **)mapnames;

	s_rules_box.generic.type = MTYPE_SPINCONTROL;
	s_rules_box.generic.x = 0;
	s_rules_box.generic.y = 20;
	s_rules_box.generic.name = "rules";

	/* Ground Zero games only available with rogue game */
	if (Developer_searchpath() == 2)
	{
		s_rules_box.itemnames = dm_coop_names_rogue;
	}
	else
	{
		s_rules_box.itemnames = dm_coop_names;
	}

	if (Cvar_VariableValue("coop"))
	{
		s_rules_box.curvalue = 1;
	}
	else
	{
		s_rules_box.curvalue = 0;
	}

	s_rules_box.generic.callback = RulesChangeFunc;

	s_timelimit_field.generic.type = MTYPE_FIELD;
	s_timelimit_field.generic.name = "time limit";
	s_timelimit_field.generic.flags = QMF_NUMBERSONLY;
	s_timelimit_field.generic.x = 0;
	s_timelimit_field.generic.y = 36;
	s_timelimit_field.generic.statusbar = "0 = no limit";
	s_timelimit_field.length = 3;
	s_timelimit_field.visible_length = 3;
	strcpy(s_timelimit_field.buffer, Cvar_VariableString("timelimit"));

	s_fraglimit_field.generic.type = MTYPE_FIELD;
	s_fraglimit_field.generic.name = "frag limit";
	s_fraglimit_field.generic.flags = QMF_NUMBERSONLY;
	s_fraglimit_field.generic.x = 0;
	s_fraglimit_field.generic.y = 54;
	s_fraglimit_field.generic.statusbar = "0 = no limit";
	s_fraglimit_field.length = 3;
	s_fraglimit_field.visible_length = 3;
	strcpy(s_fraglimit_field.buffer, Cvar_VariableString("fraglimit"));

	/* maxclients determines the maximum number of players that can join
	   the game. If maxclients is only "1" then we should default the menu
	   option to 8 players, otherwise use whatever its current value is.
	   Clamping will be done when the server is actually started. */
	s_maxclients_field.generic.type = MTYPE_FIELD;
	s_maxclients_field.generic.name = "max players";
	s_maxclients_field.generic.flags = QMF_NUMBERSONLY;
	s_maxclients_field.generic.x = 0;
	s_maxclients_field.generic.y = 72;
	s_maxclients_field.generic.statusbar = NULL;
	s_maxclients_field.length = 3;
	s_maxclients_field.visible_length = 3;

	if (Cvar_VariableValue("maxclients") == 1)
	{
		strcpy(s_maxclients_field.buffer, "8");
	}
	else
	{
		strcpy(s_maxclients_field.buffer, Cvar_VariableString("maxclients"));
	}

	s_hostname_field.generic.type = MTYPE_FIELD;
	s_hostname_field.generic.name = "hostname";
	s_hostname_field.generic.flags = 0;
	s_hostname_field.generic.x = 0;
	s_hostname_field.generic.y = 90;
	s_hostname_field.generic.statusbar = NULL;
	s_hostname_field.length = 12;
	s_hostname_field.visible_length = 12;
	strcpy(s_hostname_field.buffer, Cvar_VariableString("hostname"));

	s_startserver_dmoptions_action.generic.type = MTYPE_ACTION;
	s_startserver_dmoptions_action.generic.name = " deathmatch flags";
	s_startserver_dmoptions_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_startserver_dmoptions_action.generic.x = 24 * scale;
	s_startserver_dmoptions_action.generic.y = 108;
	s_startserver_dmoptions_action.generic.statusbar = NULL;
	s_startserver_dmoptions_action.generic.callback = DMOptionsFunc;

	s_startserver_start_action.generic.type = MTYPE_ACTION;
	s_startserver_start_action.generic.name = " begin";
	s_startserver_start_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_startserver_start_action.generic.x = 24 * scale;
	s_startserver_start_action.generic.y = 128;
	s_startserver_start_action.generic.callback = StartServerActionFunc;

	Menu_AddItem(&s_startserver_menu, &s_startmap_list);
	Menu_AddItem(&s_startserver_menu, &s_rules_box);
	Menu_AddItem(&s_startserver_menu, &s_timelimit_field);
	Menu_AddItem(&s_startserver_menu, &s_fraglimit_field);
	Menu_AddItem(&s_startserver_menu, &s_maxclients_field);
	Menu_AddItem(&s_startserver_menu, &s_hostname_field);
	Menu_AddItem(&s_startserver_menu, &s_startserver_dmoptions_action);
	Menu_AddItem(&s_startserver_menu, &s_startserver_start_action);

	Menu_Center(&s_startserver_menu);

	/* call this now to set proper inital state */
	RulesChangeFunc(NULL);
}

static void StartServer_MenuDraw()
{
	Menu_Draw(&s_startserver_menu);
}

static const char* StartServer_MenuKey(int key, int keyUnmodified)
{
	return Default_MenuKey(&s_startserver_menu, key, keyUnmodified);
}

void MenuStartServer_enter()
{
	StartServer_MenuInit();
	M_PushMenu(StartServer_MenuDraw, StartServer_MenuKey);
}
