#include "Client/menu.h"
#include "Client/screen.h"
#include "Common/cmd.h"
#include "Common/quakedef.h"
#include "Server/server.h"

#include <string.h>

static int load_cursor; // 0 < load_cursor < MAX_SAVEGAMES

#define MAX_SAVEGAMES 12
static char m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH + 1];
static int loadable[MAX_SAVEGAMES];

void M_ScanSaves()
{
	for (int i = 0; i < MAX_SAVEGAMES; i++)
	{
        char *filename = m_filenames[i];
        
		strncpy(filename, "--- UNUSED SLOT ---", SAVEGAME_COMMENT_LENGTH);
        filename[SAVEGAME_COMMENT_LENGTH] = 0;
		loadable[i] = false;

        char name[MAX_OSPATH+1];
		snprintf(name, MAX_OSPATH, "%s/s%i.sav", com_writableGamedir, i);
        name[MAX_OSPATH] = 0;
		FILE *f = fopen(name, "rb");
		if (!f)
			continue;
        int version;
		fscanf(f, "%i\n", &version);
		fscanf(f, "%79s\n", name);
		fclose(f);
        
		strncpy(filename, name, SAVEGAME_COMMENT_LENGTH);
        filename[SAVEGAME_COMMENT_LENGTH] = 0;

		// change _ back to space
		for (int j = 0; j < SAVEGAME_COMMENT_LENGTH; j++)
			if (filename[j] == '_')
				filename[j] = ' ';

		loadable[i] = true;
	}
}

void M_Load_enter()
{
	m_entersound = true;
	m_state = m_load;
	key_dest = key_menu;
	M_ScanSaves();
}

void M_Load_draw()
{
	int i;
	qpic_t *p;

	p = Draw_CachePic("gfx/p_load.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

	for (i = 0; i < MAX_SAVEGAMES; i++)
		M_Print(16, 32 + 8 * i, m_filenames[i]);

	// line cursor
	M_DrawCharacter(8, 32 + load_cursor * 8, 12 + ((int)(realtime * 4) & 1));
}

void M_Load_onKey(int k)
{
	switch (k)
	{
    case K_GAMEPAD_SELECT:
	case K_ESCAPE:
		M_SinglePlayer_enter();
		break;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_JOY1:
	case K_KP_ENTER:
	case K_ENTER:
		S_LocalSound("misc/menu2.wav");
		if (!loadable[load_cursor])
			return;

		m_state = m_none;
		key_dest = key_game;

		// Host_Loadgame_f can't bring up the loading plaque because too much
		// stack space has been used, so do it now
		SCR_BeginLoadingPlaque();

		// issue the load command
		Cbuf_AddText(va("load s%i\n", load_cursor));
		return;

    case K_GAMEPAD_UP:
	case K_UPARROW:
    case K_GAMEPAD_LEFT:
	case K_LEFTARROW:
		S_LocalSound("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES - 1;
		break;

    case K_GAMEPAD_DOWN:
	case K_DOWNARROW:
    case K_GAMEPAD_RIGHT:
	case K_RIGHTARROW:
		S_LocalSound("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}

void M_Save_enter()
{
	if (!sv.active)
		return;

	if (cl.intermission)
		return;

	if (svs.maxclients != 1)
		return;

	m_entersound = true;
	m_state = m_save;
	key_dest = key_menu;
	M_ScanSaves();
}

void M_Save_draw()
{
	int i;
	qpic_t *p;

	p = Draw_CachePic("gfx/p_save.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

	for (i = 0; i < MAX_SAVEGAMES; i++)
		M_Print(16, 32 + 8 * i, m_filenames[i]);

	// line cursor
	M_DrawCharacter(8, 32 + load_cursor * 8, 12 + ((int)(realtime * 4) & 1));
}

void M_Save_onKey(int k)
{
	switch (k)
	{
    case K_GAMEPAD_SELECT:
	case K_ESCAPE:
		M_SinglePlayer_enter();
		break;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_JOY1:
	case K_KP_ENTER:
	case K_ENTER:
		m_state = m_none;
		key_dest = key_game;
		Cbuf_AddText(va("save s%i\n", load_cursor));
		return;

    case K_GAMEPAD_UP:
	case K_UPARROW:
    case K_GAMEPAD_LEFT:
	case K_LEFTARROW:
		S_LocalSound("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES - 1;
		break;

    case K_GAMEPAD_DOWN:
	case K_DOWNARROW:
    case K_GAMEPAD_RIGHT:
	case K_RIGHTARROW:
		S_LocalSound("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}
