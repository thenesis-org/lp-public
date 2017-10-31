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
#include "client/refresh/r_private.h"
#include "client/menu/menu.h"

#if defined(__GCW_ZERO__)
#define GL_MODE_FIXED
#endif
//#define GL_MODE_FIXED

static menuframework_s MenuVideo_menu;

static void MenuVideo_init();
static void MenuVideo_apply();

bool MenuVideo_restartNeeded;

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
// Window size.
//----------------------------------------
typedef struct
{
	short width, height;
    short bpp;
    short frequency;
} VideoMode;

static int MenuVideo_findNearestVideoMode(const VideoMode *table, int tableSize, const VideoMode *mode)
{
    int bestIndex = -1;
    VideoMode bestMode;
    memset(&bestMode, 0, sizeof(bestMode));
    for (int i = 0; i < tableSize; i++)
    {
        const VideoMode *vm = &table[i];
        bool bestAbove = bestMode.width >= mode->width && bestMode.height >= mode->height && bestMode.bpp >= mode->bpp && bestMode.frequency >= mode->frequency;
        bool above = vm->width >= mode->width && vm->height >= mode->height && vm->bpp >= mode->bpp && vm->frequency >= mode->frequency;
        bool nearer =
            abs(mode->width - vm->width) <= abs(mode->width - bestMode.width) &&
            abs(mode->height - vm->height) <= abs(mode->height - bestMode.height) &&
            abs(mode->bpp - vm->bpp) <= abs(mode->bpp - bestMode.bpp) &&
            abs(mode->frequency - vm->frequency) <= abs(mode->frequency - bestMode.frequency)
            ;
        if (bestIndex < 0 || (above && !bestAbove) || nearer)
        {
            bestIndex = i;
            bestMode = *vm;
        }
    }
    return bestIndex;
}

static const VideoMode windowSizeTable[] =
{
	{ 320, 240, 0 }, // 4:3
	{ 640, 360, 0 }, // 16:9
	{ 640, 480, 0 }, // 4:3
	{ 800, 600, 0 }, // 4:3
	{ 960, 540, 0 }, // 16:9
	{ 960, 600, 0 }, // 16:10
	{ 1024, 768, 0 }, // 4:3
	{ 1280, 720, 0 }, // 16:9
	{ 1366, 768, 0 }, // 16:9
	{ 1600, 1200, 0 }, // 4:3
	{ 1680, 1050, 0 }, // 16:9
	{ 1920, 1080, 0 }, // 16:9
	{ 1920, 1200, 0 }, // 16:10
	{ 2048, 1536, 0 }, // 4:3
};

#define windowSizeTableSize (int)(sizeof(windowSizeTable) / sizeof(windowSizeTable[0]))

int MenuVideo_windowSize_stringLength = 16;

static char** windowSizeStringTable = NULL;

static menulist_s MenuVideo_windowSize_list;

static void MenuVideo_windowSize_apply()
{
	menulist_s *list = &MenuVideo_windowSize_list;
	int value = list->curvalue;
    if (value > 0)
    {
        const VideoMode *windowSize = &windowSizeTable[value - 1];
        Cvar_SetValue("r_window_width", windowSize->width);
        Cvar_SetValue("r_window_height", windowSize->height);
    }
}

static void MenuVideo_windowSize_updateCustom()
{
    int width = (int)r_window_width->value;
    int height = (int)r_window_height->value;
    char *s = windowSizeStringTable[0];
    snprintf(s, MenuVideo_windowSize_stringLength, "- %ix%i -", width, height);
    s[MenuVideo_windowSize_stringLength-1] = 0;
}

static int MenuVideo_windowSize_init(int y)
{
    if (windowSizeStringTable == NULL)
    {
        windowSizeStringTable = malloc((windowSizeTableSize + 2) * sizeof(char *));
        windowSizeStringTable[0] = malloc(MenuVideo_windowSize_stringLength * sizeof(char));
        for (int mi = 0; mi < windowSizeTableSize; mi++)
        {
            const VideoMode *windowSize = &windowSizeTable[mi];
            char *s = malloc(MenuVideo_windowSize_stringLength * sizeof(char));
            snprintf(s, MenuVideo_windowSize_stringLength, "%ix%i", windowSize->width, windowSize->height);
            s[MenuVideo_windowSize_stringLength-1] = 0;
            windowSizeStringTable[1 + mi] = s;
        }
        windowSizeStringTable[windowSizeTableSize] = NULL;
    }
    MenuVideo_windowSize_updateCustom();
    
	menulist_s *list = &MenuVideo_windowSize_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "window size";
	list->generic.x = 0;
	list->generic.y = y;
	list->itemnames = (const char **)windowSizeStringTable;
    {
        VideoMode videoMode;
        memset(&videoMode, 0, sizeof(videoMode));
        videoMode.width = (int)r_window_width->value;
        videoMode.height = (int)r_window_height->value;
        int videoModeIndex = MenuVideo_findNearestVideoMode(windowSizeTable, windowSizeTableSize, &videoMode);
        const VideoMode *videoModeNearest = &windowSizeTable[videoModeIndex];
        if (videoModeNearest->width == videoMode.width && videoModeNearest->height == videoMode.height)
            list->curvalue = 1 + videoModeIndex;
        else
            list->curvalue = 0;
    }
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuVideo_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Fullscreen size.
//----------------------------------------
static int MenuVideo_fullscreenMode_nb = 0;
static VideoMode *MenuVideo_fullscreenMode_table = NULL;
static char **MenuVideo_fullscreenMode_strings = NULL;

static menulist_s MenuVideo_fullscreenMode_list;

static void MenuVideo_fullscreenMode_apply()
{
	menulist_s *list = &MenuVideo_fullscreenMode_list;
	int value = list->curvalue;
    const VideoMode *videoMode = &MenuVideo_fullscreenMode_table[value];
	Cvar_SetValue("r_fullscreen_width", videoMode->width);
	Cvar_SetValue("r_fullscreen_height", videoMode->height);
	Cvar_SetValue("r_fullscreen_bitsPerPixel", videoMode->bpp);
	Cvar_SetValue("r_fullscreen_frequency", videoMode->frequency);
}

static int MenuVideo_fullscreenMode_init(int y)
{
    if (MenuVideo_fullscreenMode_table == NULL)
    {
        int displayModeNb = SDL_GetNumDisplayModes(0);
        if (displayModeNb < 1)
        {
            SDL_Log("SDL_GetNumDisplayModes failed: %s", SDL_GetError());
        }
        else
        {
            MenuVideo_fullscreenMode_table = (VideoMode*)malloc(displayModeNb * sizeof(VideoMode));
            MenuVideo_fullscreenMode_strings = malloc((displayModeNb + 1) * sizeof(char*));
            int displayModeValidNb = 0;
            for (int displayModeIndex = 0; displayModeIndex < displayModeNb; displayModeIndex++) {
                SDL_DisplayMode displayMode;
                if (SDL_GetDisplayMode(0, displayModeIndex, &displayMode) != 0)
                {
                    SDL_Log("SDL_GetDisplayMode %i failed: %s", displayModeIndex, SDL_GetError());
                    break;
                }
                int w = displayMode.w, h = displayMode.h;
                Uint32 f = displayMode.format;
                int bpp = SDL_BITSPERPIXEL(f);
                int frequency = displayMode.refresh_rate;
                if (w < R_WIDTH_MIN || h < R_HEIGHT_MIN || bpp < 15)
                    continue;
                
                VideoMode *videoMode = &MenuVideo_fullscreenMode_table[displayModeValidNb];
                videoMode->width = w;
                videoMode->height = h;
                videoMode->bpp = bpp;
                videoMode->frequency = frequency;

                int stringLength = 16;
                char *fullscreenModeString = (char*)malloc(stringLength * sizeof(char));
                snprintf(fullscreenModeString, stringLength, "%ix%i %i %i", w, h, bpp, frequency);
                fullscreenModeString[stringLength-1] = 0;
                MenuVideo_fullscreenMode_strings[displayModeValidNb] = fullscreenModeString;
                
                displayModeValidNb++;
            }
            MenuVideo_fullscreenMode_nb = displayModeValidNb;
            MenuVideo_fullscreenMode_strings[displayModeValidNb] = NULL;
        }        
    }

	menulist_s *list = &MenuVideo_fullscreenMode_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "fullscreen mode";
	list->generic.x = 0;
	list->generic.y = y;
	list->itemnames = (const char **)MenuVideo_fullscreenMode_strings;
    {
        VideoMode videoMode;
        memset(&videoMode, 0, sizeof(videoMode));
        videoMode.width = (int)r_fullscreen_width->value;
        videoMode.height = (int)r_fullscreen_height->value;
        videoMode.bpp = (int)r_fullscreen_bitsPerPixel->value;
        videoMode.frequency = (int)r_fullscreen_frequency->value;
        list->curvalue = MenuVideo_findNearestVideoMode(MenuVideo_fullscreenMode_table, MenuVideo_fullscreenMode_nb, &videoMode);
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
	Cvar_SetValue("r_fullscreen", list->curvalue);
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
	list->curvalue = (r_fullscreen->value != 0);
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
		Cvar_SetValue("gl_hudscale", -1);
	else if (value < customValue)
		Cvar_SetValue("gl_hudscale", value);
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
	if (scale != gl_consolescale->value || scale != gl_menuscale->value || scale != crosshair_scale->value)
		list->curvalue = customValue;
	else if (scale < 0)
		list->curvalue = 0;
	else if (scale > 0 && scale < customValue && scale == (int)scale)
		list->curvalue = scale;
	else
		list->curvalue = customValue;
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
			Cvar_SetValue("horplus", 1);
	}
	else
	{
		if (horplus->value != 0)
			Cvar_SetValue("horplus", 0);
	}

	// fov
	if (value == 0 || value == 1)
	{
		if (fov->value != 90)
			Cvar_SetValue("fov", 90); // Restarts automatically
	}
	else if (value == 2)
	{
		if (fov->value != 86)
			Cvar_SetValue("fov", 86); // Restarts automatically
	}
	else if (value == 3)
	{
		if (fov->value != 100)
			Cvar_SetValue("fov", 100); // Restarts automatically
	}
	else if (value == 4)
	{
		if (fov->value != 106)
			Cvar_SetValue("fov", 106); // Restarts automatically
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
		list->curvalue = 0;
	else if (fov->value == 90)
		list->curvalue = 1;
	else if (fov->value == 86)
		list->curvalue = 2;
	else if (fov->value == 100)
		list->curvalue = 3;
	else if (fov->value == 106)
		list->curvalue = 4;
	else
		list->curvalue = GetCustomValue(list);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuVideo_menu, (void *)list);
	y += 10;
	return y;
}

#endif

//----------------------------------------
// View size.
//----------------------------------------
static menuslider_s MenuVideo_viewSize_slider;

static void MenuVideo_viewSize_apply()
{
	menuslider_s *slider = &MenuVideo_viewSize_slider;
	Cvar_SetValue("viewsize", slider->curvalue * 10);
}

static void MenuVideo_viewSize_callback(void *s)
{
	MenuVideo_viewSize_apply();
}

static int MenuVideo_viewSize_init(int y)
{
	menuslider_s *slider = &MenuVideo_viewSize_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.name = "screen size";
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->minvalue = 4;
	slider->maxvalue = 10;
	slider->generic.callback = MenuVideo_viewSize_callback;
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
	Cvar_SetValue("r_gamma", gamma);
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
	slider->curvalue = r_gamma->value * 10;
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
		gl_hudscale = Cvar_Get("gl_hudscale", "-1", CVAR_ARCHIVE);
	if (!gl_consolescale)
		gl_consolescale = Cvar_Get("gl_consolescale", "-1", CVAR_ARCHIVE);
	if (!gl_menuscale)
		gl_menuscale = Cvar_Get("gl_menuscale", "-1", CVAR_ARCHIVE);
	if (!crosshair_scale)
		crosshair_scale = Cvar_Get("crosshair_scale", "-1", CVAR_ARCHIVE);
	if (!horplus)
		horplus = Cvar_Get("horplus", "1", CVAR_ARCHIVE);
	if (!fov)
		fov = Cvar_Get("fov", "90", CVAR_USERINFO | CVAR_ARCHIVE);
	#endif

	if (!scr_viewsize)
		scr_viewsize = Cvar_Get("viewsize", "100", CVAR_ARCHIVE);

	int y = 10;

	menuframework_s *menu = &MenuVideo_menu;
	memset(menu, 0, sizeof(menuframework_s));
	menu->x = viddef.width >> 1;
	menu->nitems = 0;

	#if !defined(GL_MODE_FIXED)
	y = MenuVideo_windowSize_init(y);
	y = MenuVideo_fullscreenMode_init(y);
	y = MenuVideo_fullscreen_init(y);
	y = MenuVideo_aspect_init(y);
	y = MenuVideo_uiscale_init(y);
	#endif
	y = MenuVideo_viewSize_init(y);
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
	MenuVideo_windowSize_apply();
	MenuVideo_fullscreenMode_apply();
	MenuVideo_fullscreen_apply();
	MenuVideo_aspect_apply();
	MenuVideo_uiscale_apply();
	#endif
	MenuVideo_viewSize_apply();
	MenuVideo_crosshair_apply();
	#if !defined(__GCW_ZERO__)
	MenuVideo_brightness_apply();
	#endif
	MenuVideo_stereoMode_apply();
	MenuVideo_stereoConvergence_apply();

	MenuGraphics_apply();

	if (MenuVideo_restartNeeded)
		Cbuf_AddText("r_restart\n");
}

static void MenuVideo_draw()
{
    #if !defined(GL_MODE_FIXED)
    MenuVideo_windowSize_updateCustom();
    #endif

	menuframework_s *menu = &MenuVideo_menu;

    // Need to be updated because the window size could have changed.
	menu->x = viddef.width >> 1;
    Menu_CenterWithBanner(menu, "m_banner_video");
	menu->x -= 8;
    
	M_Banner("m_banner_video");
	Menu_AdjustCursor(menu, 1);
	Menu_Draw(menu);
}

static const char* MenuVideo_key(int key, int keyUnmodified)
{
	return Default_MenuKey(&MenuVideo_menu, key, keyUnmodified);
}

void MenuVideo_enter()
{
	MenuVideo_init();
	MenuGraphics_init();
	M_PushMenu(MenuVideo_draw, MenuVideo_key);
}
