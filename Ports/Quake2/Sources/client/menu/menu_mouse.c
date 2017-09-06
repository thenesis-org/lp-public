#include "client/menu/menu.h"

#if !defined(GAMEPAD_ONLY)

static menuframework_s MenuMouse_menu;

//----------------------------------------
// Invert mouse pitch.
//----------------------------------------
static menulist_s MenuMouse_mousePitchInverted_list;

static void MenuMouse_mousePitchInverted_apply()
{
	menulist_s *list = & MenuMouse_mousePitchInverted_list;
	Cvar_SetValue("mouse_pitch_inverted", list->curvalue);
}

static void MenuMouse_mousePitchInverted_callback(void *unused)
{
	 MenuMouse_mousePitchInverted_apply();
}

static int MenuMouse_mousePitchInverted_init(int y)
{
	static const char *itemNames[] =
	{
		"no",
		"yes",
		0
	};

	menulist_s *list = & MenuMouse_mousePitchInverted_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.name = "invert mouse pitch";
	list->generic.callback = MenuMouse_mousePitchInverted_callback;
	list->itemnames = itemNames;
	list->curvalue = (mouse_pitch_inverted->value != 0);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuMouse_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Mouse linear sensivity.
//----------------------------------------
static menuslider_s MenuMouse_mouseLinearSensitivity_slider;

static void MenuMouse_mouseLinearSensitivity_apply()
{
	menuslider_s *slider = &MenuMouse_mouseLinearSensitivity_slider;
	Cvar_SetValue("mouse_linear_sensitivity", slider->curvalue / 2.0f);
}

static void MenuMouse_mouseLinearSensitivity_callback(void *unused)
{
	MenuMouse_mouseLinearSensitivity_apply();
}

static int MenuMouse_mouseLinearSensitivity_init(int y)
{
	menuslider_s *slider = &MenuMouse_mouseLinearSensitivity_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = "linear mouse sensivity";
	slider->generic.callback = MenuMouse_mouseLinearSensitivity_callback;
	slider->minvalue = 2;
	slider->maxvalue = 20;
	slider->curvalue = mouse_linear_sensitivity->value * 2;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuMouse_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// Forward mouse speed.
//----------------------------------------
static menuslider_s MenuMouse_mouseSpeedForward_slider;

static void MenuMouse_mouseSpeedForward_apply()
{
	menuslider_s *slider = &MenuMouse_mouseSpeedForward_slider;
	Cvar_SetValue("mouse_speed_forward", slider->curvalue / 10.0f);
}

static void MenuMouse_mouseSpeedForward_callback(void *unused)
{
	MenuMouse_mouseSpeedForward_apply();
}

static int MenuMouse_mouseSpeedForward_init(int y)
{
	menuslider_s *slider = &MenuMouse_mouseSpeedForward_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = "forward mouse speed";
	slider->generic.callback = MenuMouse_mouseSpeedForward_callback;
	slider->minvalue = 5;
	slider->maxvalue = 20;
	slider->curvalue = mouse_speed_forward->value * 10.0f;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuMouse_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// Side mouse speed.
//----------------------------------------
static menuslider_s MenuMouse_mouseSpeedSide_slider;

static void MenuMouse_mouseSpeedSide_apply()
{
	menuslider_s *slider = &MenuMouse_mouseSpeedSide_slider;
	Cvar_SetValue("mouse_speed_side", slider->curvalue / 10.0f);
}

static void MenuMouse_mouseSpeedSide_callback(void *unused)
{
	MenuMouse_mouseSpeedSide_apply();
}

static int MenuMouse_mouseSpeedSide_init(int y)
{
	menuslider_s *slider = &MenuMouse_mouseSpeedSide_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = "side mouse speed";
	slider->generic.callback = MenuMouse_mouseSpeedSide_callback;
	slider->minvalue = 1;
	slider->maxvalue = 20;
	slider->curvalue = mouse_speed_side->value * 10.0f;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuMouse_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// Yaw mouse speed.
//----------------------------------------
static menuslider_s MenuMouse_mouseSpeedYaw_slider;

static void MenuMouse_mouseSpeedYaw_apply()
{
	menuslider_s *slider = &MenuMouse_mouseSpeedYaw_slider;
	Cvar_SetValue("mouse_speed_yaw", slider->curvalue / 1000.0f);
}

static void MenuMouse_mouseSpeedYaw_callback(void *unused)
{
	MenuMouse_mouseSpeedYaw_apply();
}

static int MenuMouse_mouseSpeedYaw_init(int y)
{
	menuslider_s *slider = &MenuMouse_mouseSpeedYaw_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = "yaw mouse speed";
	slider->generic.callback = MenuMouse_mouseSpeedYaw_callback;
	slider->minvalue = 11;
	slider->maxvalue = 33;
	slider->curvalue = mouse_speed_yaw->value * 1000.0f;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuMouse_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// Pitch mouse speed.
//----------------------------------------
static menuslider_s MenuMouse_mouseSpeedPitch_slider;

static void MenuMouse_mouseSpeedPitch_apply()
{
	menuslider_s *slider = &MenuMouse_mouseSpeedPitch_slider;
	Cvar_SetValue("mouse_speed_pitch", slider->curvalue / 1000.0f);
}

static void MenuMouse_mouseSpeedPitch_callback(void *unused)
{
	MenuMouse_mouseSpeedPitch_apply();
}

static int MenuMouse_mouseSpeedPitch_init(int y)
{
	menuslider_s *slider = &MenuMouse_mouseSpeedPitch_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = "pitch mouse speed";
	slider->generic.callback = MenuMouse_mouseSpeedPitch_callback;
	slider->minvalue = 11;
	slider->maxvalue = 33;
	slider->curvalue = mouse_speed_pitch->value * 1000.0f;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuMouse_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// MenuMouse.
//----------------------------------------
static void MenuMouse_init()
{
	int y = 0;

	menuframework_s *menu = &MenuMouse_menu;
	memset(menu, 0, sizeof(menuframework_s));
	menu->x = viddef.width / 2;
	menu->nitems = 0;

	y = MenuMouse_mousePitchInverted_init(y);
	y = MenuMouse_mouseLinearSensitivity_init(y);
    y = MenuMouse_mouseSpeedForward_init(y);
    y = MenuMouse_mouseSpeedSide_init(y);
    y = MenuMouse_mouseSpeedYaw_init(y);
    y = MenuMouse_mouseSpeedPitch_init(y);

	Menu_CenterWithBanner(&MenuMouse_menu, "m_banner_options");
}

static void MenuMouse_draw()
{
	M_Banner("m_banner_options");
	Menu_AdjustCursor(&MenuMouse_menu, 1);
	Menu_Draw(&MenuMouse_menu);
	M_Popup();
}

static const char* MenuMouse_key(int key, int keyUnmodified)
{
	if (m_popup_string)
	{
		m_popup_string = NULL;
		return NULL;
	}
	return Default_MenuKey(&MenuMouse_menu, key, keyUnmodified);
}

void MenuMouse_enter()
{
	MenuMouse_init();
	M_PushMenu(MenuMouse_draw, MenuMouse_key);
}

#endif
