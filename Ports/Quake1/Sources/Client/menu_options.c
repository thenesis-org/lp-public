#include "Client/client.h"
#include "Client/console.h"
#include "Client/menu.h"
#include "Client/screen.h"
#include "Client/view.h"
#include "Common/cmd.h"
#include "Common/quakedef.h"

#define OPTIONS_ITEMS 13
static int M_Options_cursor;

#define SLIDER_RANGE 10

void M_Options_enter()
{
	key_dest = key_menu;
	m_state = m_options;
	m_entersound = true;
}

static void M_Options_adjustSliders(int dir)
{
	S_LocalSound("misc/menu3.wav");

	switch (M_Options_cursor)
	{
	case 3: // screen size
		scr_viewsize.value += dir * 10;
		if (scr_viewsize.value < 30)
			scr_viewsize.value = 30;
		if (scr_viewsize.value > 120)
			scr_viewsize.value = 120;
		Cvar_SetValue("viewsize", scr_viewsize.value);
		break;
	case 4: // gamma
		v_gamma.value -= dir * 0.05f;
		if (v_gamma.value < 0.5f)
			v_gamma.value = 0.5f;
		if (v_gamma.value > 1)
			v_gamma.value = 1;
		Cvar_SetValue("gamma", v_gamma.value);
		break;
	case 5: // mouse speed
		mouse_linear_sensitivity.value += dir * 0.5f;
		if (mouse_linear_sensitivity.value < 1)
			mouse_linear_sensitivity.value = 1;
		if (mouse_linear_sensitivity.value > 11)
			mouse_linear_sensitivity.value = 11;
		Cvar_SetValue("mouse_linear_sensitivity", mouse_linear_sensitivity.value);
		break;
	case 6: // music volume
		#if 0 // defined(_WIN32)
		s_bgmvolume.value += dir * 1.0f;
		#else
		s_bgmvolume.value += dir * 0.1f;
		#endif
		if (s_bgmvolume.value < 0)
			s_bgmvolume.value = 0;
		if (s_bgmvolume.value > 1)
			s_bgmvolume.value = 1;
		Cvar_SetValue("s_bgmvolume", s_bgmvolume.value);
		break;
	case 7: // sfx volume
		s_volume.value += dir * 0.1f;
		if (s_volume.value < 0)
			s_volume.value = 0;
		if (s_volume.value > 1)
			s_volume.value = 1;
		Cvar_SetValue("s_volume", s_volume.value);
		break;

	case 8: // allways run
  		Cvar_SetValue("cl_run", !cl_run.value);
		break;

	case 9: // invert mouse
		Cvar_SetValue("mouse_speed_pitch", -mouse_speed_pitch.value);
		break;

	case 10: // input_lookspring
		Cvar_SetValue("input_lookspring", !input_lookspring.value);
		break;

	case 11: // input_lookstrafe
		Cvar_SetValue("input_lookstrafe", !input_lookstrafe.value);
		break;

	case 12: // input_freelook
		Cvar_SetValue("input_freelook", !input_freelook.value);
		break;
	}
}

static void M_Options_drawSlider(int x, int y, float range)
{
	if (range < 0)
		range = 0;
	if (range > 1)
		range = 1;
	M_DrawCharacter(x - 8, y, 128);
    int i;
	for (i = 0; i < SLIDER_RANGE; i++)
		M_DrawCharacter(x + i * 8, y, 129);
	M_DrawCharacter(x + i * 8, y, 130);
	M_DrawCharacter(x + (SLIDER_RANGE - 1) * 8 * range, y, 131);
}

void M_DrawCheckbox(int x, int y, int on)
{
	if (on)
		M_Print(x, y, "on");
	else
		M_Print(x, y, "off");
}

void M_Options_draw()
{
	float r;
	qpic_t *p;

	M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/p_option.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

    int y = 32;

	M_Print(16, y, "    Customize controls");
    y+=8;
	M_Print(16, y, "         Go to console");
    y+=8;
	M_Print(16, y, "     Reset to defaults");
    y+=8;

	M_Print(16, y, "           Screen size");
	r = (scr_viewsize.value - 30) / (120 - 30);
	M_Options_drawSlider(220, y, r);
    y+=8;

	M_Print(16, y, "            Brightness");
	r = (1.0 - v_gamma.value) / 0.5f;
	M_Options_drawSlider(220, y, r);
    y+=8;

	M_Print(16, y, "           Mouse Speed");
	r = (mouse_linear_sensitivity.value - 1) / 10;
	M_Options_drawSlider(220, y, r);
    y+=8;

	M_Print(16, y, "       CD Music Volume");
	r = s_bgmvolume.value;
	M_Options_drawSlider(220, y, r);
    y+=8;

	M_Print(16, y, "          Sound Volume");
	r = s_volume.value;
	M_Options_drawSlider(220, y, r);
    y+=8;

	M_Print(16, y, "            Always Run");
	M_DrawCheckbox(220, y, cl_run.value);
    y+=8;

	M_Print(16, y, "          Invert Mouse");
	M_DrawCheckbox(220, y, mouse_speed_pitch.value < 0);
    y+=8;

	M_Print(16, y, "            Lookspring");
	M_DrawCheckbox(220, y, input_lookspring.value);
    y+=8;

	M_Print(16, y, "            Lookstrafe");
	M_DrawCheckbox(220, y, input_lookstrafe.value);
    y+=8;

	M_Print(16, y, "              Freelook");
	M_DrawCheckbox(220, y, input_freelook.value);
    y+=8;

	// cursor
	M_DrawCharacter(200, 32 + M_Options_cursor * 8, 12 + ((int)(realtime * 4) & 1));
}

void M_Options_onKey(int k)
{
	switch (k)
	{
    case K_GAMEPAD_SELECT:
	case K_ESCAPE:
		M_Main_enter();
		break;

	case K_GAMEPAD_START:
	case K_GAMEPAD_A:
	case K_JOY1:
	case K_KP_ENTER:
	case K_ENTER:
		m_entersound = true;
		switch (M_Options_cursor)
		{
		case 0:
			M_Keys_enter();
			break;
		case 1:
			m_state = m_none;
			Con_ToggleConsole_f();
			break;
		case 2:
			Cbuf_AddText("exec default.cfg\n");
			break;
		default:
			M_Options_adjustSliders(1);
			break;
		}
		return;

	case K_GAMEPAD_UP:
	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		M_Options_cursor--;
		if (M_Options_cursor < 0)
			M_Options_cursor = OPTIONS_ITEMS - 1;
		break;

	case K_GAMEPAD_DOWN:
	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		M_Options_cursor++;
		if (M_Options_cursor >= OPTIONS_ITEMS)
			M_Options_cursor = 0;
		break;

	case K_GAMEPAD_LEFT:
	case K_LEFTARROW:
		M_Options_adjustSliders(-1);
		break;

	case K_GAMEPAD_RIGHT:
	case K_RIGHTARROW:
		M_Options_adjustSliders(1);
		break;
	}
}
