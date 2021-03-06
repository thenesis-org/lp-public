/*
   Copyright (C) 1996-1997 Id Software, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 */
#include "Client/client.h"
#include "Client/console.h"
#include "Client/keys.h"
#include "Client/menu.h"
#include "Client/screen.h"
#include "Common/cmd.h"
#include "Common/cvar.h"
#include "Common/sys.h"
#include "Common/zone.h"
#include "Rendering/r_private.h"

#include <string.h>

/*
   key up events are sent even if in console mode
 */

#define MAXCMDLINE 256
char key_lines[32][MAXCMDLINE];
int key_linepos;
int key_lastpress;

int edit_line = 0;
int history_line = 0;

keydest_t key_dest;

int key_count; // incremented every key event

char *keybindings[K_LAST];
qboolean consolekeys[K_LAST]; // if true, can't be rebound while in console
qboolean menubound[K_LAST]; // if true, can't be rebound while in menu
static int key_repeats[K_LAST]; // if > 1, it is autorepeating
qboolean keydown[K_LAST];

typedef struct
{
	char *name;
	int keynum;
} keyname_t;

// Translates internal key representations into human readable strings.
keyname_t keynames[] =
{
	{ "SELECT", K_GAMEPAD_SELECT },
	{ "START", K_GAMEPAD_START },

	{ "LEFT", K_GAMEPAD_LEFT },
	{ "RIGHT", K_GAMEPAD_RIGHT },
	{ "DOWN", K_GAMEPAD_DOWN },
	{ "UP", K_GAMEPAD_UP },

	//    {"A", K_GAMEPAD_A},
	//    {"B", K_GAMEPAD_B},
	//    {"X", K_GAMEPAD_X},
	//    {"Y", K_GAMEPAD_Y},

	//    {"L", K_GAMEPAD_L},
	//    {"R", K_GAMEPAD_R},

	//    {"LOCK", K_GAMEPAD_LOCK},
	//    {"PAUSE", K_GAMEPAD_POWER},

	{ "TAB", K_TAB },
	{ "ENTER", K_ENTER },
	{ "ESCAPE", K_ESCAPE },
	{ "SPACE", K_SPACE },
	{ "BACKSPACE", K_BACKSPACE },

	{ "COMMAND", K_COMMAND },
	{ "CAPSLOCK", K_CAPSLOCK },
	{ "POWER", K_POWER },
	{ "PAUSE", K_PAUSE },

	{ "UPARROW", K_UPARROW },
	{ "DOWNARROW", K_DOWNARROW },
	{ "LEFTARROW", K_LEFTARROW },
	{ "RIGHTARROW", K_RIGHTARROW },

	{ "ALT", K_ALT },
	{ "CTRL", K_CTRL },
	{ "SHIFT", K_SHIFT },

	{ "F1", K_F1 },
	{ "F2", K_F2 },
	{ "F3", K_F3 },
	{ "F4", K_F4 },
	{ "F5", K_F5 },
	{ "F6", K_F6 },
	{ "F7", K_F7 },
	{ "F8", K_F8 },
	{ "F9", K_F9 },
	{ "F10", K_F10 },
	{ "F11", K_F11 },
	{ "F12", K_F12 },

	{ "INS", K_INS },
	{ "DEL", K_DEL },
	{ "PGDN", K_PGDN },
	{ "PGUP", K_PGUP },
	{ "HOME", K_HOME },
	{ "END", K_END },

	{ "MOUSE1", K_MOUSE1 },
	{ "MOUSE2", K_MOUSE2 },
	{ "MOUSE3", K_MOUSE3 },
	{ "MOUSE4", K_MOUSE4 },
	{ "MOUSE5", K_MOUSE5 },

	{ "JOY1", K_JOY1 },
	{ "JOY2", K_JOY2 },
	{ "JOY3", K_JOY3 },
	{ "JOY4", K_JOY4 },
	{ "JOY5", K_JOY5 },
	{ "JOY6", K_JOY6 },
	{ "JOY7", K_JOY7 },
	{ "JOY8", K_JOY8 },
	{ "JOY9", K_JOY9 },
	{ "JOY10", K_JOY10 },
	{ "JOY11", K_JOY11 },
	{ "JOY12", K_JOY12 },
	{ "JOY13", K_JOY13 },
	{ "JOY14", K_JOY14 },
	{ "JOY15", K_JOY15 },
	{ "JOY16", K_JOY16 },
	{ "JOY17", K_JOY17 },
	{ "JOY18", K_JOY18 },
	{ "JOY19", K_JOY19 },
	{ "JOY20", K_JOY20 },
	{ "JOY21", K_JOY21 },
	{ "JOY22", K_JOY22 },
	{ "JOY23", K_JOY23 },
	{ "JOY24", K_JOY24 },
	{ "JOY25", K_JOY25 },
	{ "JOY26", K_JOY26 },
	{ "JOY27", K_JOY27 },
	{ "JOY28", K_JOY28 },
	{ "JOY29", K_JOY29 },
	{ "JOY30", K_JOY30 },
	{ "JOY31", K_JOY31 },
	{ "JOY32", K_JOY32 },

	{ "AUX1", K_AUX1 },
	{ "AUX2", K_AUX2 },
	{ "AUX3", K_AUX3 },
	{ "AUX4", K_AUX4 },
	{ "AUX5", K_AUX5 },
	{ "AUX6", K_AUX6 },
	{ "AUX7", K_AUX7 },
	{ "AUX8", K_AUX8 },
	{ "AUX9", K_AUX9 },
	{ "AUX10", K_AUX10 },
	{ "AUX11", K_AUX11 },
	{ "AUX12", K_AUX12 },
	{ "AUX13", K_AUX13 },
	{ "AUX14", K_AUX14 },
	{ "AUX15", K_AUX15 },
	{ "AUX16", K_AUX16 },
	{ "AUX17", K_AUX17 },
	{ "AUX18", K_AUX18 },
	{ "AUX19", K_AUX19 },
	{ "AUX20", K_AUX20 },
	{ "AUX21", K_AUX21 },
	{ "AUX22", K_AUX22 },
	{ "AUX23", K_AUX23 },
	{ "AUX24", K_AUX24 },
	{ "AUX25", K_AUX25 },
	{ "AUX26", K_AUX26 },
	{ "AUX27", K_AUX27 },
	{ "AUX28", K_AUX28 },
	{ "AUX29", K_AUX29 },
	{ "AUX30", K_AUX30 },
	{ "AUX31", K_AUX31 },
	{ "AUX32", K_AUX32 },

	{ "KP_HOME", K_KP_HOME },
	{ "KP_UPARROW", K_KP_UPARROW },
	{ "KP_PGUP", K_KP_PGUP },
	{ "KP_LEFTARROW", K_KP_LEFTARROW },
	{ "KP_5", K_KP_5 },
	{ "KP_RIGHTARROW", K_KP_RIGHTARROW },
	{ "KP_END", K_KP_END },
	{ "KP_DOWNARROW", K_KP_DOWNARROW },
	{ "KP_PGDN", K_KP_PGDN },
	{ "KP_ENTER", K_KP_ENTER },
	{ "KP_INS", K_KP_INS },
	{ "KP_DEL", K_KP_DEL },
	{ "KP_SLASH", K_KP_SLASH },
	{ "KP_MINUS", K_KP_MINUS },
	{ "KP_PLUS", K_KP_PLUS },

	{ "MWHEELUP", K_MWHEELUP },
	{ "MWHEELDOWN", K_MWHEELDOWN },

	{ "SEMICOLON", ';' }, // Because a raw semicolon seperates commands.

	{ NULL, 0 }
};

/*
   ==============================================================================

                        LINE TYPING INTO THE CONSOLE

   ==============================================================================
 */

/*
   Interactive line editing and console scrollback
 */
void Key_Console(int key)
{
	if (key == K_ENTER)
	{
		Cbuf_AddText(key_lines[edit_line] + 1); // skip the >
		Cbuf_AddText("\n");
		Con_Printf("%s\n", key_lines[edit_line]);
		edit_line = (edit_line + 1) & 31;
		history_line = edit_line;
		key_lines[edit_line][0] = ']';
		key_linepos = 1;
		if (cls.state == ca_disconnected)
			SCR_UpdateScreen(); // force an update, because the command
		// may take some time
		return;
	}

	if (key == K_TAB) // command completion
	{
		char *cmd = Cmd_CompleteCommand(key_lines[edit_line] + 1);
		if (!cmd)
			cmd = Cvar_CompleteVariable(key_lines[edit_line] + 1);
		if (cmd)
		{
			Q_strcpy(key_lines[edit_line] + 1, cmd);
			key_linepos = Q_strlen(cmd) + 1;
			key_lines[edit_line][key_linepos] = ' ';
			key_linepos++;
			key_lines[edit_line][key_linepos] = 0;
			return;
		}
	}

	if (key == K_BACKSPACE || key == K_LEFTARROW)
	{
		if (key_linepos > 1)
			key_linepos--;
		return;
	}

	if (key == K_UPARROW)
	{
		do
		{
			history_line = (history_line - 1) & 31;
		}
		while (history_line != edit_line && !key_lines[history_line][1]);
		if (history_line == edit_line)
			history_line = (edit_line + 1) & 31;
		Q_strcpy(key_lines[edit_line], key_lines[history_line]);
		key_linepos = Q_strlen(key_lines[edit_line]);
		return;
	}

	if (key == K_DOWNARROW)
	{
		if (history_line == edit_line)
			return;

		do
		{
			history_line = (history_line + 1) & 31;
		}
		while (history_line != edit_line
		       && !key_lines[history_line][1]);
		if (history_line == edit_line)
		{
			key_lines[edit_line][0] = ']';
			key_linepos = 1;
		}
		else
		{
			Q_strcpy(key_lines[edit_line], key_lines[history_line]);
			key_linepos = Q_strlen(key_lines[edit_line]);
		}
		return;
	}

	if (key == K_PGUP || key == K_MWHEELUP)
	{
		con_backscroll += 2;
		if (con_backscroll > con_totallines - (vid.height >> 3) - 1)
			con_backscroll = con_totallines - (vid.height >> 3) - 1;
		return;
	}

	if (key == K_PGDN || key == K_MWHEELDOWN)
	{
		con_backscroll -= 2;
		if (con_backscroll < 0)
			con_backscroll = 0;
		return;
	}

	if (key == K_HOME)
	{
		con_backscroll = con_totallines - (vid.height >> 3) - 1;
		return;
	}

	if (key == K_END)
	{
		con_backscroll = 0;
		return;
	}

	if (key < 32 || key > 127)
		return; // non printable

	if (key_linepos < MAXCMDLINE - 1)
	{
		key_lines[edit_line][key_linepos] = key;
		key_linepos++;
		key_lines[edit_line][key_linepos] = 0;
	}
}

char chat_buffer[32];
qboolean team_message = false;

void Key_Message(int key)
{
	static int chat_bufferlen = 0;

	if (key == K_ENTER)
	{
		if (team_message)
			Cbuf_AddText("say_team \"");
		else
			Cbuf_AddText("say \"");
		Cbuf_AddText(chat_buffer);
		Cbuf_AddText("\"\n");

		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key == K_ESCAPE || key == K_GAMEPAD_SELECT)
	{
		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key < 32 || key > 127)
		return;                     // non printable

	if (key == K_BACKSPACE)
	{
		if (chat_bufferlen)
		{
			chat_bufferlen--;
			chat_buffer[chat_bufferlen] = 0;
		}
		return;
	}

	if (chat_bufferlen == 31)
		return;                    // all full

	chat_buffer[chat_bufferlen++] = key;
	chat_buffer[chat_bufferlen] = 0;
}

/*
   Returns a key number to be used to index keybindings[] by looking at
   the given string.  Single ascii characters return themselves, while
   the K_* names are matched up.
 */
int Key_StringToKeynum(char *str)
{
	if (!str || !str[0])
		return -1;

	if (!str[1])
		return str[0];

	keyname_t *kn;
	for (kn = keynames; kn->name; kn++)
	{
		if (!Q_strcasecmp(str, kn->name))
			return kn->keynum;
	}
	return -1;
}

/*
   Returns a string (either a single ascii char, or a K_* name) for the
   given keynum.
   FIXME: handle quote special (general escape sequence?)
 */
char* Key_KeynumToString(int keynum)
{
	if (keynum == -1)
		return "<KEY NOT FOUND>";

	if (keynum > 32 && keynum < 127) // printable ascii
	{
        static char tinystr[2];
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	for (keyname_t *kn = keynames; kn->name; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}

void Key_SetBinding(int keynum, char *binding)
{
	char *new;
	int l;

	if (keynum == -1)
		return;

	// free old bindings
	if (keybindings[keynum])
	{
		Z_Free(keybindings[keynum]);
		keybindings[keynum] = NULL;
	}

	// allocate memory for new binding
	l = Q_strlen(binding);
	new = Z_Malloc(l + 1);
	Q_strcpy(new, binding);
	new[l] = 0;
	keybindings[keynum] = new;
}

void Key_Unbind_f()
{
	if (Cmd_Argc() != 2)
	{
		Con_Printf("unbind <key> : remove commands from a key\n");
		return;
	}

	int b = Key_StringToKeynum(Cmd_Argv(1));
	if (b == -1)
	{
		Con_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Key_SetBinding(b, "");
}

void Key_Unbindall_f()
{
	for (int i = 0; i < 256; i++)
		if (keybindings[i])
			Key_SetBinding(i, "");
}

void Key_Bind_f()
{
	int i, c, b;
	c = Cmd_Argc();

	if (c != 2 && c != 3)
	{
		Con_Printf("bind <key> [command] : attach a command to a key\n");
		return;
	}
	b = Key_StringToKeynum(Cmd_Argv(1));
	if (b == -1)
	{
		Con_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	// Don't allow binding escape.
	if (b == K_GAMEPAD_SELECT || b == K_ESCAPE)
	{
		return;
	}

	if (c == 2)
	{
		if (keybindings[b])
			Con_Printf("\"%s\" = \"%s\"\n", Cmd_Argv(1), keybindings[b]);
		else
			Con_Printf("\"%s\" is not bound\n", Cmd_Argv(1));
		return;
	}

	// copy the rest of the command line
	char cmd[1024];
	cmd[0] = 0; // start out with a null string
	for (i = 2; i < c; i++)
	{
		if (i > 2)
			Q_strncat(cmd, " ", 1024);
		Q_strncat(cmd, Cmd_Argv(i), 1024);
	}

	Key_SetBinding(b, cmd);
}

/*
   Writes lines containing "bind key value"
 */
void Key_WriteBindings(FILE *f)
{
//	if (cfg_unbindall->value)
    fprintf(f, "unbindall\n");
    
	for (int i = 0; i < 256; i++)
		if (keybindings[i])
			if (*keybindings[i])
				fprintf(f, "bind \"%s\" \"%s\"\n", Key_KeynumToString(i), keybindings[i]);

}

void Key_Init()
{
	int i;

	for (i = 0; i < 32; i++)
	{
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}
	key_linepos = 1;

	//
	// init ascii characters in console mode
	//
	for (i = 32; i < 128; i++)
		consolekeys[i] = true;
	consolekeys[K_ENTER] = true;
	consolekeys[K_TAB] = true;
	consolekeys[K_LEFTARROW] = true;
	consolekeys[K_RIGHTARROW] = true;
	consolekeys[K_UPARROW] = true;
	consolekeys[K_DOWNARROW] = true;
	consolekeys[K_BACKSPACE] = true;
	consolekeys[K_PGUP] = true;
	consolekeys[K_PGDN] = true;
	consolekeys[K_SHIFT] = true;
	consolekeys[K_MWHEELUP] = true;
	consolekeys[K_MWHEELDOWN] = true;

	menubound[K_ESCAPE] = true;
	for (i = 0; i < 12; i++)
		menubound[K_F1 + i] = true;

	//
	// register our functions
	//
	Cmd_AddCommand("bind", Key_Bind_f);
	Cmd_AddCommand("unbind", Key_Unbind_f);
	Cmd_AddCommand("unbindall", Key_Unbindall_f);
}

static bool Key_isSpecial(int key)
{
    return !(key >= ' ' && key < '~');
}

void Char_Event(int key, bool specialOnlyFlag)
{
    #if defined(__GCW_ZERO__)
    bool specialFlag = specialOnlyFlag;
    #else
    bool specialFlag = Key_isSpecial(key);
    #endif
    
    switch (key_dest)
    {
    default:
        break;
        
    /* Chat */
    case key_message:
        if (specialOnlyFlag == specialFlag)
            Key_Message(key);
        break;

    /* Menu */
    case key_menu:
        if (specialOnlyFlag)
        {
            if (specialFlag)
                M_Keydown(key, key);
            else
                M_Keydown(-1, key); // Because it will be processed by Char_event() (and so must not be processed twice) but also needed by MenuKey for binding.
        }
        else
        {
            if (!specialFlag)
                M_Keydown(key, -1);
        }
        break;

    /* Console */
    case key_console:
        if (specialOnlyFlag == specialFlag)
            Key_Console(key);
        break;

    /* Console is really open but key_dest is game anyway (not connected) */
    case key_game:
        if (cls.state == ca_disconnected)
        {
            if (specialOnlyFlag == specialFlag)
                Key_Console(key);
        }
        break;
    }
}

#if defined(__GCW_ZERO__)
static void Key_checkHarwiredCheats(int key)
{
    // Cheats for GCW Zero.
    if (keydown[K_GAMEPAD_LOCK])
    {
        switch (key)
        {
        case K_GAMEPAD_B:
            Cbuf_AddText("impulse 9\n"); // All weapons and ammos.
            return;
        case K_GAMEPAD_X:
            Cbuf_AddText("impulse -1\n"); // Quad damage.
            return;
        case K_GAMEPAD_Y:
            Cbuf_AddText("impulse 11\n"); // Get a rune.
            return;
        case K_GAMEPAD_START:
            Cbuf_AddText("cheats 1\n");
            return;
        case K_GAMEPAD_LEFT:
            Cbuf_AddText("notarget\n");
            return;
        case K_GAMEPAD_RIGHT:
            Cbuf_AddText("fly\n");
            return;
        case K_GAMEPAD_DOWN:
            Cbuf_AddText("noclip\n");
            return;
        case K_GAMEPAD_UP:
            Cbuf_AddText("god\n");
            return;
        case K_GAMEPAD_L:
            if (r_draw_world.value == 0)
                Cbuf_AddText("r_draw_world 1\n");
            else
                Cbuf_AddText("r_draw_world 0\n");
            return;
        case K_GAMEPAD_R:
            if (r_lightmap_only.value == 0)
                Cbuf_AddText("r_lightmap_only 1\n");
            else
                Cbuf_AddText("r_lightmap_only 0\n");
            return;
        }
    }
}
#endif

/*
   Called by the system between frames for both key up and key down events
   Should NOT be called during an interrupt!
 */
void Key_Event(int key, bool down)
{
	char cmd[1024];

	keydown[key] = down;

	if (!down)
		key_repeats[key] = 0;

	key_lastpress = key;
	key_count++;
	if (key_count <= 0)
		return; // just catching keys for Con_NotifyBox

	// update auto-repeat status
	if (down)
	{
		key_repeats[key]++;
        if (key_repeats[key] > 1)
        {
            if (key_dest == key_game && cls.state == ca_connected)
                return; // Ignore autorepeats in-game.
        }
	}

	// Fullscreen switch through Alt + Return.
	if (down && keydown[K_ALT] && key == K_ENTER)
	{
//        VID_toggleFullScreen(); // TODO: Implement
		return;
	}

	// Toggle console though Shift + Escape.
	if (down && keydown[K_SHIFT] && key == K_ESCAPE)
	{
		Con_ToggleConsole_f();
		return;
	}

	//
	// handle escape specialy, so the user can never unbind it
	//
	if (key == K_ESCAPE || key == K_GAMEPAD_SELECT)
	{
		if (!down)
			return;
        switch (key_dest)
        {
        default: // Should not occur
            break;
        case key_message:
            Key_Message(key);
            break;
        case key_menu:
            M_Keydown(key, -1);
            break;
        case key_game:
        case key_console:
            M_ToggleMenu_f();
            break;
        }
		return;
	}

	//
	// during demo playback, most keys bring up the main menu
	//
	if (cls.demoplayback && down && consolekeys[key] && key_dest == key_game)
	{
		M_ToggleMenu_f();
		return;
	}

    #if defined(__GCW_ZERO__)
    if (down)
        Key_checkHarwiredCheats(key);
    #endif

	// key up events only generate commands if the game key binding is
	// a button command (leading + sign).  These will occur even in console mode,
	// to keep the character from continuing an action started before a console
	// switch.  Button commands include the kenum as a parameter, so multiple
	// downs can be matched with ups
	if (!down)
	{
		char *kb = keybindings[key];
		if (kb && kb[0] == '+')
		{
			sprintf(cmd, "-%s %i\n", kb + 1, key);
			Cbuf_AddText(cmd);
		}
		return;
	}

	//
	// if not a consolekey, send to the interpreter no matter what mode is
	//
	if (
        (key_dest == key_menu && menubound[key]) ||
	    (key_dest == key_console && !consolekeys[key]) ||
	    (key_dest == key_game && (!con_forcedup || !consolekeys[key])))
	{
		char *kb = keybindings[key];
		if (kb)
		{
			if (kb[0] == '+') // button commands add keynum as a parm
			{
				sprintf(cmd, "%s %i\n", kb, key);
				Cbuf_AddText(cmd);
			}
			else
			{
				Cbuf_AddText(kb);
				Cbuf_AddText("\n");
			}
		}
		return;
	}

    Char_Event(key, true);
}

void Key_ClearStates()
{
	for (int i = 0; i < 256; i++)
	{
		keydown[i] = false;
		key_repeats[i] = 0;
	}
}
