#include "Client/menu.h"
#include "Client/screen.h"
#include "Common/cmd.h"
#include "Server/server.h"

#define NUM_GAMEOPTIONS 9
static int gameoptions_cursor;
static const int gameoptions_cursor_table[] = { 40, 56, 64, 72, 80, 88, 96, 112, 120 };

typedef struct
{
	char *name;
	char *description;
} level_t;

static const level_t levels[] =
{
	{ "start", "Entrance" }, // 0

	{ "e1m1", "Slipgate Complex" }, // 1
	{ "e1m2", "Castle of the Damned" },
	{ "e1m3", "The Necropolis" },
	{ "e1m4", "The Grisly Grotto" },
	{ "e1m5", "Gloom Keep" },
	{ "e1m6", "The Door To Chthon" },
	{ "e1m7", "The House of Chthon" },
	{ "e1m8", "Ziggurat Vertigo" },

	{ "e2m1", "The Installation" }, // 9
	{ "e2m2", "Ogre Citadel" },
	{ "e2m3", "Crypt of Decay" },
	{ "e2m4", "The Ebon Fortress" },
	{ "e2m5", "The Wizard's Manse" },
	{ "e2m6", "The Dismal Oubliette" },
	{ "e2m7", "Underearth" },

	{ "e3m1", "Termination Central" }, // 16
	{ "e3m2", "The Vaults of Zin" },
	{ "e3m3", "The Tomb of Terror" },
	{ "e3m4", "Satan's Dark Delight" },
	{ "e3m5", "Wind Tunnels" },
	{ "e3m6", "Chambers of Torment" },
	{ "e3m7", "The Haunted Halls" },

	{ "e4m1", "The Sewage System" }, // 23
	{ "e4m2", "The Tower of Despair" },
	{ "e4m3", "The Elder God Shrine" },
	{ "e4m4", "The Palace of Hate" },
	{ "e4m5", "Hell's Atrium" },
	{ "e4m6", "The Pain Maze" },
	{ "e4m7", "Azure Agony" },
	{ "e4m8", "The Nameless City" },

	{ "end", "Shub-Niggurath's Pit" }, // 31

	{ "dm1", "Place of Two Deaths" }, // 32
	{ "dm2", "Claustrophobopolis" },
	{ "dm3", "The Abandoned Base" },
	{ "dm4", "The Bad Place" },
	{ "dm5", "The Cistern" },
	{ "dm6", "The Dark Zone" }
};

//MED 01/06/97 added hipnotic levels
static const level_t hipnoticlevels[] =
{
	{ "start", "Command HQ" }, // 0

	{ "hip1m1", "The Pumping Station" }, // 1
	{ "hip1m2", "Storage Facility" },
	{ "hip1m3", "The Lost Mine" },
	{ "hip1m4", "Research Facility" },
	{ "hip1m5", "Military Complex" },

	{ "hip2m1", "Ancient Realms" }, // 6
	{ "hip2m2", "The Black Cathedral" },
	{ "hip2m3", "The Catacombs" },
	{ "hip2m4", "The Crypt" },
	{ "hip2m5", "Mortum's Keep" },
	{ "hip2m6", "The Gremlin's Domain" },

	{ "hip3m1", "Tur Torment" }, // 12
	{ "hip3m2", "Pandemonium" },
	{ "hip3m3", "Limbo" },
	{ "hip3m4", "The Gauntlet" },

	{ "hipend", "Armagon's Lair" }, // 16

	{ "hipdm1", "The Edge of Oblivion" } // 17
};

//PGM 01/07/97 added rogue levels
//PGM 03/02/97 added dmatch level
static const level_t roguelevels[] =
{
	{ "start", "Split Decision" },
	{ "r1m1", "Deviant's Domain" },
	{ "r1m2", "Dread Portal" },
	{ "r1m3", "Judgement Call" },
	{ "r1m4", "Cave of Death" },
	{ "r1m5", "Towers of Wrath" },
	{ "r1m6", "Temple of Pain" },
	{ "r1m7", "Tomb of the Overlord" },
	{ "r2m1", "Tempus Fugit" },
	{ "r2m2", "Elemental Fury I" },
	{ "r2m3", "Elemental Fury II" },
	{ "r2m4", "Curse of Osiris" },
	{ "r2m5", "Wizard's Keep" },
	{ "r2m6", "Blood Sacrifice" },
	{ "r2m7", "Last Bastion" },
	{ "r2m8", "Source of Evil" },
	{ "ctf1", "Division of Change" }
};

typedef struct
{
	char *description;
	int firstLevel;
	int levels;
} episode_t;

static const episode_t episodes[] =
{
	{ "Welcome to Quake", 0, 1 },
	{ "Doomed Dimension", 1, 8 },
	{ "Realm of Black Magic", 9, 7 },
	{ "Netherworld", 16, 7 },
	{ "The Elder World", 23, 8 },
	{ "Final Level", 31, 1 },
	{ "Deathmatch Arena", 32, 6 }
};

//MED 01/06/97  added hipnotic episodes
static const episode_t hipnoticepisodes[] =
{
	{ "Scourge of Armagon", 0, 1 },
	{ "Fortress of the Dead", 1, 5 },
	{ "Dominion of Darkness", 6, 6 },
	{ "The Rift", 12, 4 },
	{ "Final Level", 16, 1 },
	{ "Deathmatch Arena", 17, 1 }
};

//PGM 01/07/97 added rogue episodes
//PGM 03/02/97 added dmatch episode
static const episode_t rogueepisodes[] =
{
	{ "Introduction", 0, 1 },
	{ "Hell's Fortress", 1, 7 },
	{ "Corridors of Time", 8, 8 },
	{ "Deathmatch Arena", 16, 1 }
};

static int startepisode;
static int startlevel;
static int maxplayers;
static bool m_serverInfoMessage = false;
static double m_serverInfoMessageTime;

void M_Menu_GameOptions_f()
{
	key_dest = key_menu;
	m_state = m_gameoptions;
	m_entersound = true;
	if (maxplayers == 0)
		maxplayers = svs.maxclients;
	if (maxplayers < 2)
		maxplayers = svs.maxclientslimit;
}

void M_GameOptions_draw()
{
	qpic_t *p;
	int x;

	M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

	M_DrawTextBox(152, 32, 10, 1);
	M_Print(160, 40, "begin game");

	M_Print(0, 56, "      Max players");
	M_Print(160, 56, va("%i", maxplayers));

	M_Print(0, 64, "        Game Type");
	if (coop.value)
		M_Print(160, 64, "Cooperative");
	else
		M_Print(160, 64, "Deathmatch");

	M_Print(0, 72, "        Teamplay");
	if (rogue)
	{
		char *msg;

		switch ((int)teamplay.value)
		{
		case 1: msg = "No Friendly Fire"; break;
		case 2: msg = "Friendly Fire"; break;
		case 3: msg = "Tag"; break;
		case 4: msg = "Capture the Flag"; break;
		case 5: msg = "One Flag CTF"; break;
		case 6: msg = "Three Team CTF"; break;
		default: msg = "Off"; break;
		}
		M_Print(160, 72, msg);
	}
	else
	{
		char *msg;

		switch ((int)teamplay.value)
		{
		case 1: msg = "No Friendly Fire"; break;
		case 2: msg = "Friendly Fire"; break;
		default: msg = "Off"; break;
		}
		M_Print(160, 72, msg);
	}

	M_Print(0, 80, "            Skill");
	if (skill.value == 0)
		M_Print(160, 80, "Easy difficulty");
	else
	if (skill.value == 1)
		M_Print(160, 80, "Normal difficulty");
	else
	if (skill.value == 2)
		M_Print(160, 80, "Hard difficulty");
	else
		M_Print(160, 80, "Nightmare difficulty");

	M_Print(0, 88, "       Frag Limit");
	if (fraglimit.value == 0)
		M_Print(160, 88, "none");
	else
		M_Print(160, 88, va("%i frags", (int)fraglimit.value));

	M_Print(0, 96, "       Time Limit");
	if (timelimit.value == 0)
		M_Print(160, 96, "none");
	else
		M_Print(160, 96, va("%i minutes", (int)timelimit.value));

	M_Print(0, 112, "         Episode");
	//MED 01/06/97 added hipnotic episodes
	if (hipnotic)
		M_Print(160, 112, hipnoticepisodes[startepisode].description);
	//PGM 01/07/97 added rogue episodes
	else
	if (rogue)
		M_Print(160, 112, rogueepisodes[startepisode].description);
	else
		M_Print(160, 112, episodes[startepisode].description);

	M_Print(0, 120, "           Level");
	//MED 01/06/97 added hipnotic episodes
	if (hipnotic)
	{
		M_Print(160, 120, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].description);
		M_Print(160, 128, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name);
	}
	//PGM 01/07/97 added rogue episodes
	else
	if (rogue)
	{
		M_Print(160, 120, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].description);
		M_Print(160, 128, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name);
	}
	else
	{
		M_Print(160, 120, levels[episodes[startepisode].firstLevel + startlevel].description);
		M_Print(160, 128, levels[episodes[startepisode].firstLevel + startlevel].name);
	}

	// line cursor
	M_DrawCharacter(144, gameoptions_cursor_table[gameoptions_cursor], 12 + ((int)(realtime * 4) & 1));

	if (m_serverInfoMessage)
	{
		if ((realtime - m_serverInfoMessageTime) < 5.0)
		{
			x = (320 - 26 * 8) / 2;
			M_DrawTextBox(x, 138, 24, 4);
			x += 8;
			M_Print(x, 146, "  More than 4 players   ");
			M_Print(x, 154, " requires using command ");
			M_Print(x, 162, "line parameters; please ");
			M_Print(x, 170, "   see techinfo.txt.    ");
		}
		else
		{
			m_serverInfoMessage = false;
		}
	}
}

void M_NetStart_Change(int dir)
{
	int count;

	switch (gameoptions_cursor)
	{
	case 1:
		maxplayers += dir;
		if (maxplayers > svs.maxclientslimit)
		{
			maxplayers = svs.maxclientslimit;
			m_serverInfoMessage = true;
			m_serverInfoMessageTime = realtime;
		}
		if (maxplayers < 2)
			maxplayers = 2;
		break;

	case 2:
		Cvar_SetValue("coop", coop.value ? 0 : 1);
		break;

	case 3:
		if (rogue)
			count = 6;
		else
			count = 2;

		Cvar_SetValue("teamplay", teamplay.value + dir);
		if (teamplay.value > count)
			Cvar_SetValue("teamplay", 0);
		else
		if (teamplay.value < 0)
			Cvar_SetValue("teamplay", count);
		break;

	case 4:
		Cvar_SetValue("skill", skill.value + dir);
		if (skill.value > 3)
			Cvar_SetValue("skill", 0);
		if (skill.value < 0)
			Cvar_SetValue("skill", 3);
		break;

	case 5:
		Cvar_SetValue("fraglimit", fraglimit.value + dir * 10);
		if (fraglimit.value > 100)
			Cvar_SetValue("fraglimit", 0);
		if (fraglimit.value < 0)
			Cvar_SetValue("fraglimit", 100);
		break;

	case 6:
		Cvar_SetValue("timelimit", timelimit.value + dir * 5);
		if (timelimit.value > 60)
			Cvar_SetValue("timelimit", 0);
		if (timelimit.value < 0)
			Cvar_SetValue("timelimit", 60);
		break;

	case 7:
		startepisode += dir;
		//MED 01/06/97 added hipnotic count
		if (hipnotic)
			count = 6;
		//PGM 01/07/97 added rogue count
		//PGM 03/02/97 added 1 for dmatch episode
		else
		if (rogue)
			count = 4;
		else
		if (registered.value)
			count = 7;
		else
			count = 2;

		if (startepisode < 0)
			startepisode = count - 1;

		if (startepisode >= count)
			startepisode = 0;

		startlevel = 0;
		break;

	case 8:
		startlevel += dir;
		//MED 01/06/97 added hipnotic episodes
		if (hipnotic)
			count = hipnoticepisodes[startepisode].levels;
		//PGM 01/06/97 added hipnotic episodes
		else
		if (rogue)
			count = rogueepisodes[startepisode].levels;
		else
			count = episodes[startepisode].levels;

		if (startlevel < 0)
			startlevel = count - 1;

		if (startlevel >= count)
			startlevel = 0;
		break;
	}
}

void M_GameOptions_onKey(int key)
{
	switch (key)
	{
    case K_GAMEPAD_SELECT:
	case K_ESCAPE:
		//M_Network_enter();
        M_MultiPlayer_enter();
		break;

	case K_GAMEPAD_UP:
	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		gameoptions_cursor--;
		if (gameoptions_cursor < 0)
			gameoptions_cursor = NUM_GAMEOPTIONS - 1;
		break;

	case K_GAMEPAD_DOWN:
	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		gameoptions_cursor++;
		if (gameoptions_cursor >= NUM_GAMEOPTIONS)
			gameoptions_cursor = 0;
		break;

	case K_GAMEPAD_LEFT:
	case K_LEFTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound("misc/menu3.wav");
		M_NetStart_Change(-1);
		break;

	case K_GAMEPAD_RIGHT:
	case K_RIGHTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound("misc/menu3.wav");
		M_NetStart_Change(1);
		break;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_JOY1:
	case K_KP_ENTER:
	case K_ENTER:
		S_LocalSound("misc/menu2.wav");
		if (gameoptions_cursor == 0)
		{
			if (sv.active)
				Cbuf_AddText("disconnect\n");
			Cbuf_AddText("listen 0\n"); // so host_netport will be re-examined
			Cbuf_AddText(va("maxplayers %u\n", maxplayers));
			SCR_BeginLoadingPlaque();

			if (hipnotic)
				Cbuf_AddText(va("map %s\n", hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name));
			else
			if (rogue)
				Cbuf_AddText(va("map %s\n", roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name));
			else
				Cbuf_AddText(va("map %s\n", levels[episodes[startepisode].firstLevel + startlevel].name));

			return;
		}

		M_NetStart_Change(1);
		break;
	}
}
