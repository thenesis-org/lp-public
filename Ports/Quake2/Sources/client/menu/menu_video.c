/*
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
 * This is the refresher dependend video menu. If you add a new
 * refresher this menu must be altered.
 *
 * =======================================================================
 */

#include "client/client.h"
#include "client/refresh/local.h"
#include "client/menu/menu.h"

#if defined(__GCW_ZERO__)
#define GL_MODE_FIXED
#endif
//#define GL_MODE_FIXED

static menuframework_s MenuVideo_menu;

static void MenuVideo_init();
static void MenuVideo_apply();
static void MenuVideo_draw();
static const char* MenuVideo_key(int key);

qboolean MenuVideo_restartNeeded;

#if !defined(GL_MODE_FIXED)
static int GetCustomValue(menulist_s *list)
{
	static menulist_s *last;
	static int i;

	if (list != last)
	{
		last = list;
		i = list->curvalue;
		do
		{
			i++;
		}
		while (list->itemnames[i]);
		i--;
	}

	return i;
}
#endif

#if !defined(GL_MODE_FIXED)
//----------------------------------------
// Mode.
//----------------------------------------
static menulist_s MenuVideo_mode_list;

static void MenuVideo_mode_apply()
{
	menulist_s *list = &MenuVideo_mode_list;
	int value = list->curvalue;
	int customValue = GetCustomValue(list);
	if (value != customValue)
	{
		/* Restarts automatically */
		Cvar_SetValue("gl_mode", value);
	}
	else
	{
		/* Restarts automatically */
		Cvar_SetValue("gl_mode", -1);
	}
}

static int MenuVideo_mode_init(int y)
{
	static char **resolutions = NULL;
 
    if (resolutions == NULL)
    {
        int modeNb = Renderer_GetModeNb();
        resolutions = malloc((modeNb + 2) * sizeof(char *));
        int stringLength = 16;
        for (int mi = 0; mi < modeNb; mi++)
        {
            char *s = malloc(stringLength * sizeof(char));
            int w, h;
            Renderer_GetModeInfo(mi, &w, &h);
            snprintf(s, stringLength, "[%4ix%4i]", w, h);
            s[stringLength-1] = 0;
            resolutions[mi] = s;
        }
        resolutions[modeNb] = "[custom   ]";
        resolutions[modeNb+1] = NULL;
    }

	menulist_s *list = &MenuVideo_mode_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "video mode";
	list->generic.x = 0;
	list->generic.y = y;
	list->itemnames = (const char **)resolutions;
	if (gl_mode->value >= 0)
	{
		list->curvalue = gl_mode->value;
	}
	else
	{
		list->curvalue = GetCustomValue(list);
	}
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuVideo_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Fullscreen.
//----------------------------------------
static menulist_s MenuVideo_fullscreen_list;

static void MenuVideo_fullscreen_apply()
{
	menulist_s *list = &MenuVideo_fullscreen_list;
	/* Restarts automatically */
	Cvar_SetValue("vid_fullscreen", list->curvalue);
}

static int MenuVideo_fullscreen_init(int y)
{
	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};
	menulist_s *list = &MenuVideo_fullscreen_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "fullscreen";
	list->generic.x = 0;
	list->generic.y = y;
	list->itemnames = yesno_names;
	list->curvalue = (vid_fullscreen->value != 0);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuVideo_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// UI scale.
//----------------------------------------
static menulist_s MenuVideo_uiscale_list;

static void MenuVideo_uiscale_apply()
{
	menulist_s *list = &MenuVideo_uiscale_list;
	int value = list->curvalue;
	int customValue = GetCustomValue(list);
	if (value == 0)
	{
		Cvar_SetValue("gl_hudscale", -1);
	}
	else
	if (value < customValue)
	{
		Cvar_SetValue("gl_hudscale", value);
	}
	if (value != customValue)
	{
		float scale = gl_hudscale->value;
		Cvar_SetValue("gl_consolescale", scale);
		Cvar_SetValue("gl_menuscale", scale);
		Cvar_SetValue("crosshair_scale", scale);
	}
}

static int MenuVideo_uiscale_init(int y)
{
	static const char *uiscale_names[] =
	{
		"auto",
		"1x",
		"2x",
		"3x",
		"4x",
		"5x",
		"6x",
		"custom",
		0
	};

	menulist_s *list = &MenuVideo_uiscale_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "ui scale";
	list->generic.x = 0;
	list->generic.y = y;
	list->itemnames = uiscale_names;
	float scale = gl_hudscale->value;
	int customValue = GetCustomValue(list);
	if (scale != gl_consolescale->value ||
	    scale != gl_menuscale->value ||
	    scale != crosshair_scale->value)
	{
		list->curvalue = customValue;
	}
	else
	if (scale < 0)
	{
		list->curvalue = 0;
	}
	else
	if (scale > 0 &&
	    scale < customValue &&
	    scale == (int)scale)
	{
		list->curvalue = scale;
	}
	else
	{
		list->curvalue = customValue;
	}
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuVideo_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Aspect.
//----------------------------------------
static menulist_s MenuVideo_aspect_list;

static void MenuVideo_aspect_apply()
{
	menulist_s *list = &MenuVideo_aspect_list;
	int value = list->curvalue;

	/* horplus */
	if (value == 0)
	{
		if (horplus->value != 1)
		{
			Cvar_SetValue("horplus", 1);
		}
	}
	else
	{
		if (horplus->value != 0)
		{
			Cvar_SetValue("horplus", 0);
		}
	}

	/* fov */
	if (value == 0 || value == 1)
	{
		if (fov->value != 90)
		{
			/* Restarts automatically */
			Cvar_SetValue("fov", 90);
		}
	}
	else
	if (value == 2)
	{
		if (fov->value != 86)
		{
			/* Restarts automatically */
			Cvar_SetValue("fov", 86);
		}
	}
	else
	if (value == 3)
	{
		if (fov->value != 100)
		{
			/* Restarts automatically */
			Cvar_SetValue("fov", 100);
		}
	}
	else
	if (value == 4)
	{
		if (fov->value != 106)
		{
			/* Restarts automatically */
			Cvar_SetValue("fov", 106);
		}
	}
}

static int MenuVideo_aspect_init(int y)
{
	static const char *aspect_names[] =
	{
		"auto",
		"4:3",
		"5:4",
		"16:10",
		"16:9",
		"custom",
		0
	};

	menulist_s *list = &MenuVideo_aspect_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "aspect ratio";
	list->generic.x = 0;
	list->generic.y = y;
	list->itemnames = aspect_names;
	if (horplus->value == 1)
	{
		list->curvalue = 0;
	}
	else
	if (fov->value == 90)
	{
		list->curvalue = 1;
	}
	else
	if (fov->value == 86)
	{
		list->curvalue = 2;
	}
	else
	if (fov->value == 100)
	{
		list->curvalue = 3;
	}
	else
	if (fov->value == 106)
	{
		list->curvalue = 4;
	}
	else
	{
		list->curvalue = GetCustomValue(list);
	}
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuVideo_menu, (void *)list);
	y += 10;
	return y;
}

#endif

//----------------------------------------
// Screen size.
//----------------------------------------
static menuslider_s MenuVideo_screenSize_slider;

static void MenuVideo_screenSize_apply()
{
	menuslider_s *slider = &MenuVideo_screenSize_slider;
	Cvar_SetValue("viewsize", slider->curvalue * 10);
}

static void MenuVideo_screenSize_callback(void *s)
{
	MenuVideo_screenSize_apply();
}

static int MenuVideo_screenSize_init(int y)
{
	menuslider_s *slider = &MenuVideo_screenSize_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.name = "screen size";
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->minvalue = 4;
	slider->maxvalue = 10;
	slider->generic.callback = MenuVideo_screenSize_callback;
	slider->curvalue = scr_viewsize->value / 10;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuVideo_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// Crosshair.
//----------------------------------------
static menulist_s MenuVideo_crosshair_list;

static void MenuVideo_crosshair_apply()
{
	menulist_s *list = &MenuVideo_crosshair_list;
	Cvar_SetValue("crosshair", (float)list->curvalue);
}

static void MenuVideo_crosshair_callback(void *unused)
{
	MenuVideo_crosshair_apply();
}

static int MenuVideo_crosshair_init(int y)
{
	static const char *crosshair_names[] =
	{
		"none",
		"cross",
		"dot",
		"angle",
		0
	};

	menulist_s *list = &MenuVideo_crosshair_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.name = "crosshair";
	list->generic.callback = MenuVideo_crosshair_callback;
	list->itemnames = crosshair_names;
	list->curvalue = ClampCvar(0, 3, crosshair->value);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuVideo_menu, (void *)list);
	y += 10;
	return y;
}

#if !defined(__GCW_ZERO__)

//----------------------------------------
// Brightness.
//----------------------------------------
static menuslider_s MenuVideo_brightness_slider;

static void MenuVideo_brightness_apply()
{
	menuslider_s *slider = &MenuVideo_brightness_slider;
	float gamma = slider->curvalue / 10.0f;
	Cvar_SetValue("vid_gamma", gamma);
}

static void MenuVideo_brightness_callback(void *s)
{
	MenuVideo_brightness_apply();
}

static int MenuVideo_brightness_init(int y)
{
	menuslider_s *slider = &MenuVideo_brightness_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.name = "brightness";
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.callback = MenuVideo_brightness_callback;
	slider->minvalue = 1;
	slider->maxvalue = 20;
	slider->curvalue = vid_gamma->value * 10;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuVideo_menu, (void *)slider);
	y += 10;
	return y;
}

#endif

//----------------------------------------
// Stereo mode.
//----------------------------------------
typedef enum
{
	MenuStereoMode_Disabled,
	MenuStereoMode_Anaglyph_RC,
	MenuStereoMode_Anaglyph_GM,
	MenuStereoMode_Nb
} MenuStereoMode;

static menulist_s MenuVideo_stereoMode_list;

static void MenuVideo_stereoMode_apply()
{
	menulist_s *list = &MenuVideo_stereoMode_list;
	MenuStereoMode stereoMode = (MenuStereoMode)list->curvalue;
	if (stereoMode == MenuStereoMode_Disabled)
	{
		Cvar_SetValue("gl_stereo", 0);
	}
	else
	{
		Cvar_SetValue("gl_stereo", 1);
		char *colorsString;
		switch (stereoMode)
		{
		default:
		case MenuStereoMode_Anaglyph_RC: colorsString = "rc"; break;
		case MenuStereoMode_Anaglyph_GM: colorsString = "gm"; break;
		}
		Cvar_Set("gl_stereo_anaglyph_colors", colorsString);
	}
}

static void MenuVideo_stereoMode_callback(void *s)
{
	MenuVideo_stereoMode_apply();
}

static int MenuVideo_stereoMode_init(int y)
{
	static const char *stereo_mode_names[] =
	{
		"disabled",
		"red - cyan",
		"green - magenta",
		0
	};

	menulist_s *list = &MenuVideo_stereoMode_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "stereo mode";
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.callback = MenuVideo_stereoMode_callback;
	list->itemnames = stereo_mode_names;
	int curvalue = 0;
	if (gl_stereo->value)
	{
		if (strcmp(gl_stereo_anaglyph_colors->string, "rc") == 0)
			curvalue = 1;
		else
		if (strcmp(gl_stereo_anaglyph_colors->string, "gm") == 0)
			curvalue = 2;
	}
	list->curvalue = curvalue;
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuVideo_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Stereo convergence.
//----------------------------------------
static menuslider_s MenuVideo_stereoConvergence_slider;

static void MenuVideo_stereoConvergence_apply()
{
	menuslider_s *slider = &MenuVideo_stereoConvergence_slider;
	Cvar_SetValue("gl_stereo_convergence", slider->curvalue / 10.0f);
}

static void MenuVideo_stereoConvergence_callback(void *s)
{
	MenuVideo_stereoConvergence_apply();
}

static int MenuVideo_stereoConvergence_init(int y)
{
	menuslider_s *slider = &MenuVideo_stereoConvergence_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.name = "stereo convergence";
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->minvalue = 0;
	slider->maxvalue = 20;
	slider->generic.callback = MenuVideo_stereoConvergence_callback;
	slider->curvalue = (gl_stereo_convergence->value) * 10.0f;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuVideo_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// Graphics menu.
//----------------------------------------
static menuaction_s MenuVideo_graphicsMenu_action;

static void MenuVideo_graphicsMenu_callback(void *unused)
{
	MenuGraphics_enter();
}

static int MenuVideo_graphicsMenu_init(int y)
{
	menuaction_s *action = &MenuVideo_graphicsMenu_action;
	action->generic.type = MTYPE_ACTION;
	action->generic.name = "advanced settings";
	action->generic.x = 0;
	action->generic.y = y;
	action->generic.callback = MenuVideo_graphicsMenu_callback;
	Menu_AddItem(&MenuVideo_menu, (void *)action);
	y += 10;
	return y;
}

//----------------------------------------
// Apply changes.
//----------------------------------------
static menuaction_s MenuVideo_apply_action;

static void MenuVideo_apply_callback(void *unused)
{
	MenuVideo_apply();
}

static int MenuVideo_apply_init(int y)
{
	menuaction_s *action = &MenuVideo_apply_action;
	action->generic.type = MTYPE_ACTION;
	action->generic.name = "apply";
	action->generic.x = 0;
	action->generic.y = y;
	action->generic.callback = MenuVideo_apply_callback;
	Menu_AddItem(&MenuVideo_menu, (void *)action);
	y += 10;
	return y;
}

//----------------------------------------
// MenuVideo.
//----------------------------------------
static void MenuVideo_init()
{
	MenuVideo_restartNeeded = false;

	#if !defined(GL_MODE_FIXED)
	if (!gl_hudscale)
	{
		gl_hudscale = Cvar_Get("gl_hudscale", "-1", CVAR_ARCHIVE);
	}
	if (!gl_consolescale)
	{
		gl_consolescale = Cvar_Get("gl_consolescale", "-1", CVAR_ARCHIVE);
	}
	if (!gl_menuscale)
	{
		gl_menuscale = Cvar_Get("gl_menuscale", "-1", CVAR_ARCHIVE);
	}
	if (!crosshair_scale)
	{
		crosshair_scale = Cvar_Get("crosshair_scale", "-1", CVAR_ARCHIVE);
	}
	if (!horplus)
	{
		horplus = Cvar_Get("horplus", "1", CVAR_ARCHIVE);
	}
	if (!fov)
	{
		fov = Cvar_Get("fov", "90", CVAR_USERINFO | CVAR_ARCHIVE);
	}
	#endif

	if (!scr_viewsize)
	{
		scr_viewsize = Cvar_Get("viewsize", "100", CVAR_ARCHIVE);
	}

	int y = 10;

	menuframework_s *menu = &MenuVideo_menu;
	memset(menu, 0, sizeof(menuframework_s));
	menu->x = viddef.width / 2;
	menu->nitems = 0;

	#if !defined(GL_MODE_FIXED)
	y = MenuVideo_mode_init(y);
	y = MenuVideo_fullscreen_init(y);
	y = MenuVideo_aspect_init(y);
	y = MenuVideo_uiscale_init(y);
	#endif
	y = MenuVideo_screenSize_init(y);
	#if !defined(__GCW_ZERO__)
	y = MenuVideo_brightness_init(y);
	#endif
	y = MenuVideo_crosshair_init(y);

	y += 10;

	y = MenuVideo_stereoMode_init(y);
	y = MenuVideo_stereoConvergence_init(y);

	y += 10;

	y = MenuVideo_graphicsMenu_init(y);
	y = MenuVideo_apply_init(y);

	Menu_CenterWithBanner(menu, "m_banner_video");
	menu->x -= 8;
}

static void MenuVideo_apply()
{
	#if !defined(GL_MODE_FIXED)
	MenuVideo_mode_apply();
	MenuVideo_fullscreen_apply();
	MenuVideo_aspect_apply();
	MenuVideo_uiscale_apply();
	#endif
	MenuVideo_screenSize_apply();
	MenuVideo_crosshair_apply();
	#if !defined(__GCW_ZERO__)
	MenuVideo_brightness_apply();
	#endif
	MenuVideo_stereoMode_apply();
	MenuVideo_stereoConvergence_apply();

	MenuGraphics_apply();

	if (MenuVideo_restartNeeded)
	{
		Cbuf_AddText("vid_restart\n");
	}
	M_ForceMenuOff();
}

static void MenuVideo_draw()
{
	M_Banner("m_banner_video");
	Menu_AdjustCursor(&MenuVideo_menu, 1);
	Menu_Draw(&MenuVideo_menu);
}

static const char* MenuVideo_key(int key)
{
	return Default_MenuKey(&MenuVideo_menu, key);
}

void MenuVideo_enter()
{
	MenuVideo_init();
	MenuGraphics_init();
	M_PushMenu(MenuVideo_draw, MenuVideo_key);
}
