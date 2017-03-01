#include "client/menu/menu.h"

#if !defined(GAMEPAD_ONLY)

static menuframework_s MenuMouse_menu;

//----------------------------------------
// Mouse sensivity.
//----------------------------------------

static menuslider_s MenuMouse_mouseSensitivity_slider;

static void MenuMouse_mouseSensitivity_apply()
{
	menuslider_s *slider = &MenuMouse_mouseSensitivity_slider;
	Cvar_SetValue("sensitivity", slider->curvalue / 2.0F);
}

static void MenuMouse_mouseSensitivity_callback(void *unused)
{
	MenuMouse_mouseSensitivity_apply();
}

static int MenuMouse_mouseSensitivity_init(int y)
{
	menuslider_s *slider = &MenuMouse_mouseSensitivity_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = "mouse speed";
	slider->generic.callback = MenuMouse_mouseSensitivity_callback;
	slider->minvalue = 2;
	slider->maxvalue = 22;
	slider->curvalue = sensitivity->value * 2;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuMouse_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// Invert mouse.
//----------------------------------------
static menulist_s MenuMouse_invertMouse_list;

static void MenuMouse_invertMouse_apply()
{
	menulist_s *list = &MenuMouse_invertMouse_list;
	float pitch = fabsf(m_pitch->value);
	Cvar_SetValue("m_pitch", list->curvalue ? -pitch : pitch);
}

static void MenuMouse_invertMouse_callback(void *unused)
{
	MenuMouse_invertMouse_apply();
}

static int MenuMouse_invertMouse_init(int y)
{
	static const char *itemNames[] =
	{
		"no",
		"yes",
		0
	};

	menulist_s *list = &MenuMouse_invertMouse_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.name = "invert mouse";
	list->generic.callback = MenuMouse_invertMouse_callback;
	list->itemnames = itemNames;
	list->curvalue = (m_pitch->value < 0);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuMouse_menu, (void *)list);
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

	y = MenuMouse_mouseSensitivity_init(y);
	y = MenuMouse_invertMouse_init(y);

	Menu_CenterWithBanner(&MenuMouse_menu, "m_banner_options");
}

static void MenuMouse_draw()
{
	M_Banner("m_banner_options");
	Menu_AdjustCursor(&MenuMouse_menu, 1);
	Menu_Draw(&MenuMouse_menu);
	M_Popup();
}

static const char* MenuMouse_key(int key)
{
	if (m_popup_string)
	{
		m_popup_string = NULL;
		return NULL;
	}
	return Default_MenuKey(&MenuMouse_menu, key);
}

void MenuMouse_enter()
{
	MenuMouse_init();
	M_PushMenu(MenuMouse_draw, MenuMouse_key);
}

#endif
