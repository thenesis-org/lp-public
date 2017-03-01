#include "client/menu/menu.h"

static menuframework_s MenuOptions_menu;

//----------------------------------------
// Sound options.
//----------------------------------------
static menuaction_s MenuOptions_soundOptions_action;

static void MenuOptions_soundOptions_callback(void *unused)
{
	MenuSound_enter();
}

static int MenuOptions_soundOptions_init(int y)
{
	menuaction_s *action = &MenuOptions_soundOptions_action;
	action->generic.type = MTYPE_ACTION;
	action->generic.x = 0;
	action->generic.y = y;
	action->generic.name = "sound options";
	action->generic.callback = MenuOptions_soundOptions_callback;
	Menu_AddItem(&MenuOptions_menu, (void *)action);
	y += 10;
	return y;
}

//----------------------------------------
// Forward speed.
//----------------------------------------
static menuslider_s MenuOptions_forwardSpeed_slider;

static void MenuOptions_forwardSpeed_apply()
{
	menuslider_s *slider = &MenuOptions_forwardSpeed_slider;
	Cvar_SetValue("cl_forwardspeed", slider->curvalue * 10.0f);
}

static void MenuOptions_forwardSpeed_callback(void *unused)
{
	MenuOptions_forwardSpeed_apply();
}

static int MenuOptions_forwardSpeed_init(int y)
{
	menuslider_s *slider = &MenuOptions_forwardSpeed_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = "forward speed";
	slider->generic.callback = MenuOptions_forwardSpeed_callback;
	slider->minvalue = 10;
	slider->maxvalue = 30;
	slider->curvalue = cl_forwardspeed->value / 10;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuOptions_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// Side speed.
//----------------------------------------
static menuslider_s MenuOptions_sideSpeed_slider;

static void MenuOptions_sideSpeed_apply()
{
	menuslider_s *slider = &MenuOptions_sideSpeed_slider;
	Cvar_SetValue("cl_sidespeed", slider->curvalue * 10.0f);
}

static void MenuOptions_sideSpeed_callback(void *unused)
{
	MenuOptions_sideSpeed_apply();
}

static int MenuOptions_sideSpeed_init(int y)
{
	menuslider_s *slider = &MenuOptions_sideSpeed_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = "side speed";
	slider->generic.callback = MenuOptions_sideSpeed_callback;
	slider->minvalue = 10;
	slider->maxvalue = 30;
	slider->curvalue = cl_sidespeed->value / 10;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuOptions_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// Yaw speed.
//----------------------------------------
static menuslider_s MenuOptions_yawSpeed_slider;

static void MenuOptions_yawSpeed_apply()
{
	menuslider_s *slider = &MenuOptions_yawSpeed_slider;
	Cvar_SetValue("cl_yawspeed", slider->curvalue * 10.0f);
}

static void MenuOptions_yawSpeed_callback(void *unused)
{
	MenuOptions_yawSpeed_apply();
}

static int MenuOptions_yawSpeed_init(int y)
{
	menuslider_s *slider = &MenuOptions_yawSpeed_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = "yaw speed";
	slider->generic.callback = MenuOptions_yawSpeed_callback;
	slider->minvalue = 10;
	slider->maxvalue = 30;
	slider->curvalue = cl_yawspeed->value / 10;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuOptions_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// Pitch speed.
//----------------------------------------
static menuslider_s MenuOptions_pitchSpeed_slider;

static void MenuOptions_pitchSpeed_apply()
{
	menuslider_s *slider = &MenuOptions_pitchSpeed_slider;
	Cvar_SetValue("cl_pitchspeed", slider->curvalue * 10.0f);
}

static void MenuOptions_pitchSpeed_callback(void *unused)
{
	MenuOptions_pitchSpeed_apply();
}

static int MenuOptions_pitchSpeed_init(int y)
{
	menuslider_s *slider = &MenuOptions_pitchSpeed_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = "pitch speed";
	slider->generic.callback = MenuOptions_pitchSpeed_callback;
	slider->minvalue = 10;
	slider->maxvalue = 30;
	slider->curvalue = cl_pitchspeed->value / 10;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuOptions_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// Free look.
//----------------------------------------
static menulist_s MenuOptions_freeLook_list;

static void MenuOptions_freeLook_apply()
{
	menulist_s *list = &MenuOptions_freeLook_list;
	Cvar_SetValue("freelook", (float)list->curvalue);
}

static void MenuOptions_freeLook_callback(void *unused)
{
	MenuOptions_freeLook_apply();
}

static int MenuOptions_freeLook_init(int y)
{
	static const char *itemNames[] =
	{
		"no",
		"yes",
		0
	};

	menulist_s *list = &MenuOptions_freeLook_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.name = "free look";
	list->generic.callback = MenuOptions_freeLook_callback;
	list->itemnames = itemNames;
	list->curvalue = (freelook->value != 0);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuOptions_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Look spring.
//----------------------------------------
static menulist_s MenuOptions_lookSpring_list;

static void MenuOptions_lookSpring_apply()
{
	menulist_s *list = &MenuOptions_lookSpring_list;
	Cvar_SetValue("lookspring", (float)list->curvalue);
}

static void MenuOptions_lookSpring_callback(void *unused)
{
	MenuOptions_lookSpring_apply();
}

static int MenuOptions_lookSpring_init(int y)
{
	static const char *itemNames[] =
	{
		"no",
		"yes",
		0
	};

	menulist_s *list = &MenuOptions_lookSpring_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.name = "lookspring";
	list->generic.callback = MenuOptions_lookSpring_callback;
	list->itemnames = itemNames;
	list->curvalue = (lookspring->value != 0);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuOptions_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Look strafe.
//----------------------------------------
static menulist_s MenuOptions_lookStrafe_list;

static void MenuOptions_lookStrafe_apply()
{
	menulist_s *list = &MenuOptions_lookStrafe_list;
	Cvar_SetValue("lookstrafe", (float)list->curvalue);
}

static void MenuOptions_lookStrafe_callback(void *unused)
{
	MenuOptions_lookStrafe_apply();
}

static int MenuOptions_lookStrafe_init(int y)
{
	static const char *itemNames[] =
	{
		"no",
		"yes",
		0
	};

	menulist_s *list = &MenuOptions_lookStrafe_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.name = "lookstrafe";
	list->generic.callback = MenuOptions_lookStrafe_callback;
	list->itemnames = itemNames;
	list->curvalue = (lookstrafe->value != 0);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuOptions_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Always run.
//----------------------------------------
static menulist_s MenuOptions_alwaysRun_list;

static void MenuOptions_alwaysRun_apply()
{
	menulist_s *list = &MenuOptions_alwaysRun_list;
	Cvar_SetValue("cl_run", (float)list->curvalue);
}

static void MenuOptions_alwaysRun_callback(void *unused)
{
	MenuOptions_alwaysRun_apply();
}

static int MenuOptions_alwaysRun_init(int y)
{
	static const char *itemNames[] =
	{
		"no",
		"yes",
		0
	};

	menulist_s *list = &MenuOptions_alwaysRun_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.name = "always run";
	list->generic.callback = MenuOptions_alwaysRun_callback;
	list->itemnames = itemNames;
	list->curvalue = (cl_run->value != 0);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuOptions_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Mouse options.
//----------------------------------------
#if !defined(GAMEPAD_ONLY)

static menuaction_s MenuOptions_mouseOptions_action;

static void MenuOptions_mouseOptions_callback(void *unused)
{
	MenuMouse_enter();
}

static int MenuOptions_mouseOptions_init(int y)
{
	menuaction_s *action = &MenuOptions_mouseOptions_action;
	action->generic.type = MTYPE_ACTION;
	action->generic.x = 0;
	action->generic.y = y;
	action->generic.name = "mouse options";
	action->generic.callback = MenuOptions_mouseOptions_callback;
	Menu_AddItem(&MenuOptions_menu, (void *)action);
	y += 10;
	return y;
}

#endif

//----------------------------------------
// Stick options.
//----------------------------------------
static menuaction_s MenuOptions_stickOptions_action;

static void MenuOptions_stickOptions_callback(void *unused)
{
	MenuStick_enter();
}

static int MenuOptions_stickOptions_init(int y)
{
	menuaction_s *action = &MenuOptions_stickOptions_action;
	action->generic.type = MTYPE_ACTION;
	action->generic.x = 0;
	action->generic.y = y;
	action->generic.name = "stick options";
	action->generic.callback = MenuOptions_stickOptions_callback;
	Menu_AddItem(&MenuOptions_menu, (void *)action);
	y += 10;
	return y;
}

//----------------------------------------
// Customize keys.
//----------------------------------------
static menuaction_s MenuOptions_customizeKeys_action;

static void MenuOptions_customizeKeys_callback(void *unused)
{
	MenuKeys_enter();
}

static int MenuOptions_customizeKeys_init(int y)
{
	menuaction_s *action = &MenuOptions_customizeKeys_action;
	action->generic.type = MTYPE_ACTION;
	action->generic.x = 0;
	action->generic.y = y;
	action->generic.name = "customize controls";
	action->generic.callback = MenuOptions_customizeKeys_callback;
	Menu_AddItem(&MenuOptions_menu, (void *)action);
	y += 10;
	return y;
}

//----------------------------------------
// Reset to defaults.
//----------------------------------------
static menuaction_s MenuOptions_defaults_action;

static void MenuOptions_defaults_callback(void *unused)
{
	Cbuf_AddText("exec default.cfg\n");
	Cbuf_AddText("exec platform.cfg\n");
	Cbuf_Execute();
}

static int MenuOptions_defaults_init(int y)
{
	menuaction_s *action = &MenuOptions_defaults_action;
	action->generic.type = MTYPE_ACTION;
	action->generic.x = 0;
	action->generic.y = y;
	action->generic.name = "reset defaults";
	action->generic.callback = MenuOptions_defaults_callback;
	Menu_AddItem(&MenuOptions_menu, (void *)action);
	y += 10;
	return y;
}

//----------------------------------------
// Console.
//----------------------------------------
static menuaction_s MenuOptions_console_action;

static void MenuOptions_console_callback(void *unused)
{
	SCR_EndLoadingPlaque(); /* get rid of loading plaque */

	if (cl.attractloop)
	{
		Cbuf_AddText("killserver\n");
		return;
	}

	Key_ClearTyping();
	Con_ClearNotify();

	M_ForceMenuOff();
	cls.key_dest = key_console;

	if ((Cvar_VariableValue("maxclients") == 1) &&
	    Com_ServerState())
	{
		Cvar_Set("paused", "1");
	}
}

static int MenuOptions_console_init(int y)
{
	menuaction_s *action = &MenuOptions_console_action;
	action->generic.type = MTYPE_ACTION;
	action->generic.x = 0;
	action->generic.y = y;
	action->generic.name = "go to console";
	action->generic.callback = MenuOptions_console_callback;
	Menu_AddItem(&MenuOptions_menu, (void *)action);
	y += 10;
	return y;
}

//----------------------------------------
// MenuOptions.
//----------------------------------------
static void MenuOptions_init()
{
	int y = 0;

	menuframework_s *menu = &MenuOptions_menu;
	memset(menu, 0, sizeof(menuframework_s));
	menu->x = viddef.width / 2;
	menu->nitems = 0;

	y = MenuOptions_soundOptions_init(y);

	y += 10;

	y = MenuOptions_forwardSpeed_init(y);
	y = MenuOptions_sideSpeed_init(y);
	y = MenuOptions_yawSpeed_init(y);
	y = MenuOptions_pitchSpeed_init(y);

	y = MenuOptions_freeLook_init(y);
	y = MenuOptions_lookSpring_init(y);
	y = MenuOptions_lookStrafe_init(y);
	y = MenuOptions_alwaysRun_init(y);

	#if !defined(GAMEPAD_ONLY)
	y = MenuOptions_mouseOptions_init(y);
	#endif
	y = MenuOptions_stickOptions_init(y);
	y = MenuOptions_customizeKeys_init(y);

	y += 10;
	y = MenuOptions_defaults_init(y);
	y = MenuOptions_console_init(y);

	Menu_CenterWithBanner(&MenuOptions_menu, "m_banner_options");
}

static void MenuOptions_draw()
{
	M_Banner("m_banner_options");
	Menu_AdjustCursor(&MenuOptions_menu, 1);
	Menu_Draw(&MenuOptions_menu);
	M_Popup();
}

static const char* MenuOptions_key(int key)
{
	if (m_popup_string)
	{
		m_popup_string = NULL;
		return NULL;
	}
	return Default_MenuKey(&MenuOptions_menu, key);
}

void MenuOptions_enter()
{
	MenuOptions_init();
	M_PushMenu(MenuOptions_draw, MenuOptions_key);
}
