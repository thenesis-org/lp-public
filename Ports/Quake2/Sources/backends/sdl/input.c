/*
 * Copyright (C) 2010 Yamagi Burmeister
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * This is the Quake II input system backend, implemented with SDL.
 *
 * =======================================================================
 */

#include "backends/generic/input.h"
#include "client/client.h"
#include "client/keyboard.h"
#include "client/refresh/local.h"

#include "SDL/SDLWrapper.h"

#include <SDL2/SDL.h>

#define MOUSE_MAX 3000
#define MOUSE_MIN 40

/* Globals */
static int mouse_x, mouse_y;
static int old_mouse_x, old_mouse_y;
static qboolean mlooking;
static SDL_Joystick *joystick = NULL;
static SDL_GameController *controller = NULL;

/* CVars */
cvar_t *vid_fullscreen;
static cvar_t *in_grab;
static cvar_t *exponential_speedup;
cvar_t *freelook;
cvar_t *lookstrafe;
cvar_t *m_forward;
static cvar_t *m_filter;
cvar_t *m_pitch;
cvar_t *m_side;
cvar_t *m_yaw;
cvar_t *sensitivity;
static cvar_t *windowed_mouse;

static int IN_TranslateSDLtoQ2Key(unsigned int keysym)
{
	int key = 0;

	/* These must be translated */
	switch (keysym)
	{
		#if defined(__GCW_ZERO__)

	case SDLK_RETURN:
		key = K_GAMEPAD_START;
		break;
	case SDLK_ESCAPE:
		key = K_GAMEPAD_SELECT;
		break;

	case SDLK_LCTRL:
		key = K_GAMEPAD_A;
		break;
	case SDLK_LALT:
		key = K_GAMEPAD_B;
		break;
	case SDLK_LSHIFT:
		key = K_GAMEPAD_X;
		break;
	case SDLK_SPACE:
		key = K_GAMEPAD_Y;
		break;

	case SDLK_TAB:
		key = K_GAMEPAD_L;
		break;
	case SDLK_BACKSPACE:
		key = K_GAMEPAD_R;
		break;

	case SDLK_LEFT:
		key = K_GAMEPAD_LEFT;
		break;
	case SDLK_RIGHT:
		key = K_GAMEPAD_RIGHT;
		break;
	case SDLK_DOWN:
		key = K_GAMEPAD_DOWN;
		break;
	case SDLK_UP:
		key = K_GAMEPAD_UP;
		break;

	case SDLK_PAUSE:
		key = K_GAMEPAD_LOCK;
		break;
	case SDLK_HOME:
		key = K_GAMEPAD_POWER;
		break;

		#else

	case SDLK_PAGEUP:
		key = K_PGUP;
		break;
	case SDLK_KP_9:
		key = K_KP_PGUP;
		break;
	case SDLK_PAGEDOWN:
		key = K_PGDN;
		break;
	case SDLK_KP_3:
		key = K_KP_PGDN;
		break;
	case SDLK_KP_7:
		key = K_KP_HOME;
		break;
	case SDLK_HOME:
		key = K_HOME;
		break;
	case SDLK_KP_1:
		key = K_KP_END;
		break;
	case SDLK_END:
		key = K_END;
		break;
	case SDLK_KP_4:
		key = K_KP_LEFTARROW;
		break;
	case SDLK_LEFT:
		key = K_LEFTARROW;
		break;
	case SDLK_KP_6:
		key = K_KP_RIGHTARROW;
		break;
	case SDLK_RIGHT:
		key = K_RIGHTARROW;
		break;
	case SDLK_KP_2:
		key = K_KP_DOWNARROW;
		break;
	case SDLK_DOWN:
		key = K_DOWNARROW;
		break;
	case SDLK_KP_8:
		key = K_KP_UPARROW;
		break;
	case SDLK_UP:
		key = K_UPARROW;
		break;
	case SDLK_ESCAPE:
		key = K_ESCAPE;
		break;
	case SDLK_KP_ENTER:
		key = K_KP_ENTER;
		break;
	case SDLK_RETURN:
		key = K_ENTER;
		break;
	case SDLK_TAB:
		key = K_TAB;
		break;
	case SDLK_F1:
		key = K_F1;
		break;
	case SDLK_F2:
		key = K_F2;
		break;
	case SDLK_F3:
		key = K_F3;
		break;
	case SDLK_F4:
		key = K_F4;
		break;
	case SDLK_F5:
		key = K_F5;
		break;
	case SDLK_F6:
		key = K_F6;
		break;
	case SDLK_F7:
		key = K_F7;
		break;
	case SDLK_F8:
		key = K_F8;
		break;
	case SDLK_F9:
		key = K_F9;
		break;
	case SDLK_F10:
		key = K_F10;
		break;
	case SDLK_F11:
		key = K_F11;
		break;
	case SDLK_F12:
		key = K_F12;
		break;
	case SDLK_F13:
		key = K_F13;
		break;
	case SDLK_F14:
		key = K_F14;
		break;
	case SDLK_F15:
		key = K_F15;
		break;
	case SDLK_BACKSPACE:
		key = K_BACKSPACE;
		break;
	case SDLK_KP_PERIOD:
		key = K_KP_DEL;
		break;
	case SDLK_DELETE:
		key = K_DEL;
		break;
	case SDLK_PAUSE:
		key = K_PAUSE;
		break;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
		key = K_SHIFT;
		break;
	case SDLK_LCTRL:
	case SDLK_RCTRL:
		key = K_CTRL;
		break;
	case SDLK_RGUI:
	case SDLK_LGUI:
		key = K_COMMAND;
		break;
	case SDLK_RALT:
	case SDLK_LALT:
		key = K_ALT;
		break;
	case SDLK_KP_5:
		key = K_KP_5;
		break;
	case SDLK_INSERT:
		key = K_INS;
		break;
	case SDLK_KP_0:
		key = K_KP_INS;
		break;
	case SDLK_KP_MULTIPLY:
		key = K_KP_STAR;
		break;
	case SDLK_KP_PLUS:
		key = K_KP_PLUS;
		break;
	case SDLK_KP_MINUS:
		key = K_KP_MINUS;
		break;
	case SDLK_KP_DIVIDE:
		key = K_KP_SLASH;
		break;
	case SDLK_MODE:
		key = K_MODE;
		break;
	case SDLK_APPLICATION:
		key = K_COMPOSE;
		break;
	case SDLK_HELP:
		key = K_HELP;
		break;
	case SDLK_PRINTSCREEN:
		key = K_PRINT;
		break;
	case SDLK_SYSREQ:
		key = K_SYSREQ;
		break;
	case SDLK_MENU:
		key = K_MENU;
		break;
	case SDLK_POWER:
		key = K_POWER;
		break;
	case SDLK_UNDO:
		key = K_UNDO;
		break;
	case SDLK_SCROLLLOCK:
		key = K_SCROLLOCK;
		break;
	case SDLK_NUMLOCKCLEAR:
		key = K_KP_NUMLOCK;
		break;
	case SDLK_CAPSLOCK:
		key = K_CAPSLOCK;
		break;

		#endif

	default:
		break;
	}

	return key;
}

bool IN_processEvent(SDL_Event *event)
{
	unsigned int key = 0;
	switch (event->type)
	{
	case SDL_QUIT:
		printf("Exit requested by the system.");
		sdlwRequestExit(true);
		break;

	case SDL_WINDOWEVENT:
		switch (event->window.event)
		{
		case SDL_WINDOWEVENT_CLOSE:
			printf("Exit requested by the user (by closing the window).");
			sdlwRequestExit(true);
			break;
		case SDL_WINDOWEVENT_RESIZED:
			//sdlwResize(event->window.data1, event->window.data2);
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			Key_MarkAllUp();
			break;
		}
		break;

	case SDL_MOUSEWHEEL:
		Key_Event((event->wheel.y > 0 ? K_MWHEELUP : K_MWHEELDOWN), true, true);
		Key_Event((event->wheel.y > 0 ? K_MWHEELUP : K_MWHEELDOWN), false, true);
		break;
	case SDL_MOUSEBUTTONDOWN:
	// fall-through
	case SDL_MOUSEBUTTONUP:
		switch (event->button.button)
		{
		case SDL_BUTTON_LEFT:
			key = K_MOUSE1;
			break;
		case SDL_BUTTON_MIDDLE:
			key = K_MOUSE3;
			break;
		case SDL_BUTTON_RIGHT:
			key = K_MOUSE2;
			break;
		case SDL_BUTTON_X1:
			key = K_MOUSE4;
			break;
		case SDL_BUTTON_X2:
			key = K_MOUSE5;
			break;
		}
		Key_Event(key, (event->type == SDL_MOUSEBUTTONDOWN), true);
		break;
	case SDL_MOUSEMOTION:
		if (cls.key_dest == key_game && (int)cl_paused->value == 0)
		{
			mouse_x += event->motion.xrel;
			mouse_y += event->motion.yrel;
		}
		break;

		#if !defined(__GCW_ZERO__)
	// FIXME: Numpad keys are both send here and to SDL_KEYDOWN. As a result, two keys are sent in the menus which causes trouble.
	case SDL_TEXTINPUT:
		if ((event->text.text[0] >= ' ') && (event->text.text[0] <= '~'))
		{
			Char_Event(event->text.text[0]);
		}
		break;
		#endif

	case SDL_KEYDOWN:
	/* fall-through */
	case SDL_KEYUP:
	{
		qboolean down = (event->type == SDL_KEYDOWN);
		#if defined(__GCW_ZERO__)
		Key_Event(IN_TranslateSDLtoQ2Key(event->key.keysym.sym), down, true);
		#else
		/* workaround for AZERTY-keyboards, which don't have 1, 2, ..., 9, 0 in first row:
		 * always map those physical keys (scancodes) to those keycodes anyway
		 * see also https://bugzilla.libsdl.org/show_bug.cgi?id=3188 */
		SDL_Scancode sc = event->key.keysym.scancode;
		if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0)
		{
			/* Note that the SDL_SCANCODEs are SDL_SCANCODE_1, _2, ..., _9, SDL_SCANCODE_0
			 * while in ASCII it's '0', '1', ..., '9' => handle 0 and 1-9 separately
			 * (quake2 uses the ASCII values for those keys) */
			int key = '0'; /* implicitly handles SDL_SCANCODE_0 */
			if (sc <= SDL_SCANCODE_9)
			{
				key = '1' + (sc - SDL_SCANCODE_1);
			}
			Key_Event(key, down, false);
		}
		else
		if ((event->key.keysym.sym >= SDLK_SPACE) && (event->key.keysym.sym < SDLK_DELETE))
		{
			Key_Event(event->key.keysym.sym, down, false);
		}
		else
		{
			Key_Event(IN_TranslateSDLtoQ2Key(event->key.keysym.sym), down, true);
		}
		#endif
	}
	break;

	case SDL_JOYAXISMOTION:
		break;
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
	{
		qboolean down = (event->type == SDL_JOYBUTTONDOWN);
		Key_Event(K_JOY1 + event->jbutton.button, down, true);
	}
	break;
	case SDL_JOYHATMOTION:
	{
		int v = event->jhat.value;
		bool left = (v == SDL_HAT_LEFTDOWN || v == SDL_HAT_LEFT || v == SDL_HAT_LEFTUP);
		bool right = (v == SDL_HAT_RIGHTDOWN || v == SDL_HAT_RIGHT || v == SDL_HAT_RIGHTUP);
		bool down = (v == SDL_HAT_LEFTDOWN || v == SDL_HAT_DOWN || v == SDL_HAT_RIGHTDOWN);
		bool up = (v == SDL_HAT_LEFTUP || v == SDL_HAT_UP || v == SDL_HAT_RIGHTUP);
		Key_Event(K_GAMEPAD_LEFT, left, true);
		Key_Event(K_GAMEPAD_RIGHT, right, true);
		Key_Event(K_GAMEPAD_DOWN, down, true);
		Key_Event(K_GAMEPAD_UP, up, true);
	}
	break;

	case SDL_CONTROLLERAXISMOTION:
		break;
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
	{
		qboolean down = (event->type == SDL_CONTROLLERBUTTONDOWN);
		int key;
		switch (event->cbutton.button)
		{
		default: key = -1; break;
		case SDL_CONTROLLER_BUTTON_A: key = K_GAMEPAD_A; break;
		case SDL_CONTROLLER_BUTTON_B: key = K_GAMEPAD_B; break;
		case SDL_CONTROLLER_BUTTON_X: key = K_GAMEPAD_X; break;
		case SDL_CONTROLLER_BUTTON_Y: key = K_GAMEPAD_Y; break;
		//case SDL_CONTROLLER_BUTTON_BACK: key = K_GAMEPAD_; break;
		case SDL_CONTROLLER_BUTTON_GUIDE: key = K_GAMEPAD_SELECT; break;
		case SDL_CONTROLLER_BUTTON_START: key = K_GAMEPAD_START; break;
		//case SDL_CONTROLLER_BUTTON_LEFTSTICK: key = K_GAMEPAD_; break;
		//case SDL_CONTROLLER_BUTTON_RIGHTSTICK: key = K_GAMEPAD_; break;
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: key = K_GAMEPAD_L; break;
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: key = K_GAMEPAD_R; break;
		case SDL_CONTROLLER_BUTTON_DPAD_UP: key = K_GAMEPAD_UP; break;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN: key = K_GAMEPAD_DOWN; break;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT: key = K_GAMEPAD_LEFT; break;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: key = K_GAMEPAD_RIGHT; break;
		}
		if (key >= 0)
			Key_Event(key, down, true);
	}
	break;
	}
	return true;
}

/*
 * Updates the input queue state. Called every
 * frame by the client and does nearly all the
 * input magic.
 */
void IN_Update()
{
	sdlwCheckEvents();

	// Grab and ungrab the mouse if the  console or the menu is opened.
	qboolean want_grab = (vid_fullscreen->value || in_grab->value == 1 || (in_grab->value == 2 && windowed_mouse->value));
	In_Grab(want_grab);
}

static float ComputeStickValue(float stickValue)
{
	float deadzone = stick_deadzone->value * (1.0f / 2.0f);
	if (stickValue < 0.0f)
	{
		if (stickValue > -deadzone)
			stickValue = 0.0f;
		else
			stickValue = (stickValue + deadzone) / (1.0f - deadzone); // Normalize.
	}
	else
	{
		if (stickValue < deadzone)
			stickValue = 0.0f;
		else
			stickValue = (stickValue - deadzone) / (1.0f - deadzone); // Normalize.
	}

	// for stick_curve, 0.5 gives quick response, 1.0 is linear, 2.0 gives slow responses.
	if (stickValue < 0.0f)
		stickValue = -powf(-stickValue, stick_curve->value);
	else
		stickValue = +powf(+stickValue, stick_curve->value);

	return stickValue * stick_sensitivity->value;
}

/*
 * Move handling
 */
void IN_Move(usercmd_t *cmd)
{
	if (m_filter->value)
	{
		if ((mouse_x > 1) || (mouse_x < -1))
		{
			mouse_x = (mouse_x + old_mouse_x) * 0.5f;
		}

		if ((mouse_y > 1) || (mouse_y < -1))
		{
			mouse_y = (mouse_y + old_mouse_y) * 0.5f;
		}
	}

	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;

	//	if (mouse_x || mouse_y) // Always update now, for joystick.
	{
		if (!exponential_speedup->value)
		{
			mouse_x *= sensitivity->value;
			mouse_y *= sensitivity->value;
		}
		else
		{
			if ((mouse_x > MOUSE_MIN) || (mouse_y > MOUSE_MIN) || (mouse_x < -MOUSE_MIN) || (mouse_y < -MOUSE_MIN))
			{
				mouse_x = (mouse_x * mouse_x * mouse_x) / 4;
				mouse_y = (mouse_y * mouse_y * mouse_y) / 4;

				if (mouse_x > MOUSE_MAX)
					mouse_x = MOUSE_MAX;
				else
				if (mouse_x < -MOUSE_MAX)
					mouse_x = -MOUSE_MAX;

				if (mouse_y > MOUSE_MAX)
					mouse_y = MOUSE_MAX;
				else
				if (mouse_y < -MOUSE_MAX)
					mouse_y = -MOUSE_MAX;
			}
		}

		float speed;
		if (in_speed.state & 1)
		{
			speed = cls.frametime * cl_anglespeedkey->value;
		}
		else
		{
			speed = cls.frametime;
		}

		float running = 1.0f;
		if ((in_speed.state & 1) ^ (int)(cl_run->value))
		{
			running = 2.0f;
		}

		float joyXFloat = 0.0f, joyYFloat = 0.0f;

		if (joystick != NULL && joystick_enabled->value)
		{
			float joyX, joyY;
			joyX = (float)SDL_JoystickGetAxis(joystick, 0) / 32768.0f;
			joyY = (float)SDL_JoystickGetAxis(joystick, 1) / 32768.0f;
			joyXFloat = ComputeStickValue(joyX);
			joyYFloat = ComputeStickValue(joyY);
		}

		if (controller != NULL)
		{
			float joyX, joyY;

			joyX = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) / 32768.0f;;
			joyY = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY) / 32768.0f;;
			joyXFloat += ComputeStickValue(joyX);
			joyYFloat += ComputeStickValue(joyY);

			joyX = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX) / 32768.0f;;
			joyY = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY) / 32768.0f;;
			joyX = ComputeStickValue(joyX);
			joyY = ComputeStickValue(joyY);
			cmd->sidemove += cl_sidespeed->value * joyX * running;
			cmd->forwardmove += cl_forwardspeed->value * joyY * running;
		}

		/* add mouse X/Y movement to cmd */
		if ((in_strafe.state & 1) || (lookstrafe->value && mlooking))
		{
			cmd->sidemove += m_side->value * mouse_x;
			cmd->sidemove += cl_sidespeed->value * joyXFloat * running;
		}
		else
		{
			cl.viewangles[YAW] -= m_yaw->value * mouse_x;
			cl.viewangles[YAW] -= speed * cl_yawspeed->value * joyXFloat;
		}

		if ((mlooking || freelook->value) && !(in_strafe.state & 1))
		{
			cl.viewangles[PITCH] += m_pitch->value * mouse_y;
			cl.viewangles[PITCH] += speed * cl_pitchspeed->value * joyYFloat;
		}
		else
		{
			cmd->forwardmove -= m_forward->value * mouse_y;
			cmd->forwardmove += cl_forwardspeed->value * joyYFloat * running;
		}

		mouse_x = mouse_y = 0;
	}
}

/* ------------------------------------------------------------------ */

/*
 * Look down
 */
static void IN_MLookDown()
{
	mlooking = true;
}

/*
 * Look up
 */
static void IN_MLookUp()
{
	mlooking = false;
	IN_CenterView();
}

void IN_Init()
{
	Com_Printf("------- input initialization -------\n");

	mouse_x = mouse_y = 0;

	exponential_speedup = Cvar_Get("exponential_speedup", "0", CVAR_ARCHIVE);
	freelook = Cvar_Get("freelook", "1", 0);
	in_grab = Cvar_Get("in_grab", "2", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get("lookstrafe", "0", 0);
	m_filter = Cvar_Get("m_filter", "0", CVAR_ARCHIVE);
	m_forward = Cvar_Get("m_forward", "1", 0);
	m_pitch = Cvar_Get("m_pitch", "0.022", 0);
	m_side = Cvar_Get("m_side", "0.8", 0);
	m_yaw = Cvar_Get("m_yaw", "0.022", 0);
	sensitivity = Cvar_Get("sensitivity", "3", 0);
	vid_fullscreen = Cvar_Get("vid_fullscreen", GL_FULLSCREEN_DEFAULT_STRING, CVAR_ARCHIVE);
	windowed_mouse = Cvar_Get("windowed_mouse", GL_WINDOWED_MOUSE_DEFAULT_STRING, CVAR_USERINFO | CVAR_ARCHIVE);

	Cmd_AddCommand("+mlook", IN_MLookDown);
	Cmd_AddCommand("-mlook", IN_MLookUp);

	SDL_StartTextInput();

	if (!SDL_WasInit(SDL_INIT_JOYSTICK))
	{
		if (SDL_Init(SDL_INIT_JOYSTICK) == -1)
		{
			VID_Printf(PRINT_ALL, "Couldn't init SDL joystick: %s.\n", SDL_GetError());
		}
		else
		{
			if (SDL_NumJoysticks() > 0)
			{
				int n = SDL_NumJoysticks();
				for (int i = 0; i < n; i++)
				{
					if (SDL_IsGameController(i))
					{
						if (!controller)
						{
							controller = SDL_GameControllerOpen(i);
							if (!controller)
							{
								VID_Printf(PRINT_ALL, "Could not open gamecontroller %i: %s\n", i, SDL_GetError());
							}
						}
					}
					else
					{
						if (!joystick)
						{
							joystick = SDL_JoystickOpen(i);
							if (!joystick)
							{
								VID_Printf(PRINT_ALL, "Could not open joystick %i: %s\n", i, SDL_GetError());
							}
						}
					}
				}
			}
		}
	}

	Com_Printf("------------------------------------\n\n");
}

void IN_Shutdown()
{
	Cmd_RemoveCommand("force_centerview");
	Cmd_RemoveCommand("+mlook");
	Cmd_RemoveCommand("-mlook");

	if (SDL_WasInit(SDL_INIT_JOYSTICK))
	{
		if (SDL_JoystickGetAttached(joystick))
		{
			SDL_JoystickClose(joystick);
			joystick = NULL;
		}
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
	}

	Com_Printf("Shutting down input.\n");
}

void In_Grab(qboolean grab)
{
	if (sdlwContext->window != NULL)
	{
		SDL_SetWindowGrab(sdlwContext->window, grab ? SDL_TRUE : SDL_FALSE);
	}
	if (SDL_SetRelativeMouseMode(grab ? SDL_TRUE : SDL_FALSE) < 0)
	{
		VID_Printf(PRINT_ALL, "Setting relative mouse mode failed, reason: %s\nYou should probably update to SDL 2.0.3 or newer\n", SDL_GetError());
	}
}
