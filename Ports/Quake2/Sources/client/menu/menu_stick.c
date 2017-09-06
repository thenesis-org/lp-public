#include "client/menu/menu.h"

static menuframework_s MenuStick_menu;

//----------------------------------------
// Joystick enabled.
//----------------------------------------
static menulist_s MenuStick_stickEnabled_list;

static void MenuStick_stickEnabled_apply()
{
	menulist_s *list = &MenuStick_stickEnabled_list;
	Cvar_SetValue("stick_enabled", list->curvalue != 0);
}

static void MenuStick_stickEnabled_callback(void *unused)
{
	MenuStick_stickEnabled_apply();
}

static int MenuStick_stickEnabled_init(int y)
{
	static const char *itemNames[] =
	{
		"no",
		"yes",
		0
	};
	menulist_s *list = &MenuStick_stickEnabled_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "stick enabled";
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.callback = MenuStick_stickEnabled_callback;
	list->itemnames = itemNames;
	list->curvalue = stick_enabled->value != 0;
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuStick_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Invert stick pitch.
//----------------------------------------
static menulist_s  MenuStick_stickPitchInverted_list;

static void  MenuStick_stickPitchInverted_apply()
{
	menulist_s *list = & MenuStick_stickPitchInverted_list;
	Cvar_SetValue("stick_pitch_inverted", list->curvalue);
}

static void  MenuStick_stickPitchInverted_callback(void *unused)
{
	 MenuStick_stickPitchInverted_apply();
}

static int  MenuStick_stickPitchInverted_init(int y)
{
	static const char *itemNames[] =
	{
		"no",
		"yes",
		0
	};

	menulist_s *list = & MenuStick_stickPitchInverted_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.name = "invert stick pitch";
	list->generic.callback =  MenuStick_stickPitchInverted_callback;
	list->itemnames = itemNames;
	list->curvalue = (stick_pitch_inverted->value != 0);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuStick_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Stick sensivity.
//----------------------------------------
static menuslider_s MenuStick_stickSensitivity_slider;

static void MenuStick_stickSensitivity_apply()
{
	menuslider_s *slider = &MenuStick_stickSensitivity_slider;
	Cvar_SetValue("stick_sensitivity", slider->curvalue / 10.0f);
}

static void MenuStick_stickSensitivity_callback(void *unused)
{
	MenuStick_stickSensitivity_apply();
}

static int MenuStick_stickSensitivity_init(int y)
{
	menuslider_s *slider = &MenuStick_stickSensitivity_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = "stick speed";
	slider->generic.callback = MenuStick_stickSensitivity_callback;
	slider->minvalue = 5;
	slider->maxvalue = 20;
	slider->curvalue = stick_sensitivity->value * 10.0f;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuStick_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// Stick curve.
//----------------------------------------
static menuslider_s MenuStick_stickCurve_slider;

static void MenuStick_stickCurve_apply()
{
	menuslider_s *slider = &MenuStick_stickCurve_slider;
	Cvar_SetValue("stick_curve", slider->curvalue / 10.0f);
}

static void MenuStick_stickCurve_callback(void *unused)
{
	MenuStick_stickCurve_apply();
}

static int MenuStick_stickCurve_init(int y)
{
	menuslider_s *slider = &MenuStick_stickCurve_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = "stick curve";
	slider->generic.callback = MenuStick_stickCurve_callback;
	slider->minvalue = 5;
	slider->maxvalue = 25;
	slider->curvalue = stick_curve->value * 10.0f;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuStick_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// Stick dead zone.
//----------------------------------------
static menuslider_s MenuStick_stickDeadZone_slider;

static void MenuStick_stickDeadZone_apply()
{
	menuslider_s *slider = &MenuStick_stickDeadZone_slider;
	Cvar_SetValue("stick_deadzone", slider->curvalue / 20.0f);
}

static void MenuStick_stickDeadZone_callback(void *unused)
{
	MenuStick_stickDeadZone_apply();
}

static int MenuStick_stickDeadZone_init(int y)
{
	menuslider_s *slider = &MenuStick_stickDeadZone_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = "stick deadzone";
	slider->generic.callback = MenuStick_stickDeadZone_callback;
	slider->minvalue = 0;
	slider->maxvalue = 20;
	slider->curvalue = stick_deadzone->value * 20.0f;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuStick_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// MenuStick.
//----------------------------------------
static void MenuStick_init()
{
	int y = 0;

	menuframework_s *menu = &MenuStick_menu;
	memset(menu, 0, sizeof(menuframework_s));
	menu->x = viddef.width / 2;
	menu->nitems = 0;

	y = MenuStick_stickEnabled_init(y);
	y = MenuStick_stickPitchInverted_init(y);
	y = MenuStick_stickSensitivity_init(y);
	y = MenuStick_stickCurve_init(y);
	y = MenuStick_stickDeadZone_init(y);

	Menu_CenterWithBanner(&MenuStick_menu, "m_banner_options");
}

static void MenuStick_draw()
{
	M_Banner("m_banner_options");
	Menu_AdjustCursor(&MenuStick_menu, 1);
	Menu_Draw(&MenuStick_menu);
	M_Popup();
}

static const char* MenuStick_key(int key, int keyUnmodified)
{
	if (m_popup_string)
	{
		m_popup_string = NULL;
		return NULL;
	}
	return Default_MenuKey(&MenuStick_menu, key, keyUnmodified);
}

void MenuStick_enter()
{
	MenuStick_init();
	M_PushMenu(MenuStick_draw, MenuStick_key);
}
