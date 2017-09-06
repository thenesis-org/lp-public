#include "client/client.h"
#include "client/refresh/local.h"
#include "client/menu/menu.h"

#include "OpenGLES/EGLWrapper.h"

extern qboolean graphics_support_msaa;
extern cvar_t *cl_drawfps;

static menuframework_s MenuGraphics_menu;

static const char *yesno_names[] =
{
	"no",
	"yes",
	0
};

//----------------------------------------
// Show FPS.
//----------------------------------------
static menulist_s MenuGraphics_showfps_list;

static void MenuGraphics_showfps_apply()
{
	menulist_s *list = &MenuGraphics_showfps_list;
	Cvar_SetValue("cl_drawfps", list->curvalue);
}

static void MenuGraphics_showfps_callback(void *s)
{
	MenuGraphics_showfps_apply();
}

static int MenuGraphics_showfps_init(int y)
{
	menulist_s *list = &MenuGraphics_showfps_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "show fps";
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.callback = MenuGraphics_showfps_callback;
	list->itemnames = yesno_names;
	list->curvalue = (cl_drawfps->value != 0);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuGraphics_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Vsync.
//----------------------------------------
static menulist_s MenuGraphics_vsync_list;

static void MenuGraphics_vsync_apply()
{
	menulist_s *list = &MenuGraphics_vsync_list;
	int value = list->curvalue;
	if (gl_swapinterval->value != value)
	{
		Cvar_SetValue("gl_swapinterval", value);
		MenuVideo_restartNeeded = true;
	}
}

static int MenuGraphics_vsync_init(int y)
{
	menulist_s *list = &MenuGraphics_vsync_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "vertical sync";
	list->generic.x = 0;
	list->generic.y = y;
	list->itemnames = yesno_names;
	list->curvalue = (gl_swapinterval->value != 0);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuGraphics_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// MSAA.
//----------------------------------------
static menulist_s MenuGraphics_msaa_list;

static void MenuGraphics_msaa_apply()
{
	menulist_s *list = &MenuGraphics_msaa_list;
	int value = list->curvalue;
	if (value == 0)
	{
		if (gl_msaa_samples->value != 0)
		{
			Cvar_SetValue("gl_msaa_samples", 0);
			MenuVideo_restartNeeded = true;
		}
	}
	else
	{
		float valuePow = powf(2, value);
		if (gl_msaa_samples->value != valuePow)
		{
			Cvar_SetValue("gl_msaa_samples", valuePow);
			MenuVideo_restartNeeded = true;
		}
	}
}

static int MenuGraphics_msaa_init(int y)
{
	enum { MAX_POW = 4 };
	static const char *pow2_names_all[MAX_POW + 1] =
	{
		"off", "2x", "4x", "8x", "16x"
	};
	static const char *pow2_names[MAX_POW + 2];
	for (int pi = 0; pi <= MAX_POW; pi++)
	{
		pow2_names[pi] = pow2_names_all[pi];
		if ((1 << pi) >= eglwContext->configInfoAbilities.samples)
        {
            pow2_names[pi + 1] = NULL;
			break;
        }
	}
	pow2_names[MAX_POW + 1] = NULL;

	menulist_s *list = &MenuGraphics_msaa_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "multisampling";
	list->generic.x = 0;
	list->generic.y = y;
	list->itemnames = pow2_names;
	list->curvalue = 0;
	if (gl_msaa_samples->value)
	{
		do
		{
			list->curvalue++;
		}
		while (pow2_names[list->curvalue] && powf(2, list->curvalue) <= gl_msaa_samples->value);
		list->curvalue--;
	}
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuGraphics_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Clear mode.
//----------------------------------------
typedef enum
{
	MenuClearMode_None,
	MenuClearMode_DepthOnly,
	MenuClearMode_ColorOnly,
	MenuClearMode_DepthAndColor,
	MenuClearMode_Nb
} MenuClearMode;

static menulist_s MenuGraphics_clearMode_list;

static void MenuGraphics_clearMode_apply()
{
	menulist_s *list = &MenuGraphics_clearMode_list;
	MenuClearMode mode = (MenuClearMode)list->curvalue;
	switch (mode)
	{
	case MenuClearMode_None:
		Cvar_SetValue("gl_clear", 0);
		Cvar_SetValue("gl_ztrick", 1);
		break;
	default:
	case MenuClearMode_DepthOnly:
		Cvar_SetValue("gl_clear", 0);
		Cvar_SetValue("gl_ztrick", 0);
		break;
	case MenuClearMode_ColorOnly:
		Cvar_SetValue("gl_clear", 1);
		Cvar_SetValue("gl_ztrick", 1);
		break;
	case MenuClearMode_DepthAndColor:
		Cvar_SetValue("gl_clear", 1);
		Cvar_SetValue("gl_ztrick", 0);
		break;
	}
}

static void MenuGraphics_clearMode_callback(void *s)
{
	MenuGraphics_clearMode_apply();
}

static int MenuGraphics_clearMode_init(int y)
{
	static const char *clear_mode_names[] =
	{
		"none",
		"depth only",
		"color only",
		"depth & color",
		0
	};
	menulist_s *list = &MenuGraphics_clearMode_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "clear mode";
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.callback = MenuGraphics_clearMode_callback;
	list->itemnames = clear_mode_names;
	{
		MenuClearMode clearMode = 0;
		if (gl_ztrick->value == 0)
			clearMode += MenuClearMode_DepthOnly;
		if (gl_clear->value != 0)
			clearMode += MenuClearMode_ColorOnly;
		list->curvalue = clearMode;
	}
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuGraphics_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Discard framebuffer.
//----------------------------------------
static menulist_s MenuGraphics_discardFramebuffer_list;

static void MenuGraphics_discardFramebuffer_apply()
{
	menulist_s *list = &MenuGraphics_discardFramebuffer_list;
	int value = list->curvalue;
	Cvar_SetValue("gl_discardframebuffer", value);
}

static void MenuGraphics_discardFramebuffer_callback(void *s)
{
	MenuGraphics_discardFramebuffer_apply();
}

static int MenuGraphics_discardFramebuffer_init(int y)
{
	menulist_s *list = &MenuGraphics_discardFramebuffer_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "discard framebuffer";
	list->generic.x = 0;
	list->generic.y = y;
	list->itemnames = yesno_names;
	list->curvalue = (gl_discardframebuffer->value != 0);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuGraphics_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Multitexturing.
//----------------------------------------
static menulist_s MenuGraphics_multitexturing_list;

static void MenuGraphics_multitexturing_apply()
{
	menulist_s *list = &MenuGraphics_multitexturing_list;
	Cvar_SetValue("gl_multitexturing", list->curvalue);
}

static void MenuGraphics_multitexturing_callback(void *s)
{
	MenuGraphics_multitexturing_apply();
}

static int MenuGraphics_multitexturing_init(int y)
{
	menulist_s *list = &MenuGraphics_multitexturing_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "multitexturing";
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.callback = MenuGraphics_multitexturing_callback;
	list->itemnames = yesno_names;
	list->curvalue = (gl_multitexturing->value != 0);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuGraphics_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Texture filtering.
//----------------------------------------
typedef enum
{
	MenuFilteringMode_Nearest,
	MenuFilteringMode_Bilinear,
	MenuFilteringMode_Trilinear,
} MenuFilteringMode;

static menulist_s MenuGraphics_filtering_list;

static void MenuGraphics_filtering_apply()
{
	menulist_s *list = &MenuGraphics_filtering_list;
	switch (list->curvalue)
	{
	default:
	case MenuFilteringMode_Nearest:
		Cvar_Set("gl_texturefilter", "GL_NEAREST_MIPMAP_NEAREST");
		break;
	case MenuFilteringMode_Bilinear:
		Cvar_Set("gl_texturefilter", "GL_LINEAR_MIPMAP_NEAREST");
		break;
	case MenuFilteringMode_Trilinear:
		Cvar_Set("gl_texturefilter", "GL_LINEAR_MIPMAP_LINEAR");
		break;
	}
}

static void MenuGraphics_filtering_callback(void *s)
{
	MenuGraphics_filtering_apply();
}

static int MenuGraphics_filtering_init(int y)
{
	static const char *texture_filtering_names[] =
	{
		"nearest",
		"bilinear",
		"trilinear",
		0
	};
	menulist_s *list = &MenuGraphics_filtering_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "filtering";
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.callback = MenuGraphics_filtering_callback;
	list->itemnames = texture_filtering_names;
	{
		if (Q_stricmp("GL_LINEAR_MIPMAP_LINEAR", gl_texturefilter->string) == 0)
		{
			list->curvalue = MenuFilteringMode_Trilinear;
		}
		else
		if (Q_stricmp("GL_LINEAR_MIPMAP_NEAREST", gl_texturefilter->string) == 0)
		{
			list->curvalue = MenuFilteringMode_Bilinear;
		}
		else
		{
			list->curvalue = MenuFilteringMode_Nearest;
		}
	}
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuGraphics_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Anisotropic texture filtering.
//----------------------------------------
static menulist_s MenuGraphics_anisotropicFiltering_list;

static void MenuGraphics_anisotropicFiltering_apply()
{
	menulist_s *list = (menulist_s *)&MenuGraphics_anisotropicFiltering_list;
	if (list->curvalue == 0)
	{
		Cvar_SetValue("gl_anisotropic", 0);
	}
	else
	{
		Cvar_SetValue("gl_anisotropic", pow(2, list->curvalue));
	}
}

static void MenuGraphics_anisotropicFiltering_callback(void *s)
{
	MenuGraphics_anisotropicFiltering_apply();
}

static int MenuGraphics_anisotropicFiltering_init(int y)
{
	enum { MAX_POW = 4 };
	static const char *pow2_names_all[MAX_POW + 1] =
	{
		"off", "2x", "4x", "8x", "16x"
	};
	static const char *pow2_names[MAX_POW + 2];
	for (int pi = 0; pi <= MAX_POW; pi++)
	{
		pow2_names[pi] = pow2_names_all[pi];
		if ((1 << pi) >= gl_config.max_anisotropy)
        {
            pow2_names[pi + 1] = NULL;
            break;
        }
	}
	pow2_names[MAX_POW + 1] = NULL;

	menulist_s *list = (menulist_s *)&MenuGraphics_anisotropicFiltering_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "aniso filtering";
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.callback = MenuGraphics_anisotropicFiltering_callback;
	list->itemnames = pow2_names;
	list->curvalue = 0;
	if (gl_anisotropic->value)
	{
		do
		{
			list->curvalue++;
		}
		while (pow2_names[list->curvalue] && powf(2, list->curvalue) <= gl_anisotropic->value);
		list->curvalue--;
	}
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuGraphics_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Particle mode.
//----------------------------------------
#if defined(EGLW_GLES1)

typedef enum
{
	MenuParticleMode_Points,
	MenuParticleMode_PointSprites,
	MenuParticleMode_Quads,
} MenuParticleMode;

static menulist_s MenuGraphics_particleMode_list;

static void MenuGraphics_particleMode_apply()
{
	menulist_s *list = (menulist_s *)&MenuGraphics_particleMode_list;
	MenuParticleMode particleMode = (MenuParticleMode)list->curvalue;
	switch (particleMode)
	{
	default:
	case MenuParticleMode_Points:
		Cvar_SetValue("gl_particle_point", 1);
		Cvar_SetValue("gl_particle_sprite", 0);
		break;
	case MenuParticleMode_PointSprites:
		Cvar_SetValue("gl_particle_point", 1);
		Cvar_SetValue("gl_particle_sprite", 1);
		break;
	case MenuParticleMode_Quads:
		Cvar_SetValue("gl_particle_point", 0);
		Cvar_SetValue("gl_particle_sprite", 0);
		break;
	}
}

static void MenuGraphics_particleMode_callback(void *s)
{
	MenuGraphics_particleMode_apply();
}

static int MenuGraphics_particleMode_init(int y)
{
	static const char *particle_mode_names[] =
	{
		"points",
		"point sprites",
		"quads",
		0
	};
	menulist_s *list = (menulist_s *)&MenuGraphics_particleMode_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "particle mode";
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.callback = MenuGraphics_particleMode_callback;
	list->itemnames = particle_mode_names;
	{
		MenuParticleMode particleMode;
		if (gl_particle_point->value != 0)
		{
			if (gl_particle_sprite->value != 0)
			{
				particleMode = MenuParticleMode_PointSprites;
			}
			else
			{
				particleMode = MenuParticleMode_Points;
			}
		}
		else
		{
			particleMode = MenuParticleMode_Quads;
		}
		list->curvalue = particleMode;
	}
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuGraphics_menu, (void *)list);
	y += 10;
	return y;
}

#endif

//----------------------------------------
// Particle size.
//----------------------------------------
static menuslider_s MenuGraphics_particleSize_slider;

static void MenuGraphics_particleSize_apply()
{
	menuslider_s *slider = (menuslider_s *)&MenuGraphics_particleSize_slider;
	Cvar_SetValue("gl_particle_size", slider->curvalue * 10.0f);
	Cvar_SetValue("gl_particle_max_size", slider->curvalue * 10.0f);
}

static void MenuGraphics_particleSize_callback(void *s)
{
	MenuGraphics_particleSize_apply();
}

static int MenuGraphics_particleSize_init(int y)
{
	menuslider_s *slider = (menuslider_s *)&MenuGraphics_particleSize_slider;
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.name = "particle size";
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->minvalue = 0;
	slider->maxvalue = 20;
	slider->generic.callback = MenuGraphics_particleSize_callback;
	slider->curvalue = (gl_particle_size->value) / 10.0f;
	slider->savedValue = slider->curvalue;
	Menu_AddItem(&MenuGraphics_menu, (void *)slider);
	y += 10;
	return y;
}

//----------------------------------------
// Shadow mode.
//----------------------------------------
typedef enum
{
	MenuShadowMode_Disabled,
	MenuShadowMode_Projected,
	MenuShadowMode_ProjectedWithStencil,
} MenuShadowMode;

static menulist_s MenuGraphics_shadowMode_list;

static void MenuGraphics_shadowMode_apply()
{
	menulist_s *list = (menulist_s *)&MenuGraphics_shadowMode_list;
	MenuShadowMode shadowMode = (MenuShadowMode)list->curvalue;
	switch (shadowMode)
	{
	default:
	case MenuShadowMode_Disabled:
		Cvar_SetValue("gl_shadows", 0);
		Cvar_SetValue("gl_stencilshadow", 0);
		break;
	case MenuShadowMode_Projected:
		Cvar_SetValue("gl_shadows", 1);
		Cvar_SetValue("gl_stencilshadow", 0);
		break;
	case MenuShadowMode_ProjectedWithStencil:
		Cvar_SetValue("gl_shadows", 1);
		Cvar_SetValue("gl_stencilshadow", 1);
		break;
	}
}

static void MenuGraphics_shadowMode_callback(void *s)
{
	MenuGraphics_shadowMode_apply();
}

static int MenuGraphics_shadowMode_init(int y)
{
	static const char *shadow_mode_names[] =
	{
		"none",
		"projected",
		"projected stencil",
		0
	};
	menulist_s *list = (menulist_s *)&MenuGraphics_shadowMode_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "shadow mode";
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.callback = MenuGraphics_shadowMode_callback;
	list->itemnames = shadow_mode_names;
	{
		MenuShadowMode shadowMode = 0;
		if (gl_shadows->value != 0)
		{
			shadowMode++;
			if (gl_stencilshadow->value != 0)
				shadowMode++;
		}
		list->curvalue = shadowMode;
	}
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuGraphics_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Lighting.
//----------------------------------------
typedef enum
{
	MenuLightingMode_Static, // Nothing is dynamically lighted.
	MenuLightingMode_FlashBlend, // Use only flash blends.
	MenuLightingMode_DynamicFlashBlend, // World is dynamically lighted only with animated lights. All other things use flash blends.
	MenuLightingMode_Dynamic, // All is dynamically lighted.
} MenuLightingMode;

static menulist_s MenuGraphics_lighting_list;

static void MenuGraphics_lighting_apply()
{
	menulist_s *list = &MenuGraphics_lighting_list;
	MenuLightingMode lightingMode = (MenuLightingMode)list->curvalue;
	switch (lightingMode)
	{
	case MenuLightingMode_Static:
		Cvar_SetValue("gl_dynamic", 0);
		Cvar_SetValue("gl_flashblend", 0);
		break;
	case MenuLightingMode_FlashBlend:
		Cvar_SetValue("gl_dynamic", 0);
		Cvar_SetValue("gl_flashblend", 1);
		break;
	case MenuLightingMode_DynamicFlashBlend:
		Cvar_SetValue("gl_dynamic", 1);
		Cvar_SetValue("gl_flashblend", 1);
		break;
	default:
	case MenuLightingMode_Dynamic:
		Cvar_SetValue("gl_dynamic", 1);
		Cvar_SetValue("gl_flashblend", 0);
		break;
	}
}

static void MenuGraphics_lighting_callback(void *s)
{
	MenuGraphics_lighting_apply();
}

static int MenuGraphics_lighting_init(int y)
{
	static const char *lighting_names[] =
	{
		"static",
		"flash blend",
		"dynamic flash blend",
		"dynamic",
		0
	};
	menulist_s *list = &MenuGraphics_lighting_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "lighting";
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.callback = MenuGraphics_lighting_callback;
	list->itemnames = lighting_names;
	{
		MenuLightingMode lightingMode = 0;
		if (gl_dynamic->value != 0)
		{
			if (gl_flashblend->value != 0)
			{
				lightingMode = MenuLightingMode_DynamicFlashBlend;
			}
			else
			{
				lightingMode = MenuLightingMode_Dynamic;
			}
		}
		else
		{
			if (gl_flashblend->value != 0)
			{
				lightingMode = MenuLightingMode_FlashBlend;
			}
			else
			{
				lightingMode = MenuLightingMode_Static;
			}
		}
		list->curvalue = lightingMode;
	}
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuGraphics_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// Full screen flash.
//----------------------------------------
static menulist_s MenuGraphics_flash_list;

static void MenuGraphics_flash_apply()
{
	menulist_s *list = &MenuGraphics_flash_list;
	Cvar_SetValue("gl_fullscreenflash", list->curvalue);
}

static void MenuGraphics_flash_callback(void *s)
{
	MenuGraphics_flash_apply();
}

static int MenuGraphics_flash_init(int y)
{
	menulist_s *list = &MenuGraphics_flash_list;
	list->generic.type = MTYPE_SPINCONTROL;
	list->generic.name = "full screen flash";
	list->generic.x = 0;
	list->generic.y = y;
	list->generic.callback = MenuGraphics_flash_callback;
	list->itemnames = yesno_names;
	list->curvalue = (gl_fullscreenflash->value != 0);
	list->savedValue = list->curvalue;
	Menu_AddItem(&MenuGraphics_menu, (void *)list);
	y += 10;
	return y;
}

//----------------------------------------
// MenuGraphics.
//----------------------------------------
static void MenuGraphics_draw()
{
	M_Banner("m_banner_video");
	Menu_AdjustCursor(&MenuGraphics_menu, 1);
	Menu_Draw(&MenuGraphics_menu);
}

static const char* MenuGraphics_key(int key, int keyUnmodified)
{
	return Default_MenuKey(&MenuGraphics_menu, key, keyUnmodified);
}

void MenuGraphics_init()
{
	int y = 10;

	menuframework_s *menu = &MenuGraphics_menu;
	memset(menu, 0, sizeof(menuframework_s));
	MenuGraphics_menu.x = viddef.width * 0.5f;
	MenuGraphics_menu.nitems = 0;

	y = MenuGraphics_showfps_init(y);
	y = MenuGraphics_vsync_init(y);
	if (graphics_support_msaa)
	{
		y = MenuGraphics_msaa_init(y);
	}
	y = MenuGraphics_clearMode_init(y);
	if (gl_config.discardFramebuffer)
	{
		y = MenuGraphics_discardFramebuffer_init(y);
	}
	y = MenuGraphics_multitexturing_init(y);
	y = MenuGraphics_filtering_init(y);
	if (gl_config.anisotropic)
	{
		y = MenuGraphics_anisotropicFiltering_init(y);
	}
	#if defined(EGLW_GLES1)
	y = MenuGraphics_particleMode_init(y);
	#endif
	y = MenuGraphics_particleSize_init(y);
	y = MenuGraphics_shadowMode_init(y);
	y = MenuGraphics_lighting_init(y);
	y = MenuGraphics_flash_init(y);

	Menu_CenterWithBanner(&MenuGraphics_menu, "m_banner_video");
	MenuGraphics_menu.x -= 8;
}

void MenuGraphics_enter()
{
	M_PushMenu(MenuGraphics_draw, MenuGraphics_key);
}

void MenuGraphics_apply()
{
	MenuGraphics_showfps_apply();
	MenuGraphics_vsync_apply();
	if (graphics_support_msaa)
	{
		MenuGraphics_msaa_apply();
	}
	MenuGraphics_clearMode_apply();
	MenuGraphics_discardFramebuffer_apply();
	MenuGraphics_multitexturing_apply();
	MenuGraphics_filtering_apply();
	if (gl_config.anisotropic)
	{
		MenuGraphics_anisotropicFiltering_apply();
	}
	#if defined(EGLW_GLES1)
	MenuGraphics_particleMode_apply();
	#endif
	MenuGraphics_particleSize_apply();
	MenuGraphics_shadowMode_apply();
	MenuGraphics_lighting_apply();
	MenuGraphics_flash_apply();
}
