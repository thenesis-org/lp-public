#include "client/menu/menu.h"

static const char* M_Quit_Key(int key, int keyUnmodified)
{
	switch (key)
	{
	case K_GAMEPAD_SELECT:
	case K_GAMEPAD_B:
	case K_ESCAPE:
	case 'n':
	case 'N':
		M_PopMenu();
		break;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_KP_ENTER:
	case K_ENTER:
	case 'Y':
	case 'y':
		cls.key_dest = key_console;
		CL_Quit_f();
		break;

	default:
		break;
	}

	return NULL;
}

static void M_Quit_Draw()
{
	int w, h;
	Draw_GetPicSize(&w, &h, "quit");
	float scale = SCR_GetMenuScale();
	Draw_PicScaled((viddef.width - w * scale) / 2, (viddef.height - h * scale) / 2, "quit", scale);
}

void MenuQuit_enter()
{
	M_PushMenu(M_Quit_Draw, M_Quit_Key);
}
