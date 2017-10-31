#include "Client/client.h"
#include "Client/menu.h"
#include "Common/cmd.h"
#include "Common/quakedef.h"

#include <string.h>

static const char *M_Keys_bindNames[][2] =
{
	{ "+attack", "attack" },
	{ "impulse 10", "change weapon" },
	{ "+forward", "walk forward" },
	{ "+back", "backpedal" },
	{ "+moveleft", "step left" },
	{ "+moveright", "step right" },
	{ "+strafe", "sidestep" },
	{ "+speed", "run" },
	{ "+jump", "jump / swim up" },
	{ "+moveup", "swim up" },
	{ "+movedown", "swim down" },
	{ "+left", "turn left" },
	{ "+right", "turn right" },
	{ "+lookup", "look up" },
	{ "+lookdown", "look down" },
	{ "centerview", "center view" },
	{ "+mlook", "mouse look" },
	{ "+klook", "keyboard look" },
	{ "toggleconsole", "toggle console" },
};

#define NUMCOMMANDS (sizeof(M_Keys_bindNames) / sizeof(M_Keys_bindNames[0]))

static int M_Keys_cursor;
static bool M_Keys_bindGrab;

void M_Keys_enter()
{
	key_dest = key_menu;
	m_state = m_keys;
	m_entersound = true;
}

static void M_Keys_findKeys(const char *command, int *twokeys)
{
	int l = strlen(command);
	int count = 0;
	twokeys[0] = twokeys[1] = -1;
	for (int j = 0; j < 256; j++)
	{
		char *b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp(b, command, l))
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

static void M_Keys_unbindCommand(const char *command)
{
	int l = strlen(command);
	for (int j = 0; j < 256; j++)
	{
		char *b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp(b, command, l))
			Key_SetBinding(j, "");
	}
}

void M_Keys_draw()
{
	qpic_t *p = Draw_CachePic("gfx/ttl_cstm.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

	if (M_Keys_bindGrab)
		M_Print(12, 32, "Press a key or button for this action");
	else
		M_Print(18, 32, "Enter to change, backspace to clear");

	// search for known bindings
	for (int i = 0; i < (int)NUMCOMMANDS; i++)
	{
		int y = 48 + 8 * i;

		M_Print(16, y, M_Keys_bindNames[i][1]);

        int keys[2];
		M_Keys_findKeys(M_Keys_bindNames[i][0], keys);

		if (keys[0] == -1)
		{
			M_Print(140, y, "???");
		}
		else
		{
            char *name = Key_KeynumToString(keys[0]);
			M_Print(140, y, name);
			int x = strlen(name) * 8;
			if (keys[1] != -1)
			{
				M_Print(140 + x + 8, y, "or");
				M_Print(140 + x + 32, y, Key_KeynumToString(keys[1]));
			}
		}
	}

	if (M_Keys_bindGrab)
		M_DrawCharacter(130, 48 + M_Keys_cursor * 8, '=');
	else
		M_DrawCharacter(130, 48 + M_Keys_cursor * 8, 12 + ((int)(realtime * 4) & 1));
}

void M_Keys_onKey(int k, int keyUnmodified)
{
	int keys[2];

	if (M_Keys_bindGrab) // defining a key
	{
		S_LocalSound("misc/menu1.wav");
		if (keyUnmodified > 0 && keyUnmodified != K_GAMEPAD_SELECT && keyUnmodified != K_ESCAPE)
		{
            char cmd[1024];
			snprintf(cmd, sizeof(cmd), "bind \"%s\" \"%s\"\n", Key_KeynumToString(keyUnmodified), M_Keys_bindNames[M_Keys_cursor][0]);
            cmd[1023] = 0;
			Cbuf_InsertText(cmd);
		}

		M_Keys_bindGrab = false;
		return;
	}

	switch (k)
	{
    case K_GAMEPAD_SELECT:
	case K_ESCAPE:
		M_Options_enter();
		break;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_JOY1:
	case K_KP_ENTER:
	case K_ENTER:
		M_Keys_findKeys(M_Keys_bindNames[M_Keys_cursor][0], keys);
		S_LocalSound("misc/menu2.wav");
		if (keys[1] != -1)
			M_Keys_unbindCommand(M_Keys_bindNames[M_Keys_cursor][0]);
		M_Keys_bindGrab = true;
		break;

	case K_GAMEPAD_Y:
	case K_JOY3:
	case K_BACKSPACE:
	case K_DEL:
		S_LocalSound("misc/menu2.wav");
		M_Keys_unbindCommand(M_Keys_bindNames[M_Keys_cursor][0]);
		break;

	case K_GAMEPAD_UP:
	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		M_Keys_cursor--;
		if (M_Keys_cursor < 0)
			M_Keys_cursor = NUMCOMMANDS - 1;
		break;

	case K_GAMEPAD_DOWN:
	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		M_Keys_cursor++;
		if (M_Keys_cursor >= (int)NUMCOMMANDS)
			M_Keys_cursor = 0;
		break;
	}
}
