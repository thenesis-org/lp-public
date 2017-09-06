#include "client/menu/menu.h"

static char *bindnames[][2] =
{
	{ "+speed", "run" },
	{ "+forward", "walk forward" },
	{ "+back", "backpedal" },
	{ "+moveleft", "step left" },
	{ "+moveright", "step right" },
	{ "+strafe", "sidestep" },
	{ "+moveup", "up / jump" },
	{ "+movedown", "down / crouch" },
	{ "+toggledown", "toggle crouch" },

	#if !defined(GAMEPAD_ONLY)
	{ "+left", "turn left" }, // Already with the stick.
	{ "+right", "turn right" }, // Already with the stick.
	{ "+lookup", "look up" }, // Already with the stick.
	{ "+lookdown", "look down" }, // Already with the stick.
	{ "+klook", "keyboard look" }, // Already with the stick.
	#endif
	{ "+mlook", "mouse look" },
	{ "centerview", "center view" },

    { NULL, NULL },

	{ "+attack", "attack" },

	{ "weapnext", "next weapon" },
	{ "weapprev", "previous weapon" },
	{ "weaplast", "last weapon" },
	{ "use Blaster", "Blaster" },
	{ "use Shotgun", "Shotgun" },
	{ "use Super Shotgun", "Super Shotgun" },
	{ "use Machinegun", "Machinegun" },
	{ "use Chaingun", "Chaingun" },
	{ "use Grenades", "Grenades" },
	{ "use Grenade Launcher", "Grenade Launcher" },
	{ "use Rocket Launcher", "Rocket Launcher" },
	{ "use HyperBlaster", "HyperBlaster" },
	{ "use Railgun", "Railgun" },
	{ "use BFG10K", "BFG10K" },
	{ "use Trap", "Trap (xatrix)" },
	{ "use IonRipper", "IonRipper (xatrix)" },
	{ "use Phalanx", "Phalanx (xatrix)" },
	{ "use ETF Rifle", "ETF Rifle (rogue)" },
	{ "use Prox Launcher", "Prox Launcher (rogue)" },
	{ "use Plasma Beam", "Plasma Beam (rogue)" },
	{ "use Chainfist", "Chainfist (rogue)" },
	{ "use Disruptor", "Disruptor (rogue)" },

    { NULL, NULL },

	{ "inven", "inventory" },
	{ "invuse", "use item" },
	{ "invdrop", "drop item" },
	{ "invprev", "prev item" },
	{ "invnext", "next item" },
    { "use quad damage", "quad damage" },
    { "use invulnerability", "invulnerability" },
    { "use silencer", "silencer" },
    { "use rebreather", "rebreather" },
    { "use environment suit", "environment suit" },
    { "use power shield", "power shield" },
    { "use dualfire damage", "dualfire damage (xatrix)" },
    { "use IR Goggles", "IR Goggles (rogue)" },
    { "use Double Damage", "Double Damage (rogue)" },
    { "use compass", "compass (rogue)" },
    { "use vengeance sphere", "vengeance sphere (rogue)" },
    { "use hunter sphere", "hunter sphere (rogue)" },
    { "use defender sphere", "defender sphere (rogue)" },
    { "use Doppleganger", "Doppleganger (rogue)" },

    { NULL, NULL },

	{ "cmd help", "help computer" },
	{ "togglepause", "toggle pause" },
	{ "+pause", "pause" },
	{ "echo Quick Saving...; wait; save quick", "save quick" },
	{ "echo Quick Loading...; wait; load quick", "load quick" },
	{ "screenshot", "screenshot" },
	{ "toggleconsole", "toggle console" },

    { NULL, NULL },

	{ "messagemode", "message" },
	{ "messagemode2", "team message" },
	{ "togglechat", "toggle chat" },
	{ "wave 1", "wave 1" },
	{ "wave 2", "wave 2" },
	{ "wave 3", "wave 3" },
	{ "wave 4", "wave 4" },
	{ "kill", "kill yourself" },

    { NULL, NULL },

	{ "god", "god mode" },
	{ "noclip", "no clip" },
	{ "notarget", "no target" },
	{ "give all", "all items" },

    { NULL, NULL },

	{ "menu_video", "video menu" },
	{ "menu_options", "options menu" },
	{ "menu_keys", "keys menu" },
	{ "menu_quit", "quit menu" },
	{ "menu_main", "main menu" },
	{ "menu_game", "game menu" },
	{ "menu_loadgame", "load menu" },
	{ "menu_savegame", "save menu" },
	{ "menu_multiplayer", "multiplayer menu" },
	{ "menu_joinserver", "join server menu" },
	{ "menu_addressbook", "address book menu" },
	{ "menu_startserver", "start server menu" },
	{ "menu_dmoptions", "deathmatch menu" },
	{ "menu_playerconfig", "player menu" },
	{ "menu_downloadoptions", "download menu" },
	{ "menu_credits", "credits menu" },
};
#define NUM_BINDNAMES (int)(sizeof bindnames / sizeof bindnames[0])

#define KEYS_PER_PAGE 25
#define PAGE_NB ((NUM_BINDNAMES + KEYS_PER_PAGE - 1) / KEYS_PER_PAGE)

static qboolean bind_grab;

static menuframework_s s_keys_menu;
static menuaction_s s_keys_actions[NUM_BINDNAMES];

static void M_UnbindCommand(char *command)
{
	int l = Q_strlen(command);
	for (int j = 0; j < 256; j++)
	{
		char *b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp(b, command, l))
			Key_SetBinding(j, "");
	}
}

static void M_FindKeysForCommand(char *command, int *twokeys)
{
	int l = Q_strlen(command);
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

static void KeyCursorDrawFunc(menuframework_s *menu)
{
	menucommon_s *item = Menu_ItemAtCursor(menu);
	if (item)
	{
		float scale = SCR_GetMenuScale();
		if (bind_grab)
			Draw_CharScaled(menu->x + item->x, (menu->y + item->y) * scale, '=', scale);
		else
			Draw_CharScaled(menu->x + item->x, (menu->y + item->y) * scale, 12 + ((int)(Sys_Milliseconds() / 250) & 1), scale);
	}
}

static void DrawKeyBindingFunc(void *self)
{
	menuaction_s *a = (menuaction_s *)self;

	int keys[2];
	M_FindKeysForCommand(bindnames[a->generic.localdata[0]][0], keys);

	float scale = SCR_GetMenuScale();
	Draw_CharBegin();

	if (keys[0] == -1)
	{
		Menu_DrawString(a->generic.x + a->generic.parent->x + 16 * scale, a->generic.y + a->generic.parent->y, "???");
	}
	else
	{
		const char *name = Key_KeynumToString(keys[0]);

		Menu_DrawString(a->generic.x + a->generic.parent->x + 16 * scale,
			a->generic.y + a->generic.parent->y, name);

		int x = Q_strlen(name) * 8;

		if (keys[1] != -1)
		{
			Menu_DrawString(a->generic.x + a->generic.parent->x + 24 * scale + (x * scale),
				a->generic.y + a->generic.parent->y, "or");
			Menu_DrawString(a->generic.x + a->generic.parent->x + 48 * scale + (x * scale),
				a->generic.y + a->generic.parent->y,
				Key_KeynumToString(keys[1]));
		}
	}

	Draw_CharEnd();
}

static void MenuKeys_resetStatusBar(menuframework_s *menu)
{
	#if defined(GAMEPAD_ONLY)
	Menu_SetStatusBar(menu, "A to change, Y to clear");
	#else
	Menu_SetStatusBar(menu, "ENTER to change, BACKSPACE to clear");
	#endif
}

static void KeyBindingFunc(void *self)
{
	menuaction_s *a = (menuaction_s *)self;
	char *bindCommand = bindnames[a->generic.localdata[0]][0];

	int keys[2];
	M_FindKeysForCommand(bindCommand, keys);
	if (keys[1] != -1)
		M_UnbindCommand(bindCommand);

	bind_grab = true;

	#if defined(GAMEPAD_ONLY)
	Menu_SetStatusBar(&s_keys_menu, "press a button for this action");
	#else
	Menu_SetStatusBar(&s_keys_menu, "press a key or button for this action");
	#endif
}

static void Keys_MenuInit()
{
	menuframework_s *menu = &s_keys_menu;
	memset(menu, 0, sizeof(menuframework_s));
	menu->x = (int)(viddef.width * 0.50f);
	menu->nitems = 0;
	menu->cursordraw = KeyCursorDrawFunc;
	menu->itemPerPageNb = KEYS_PER_PAGE;

	for (int i = 0; i < NUM_BINDNAMES; i++)
	{
        menuaction_s *action = &s_keys_actions[i];
        action->generic.type = bindnames[i][0] != NULL && bindnames[i][1] != NULL ? MTYPE_ACTION : MTYPE_SEPARATOR;
        action->generic.flags = QMF_GRAYED;
        action->generic.x = 0;
        action->generic.y = ((i % KEYS_PER_PAGE) * 9);
        action->generic.ownerdraw = DrawKeyBindingFunc;
        action->generic.localdata[0] = i;
        action->generic.name = bindnames[i][1];
        Menu_AddItem(menu, (void *)action);
	}

    MenuKeys_resetStatusBar(menu);
	Menu_Center(menu);
}

static void Keys_MenuDraw()
{
	menuframework_s *menu = &s_keys_menu;
	Menu_AdjustCursor(menu, 1);
	Menu_Draw(menu);
}

static const char* Keys_MenuKey(int key, int keyUnmodified)
{
	menuframework_s *menu = &s_keys_menu;
	menuaction_s *item = (menuaction_s *)Menu_ItemAtCursor(menu);

	if (bind_grab)
	{
		if (keyUnmodified > 0 && keyUnmodified != K_GAMEPAD_SELECT && keyUnmodified != K_ESCAPE)
		{
			char cmd[1024];
			Com_sprintf(cmd, sizeof(cmd), "bind \"%s\" \"%s\"\n", Key_KeynumToString(keyUnmodified), bindnames[item->generic.localdata[0]][0]);
			Cbuf_InsertText(cmd);
		}

        MenuKeys_resetStatusBar(menu);
		bind_grab = false;
		return menu_out_sound;
	}

	switch (key)
	{
	case K_GAMEPAD_A:
	case K_JOY1:
	case K_KP_ENTER:
	case K_ENTER:
		KeyBindingFunc(item);
		return menu_in_sound;

	case K_GAMEPAD_Y:
	case K_JOY3:
	case K_BACKSPACE: /* delete bindings */
	case K_DEL: /* delete bindings */
		M_UnbindCommand(bindnames[item->generic.localdata[0]][0]);
		return menu_out_sound;

	case K_GAMEPAD_LEFT:
	case K_LEFTARROW:
		menu->cursor -= menu->itemPerPageNb;
		if (menu->cursor < 0)
			menu->cursor = 0;
		Menu_AdjustCursor(menu, -1);
		return menu_in_sound;

	case K_GAMEPAD_RIGHT:
	case K_RIGHTARROW:
		menu->cursor += menu->itemPerPageNb;
		if (menu->cursor >= menu->nitems)
			menu->cursor = menu->nitems - 1;
		Menu_AdjustCursor(menu, +1);
		return menu_in_sound;

	default:
		return Default_MenuKey(menu, key, keyUnmodified);
	}
}

void MenuKeys_enter()
{
	Keys_MenuInit();
	M_PushMenu(Keys_MenuDraw, Keys_MenuKey);
}
