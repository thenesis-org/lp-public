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
 * Refresher setup and main part of the frame generation
 *
 * =======================================================================
 */

#include "local.h"

static void R_Strings();
static void R_Register();
static void R_SetDefaultState();

static void R_SetLightLevel();

static void R_NoTexture_Init();

static void R_Particles_Init();
static void R_Particles_Draw();

static void R_ScreenShot();

viddef_t vid;
model_t *r_worldmodel;

float gldepthmin, gldepthmax;

glconfig_t gl_config;
glstate_t gl_state;

image_t *r_notexture; /* use for bad textures */
image_t *r_particletexture; // little dot for particles.

entity_t *currententity;
model_t *currentmodel;

cplane_t frustum[4];

int r_visframecount; /* bumped when going to a new PVS */
int r_framecount; /* used for dlight push checking */

int c_brush_polys, c_alias_polys;

float v_blend[4]; /* final blending color */

/* view origin */
vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t r_origin;

float r_world_matrix[16];
float r_base_world_matrix[16];

/* screen size info */
refdef_t r_newrefdef;

int r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;
extern qboolean graphics_has_stencil;
unsigned r_rawpalette[256];

cvar_t *gl_mode;
cvar_t *gl_customwidth;
cvar_t *gl_customheight;
cvar_t *vid_fullscreen;
cvar_t *vid_gamma;

cvar_t *gl_msaa_samples;

cvar_t *gl_discardframebuffer;
cvar_t *gl_clear;
cvar_t *gl_ztrick;
cvar_t *gl_zfix;
cvar_t *gl_farsee;
cvar_t *gl_swapinterval;

cvar_t *gl_norefresh;
cvar_t *gl_drawentities;
cvar_t *gl_drawworld;
cvar_t *gl_speeds;
cvar_t *gl_novis;
cvar_t *gl_nocull;
cvar_t *gl_cull;
cvar_t *gl_lerpmodels;

cvar_t *gl_lefthand;
cvar_t *gl_lightlevel;

cvar_t *gl_multitexturing;
cvar_t *gl_fullbright;
cvar_t *gl_lightmap;
cvar_t *gl_dynamic;
cvar_t *gl_anisotropic;
cvar_t *gl_anisotropic_avail;

cvar_t *gl_round_down;
cvar_t *gl_picmip;
cvar_t *gl_retexturing;
cvar_t *gl_texturefilter;
cvar_t *gl_texturealphaformat;
cvar_t *gl_texturesolidformat;
cvar_t *gl_nobind;

cvar_t *gl_modulate;
cvar_t *gl_showtris;

cvar_t *gl_fullscreenflash;
cvar_t *gl_flashblend;
cvar_t *gl_saturatelighting;
cvar_t *gl_lockpvs;

cvar_t *gl_particle_min_size;
cvar_t *gl_particle_max_size;
cvar_t *gl_particle_size;
cvar_t *gl_particle_att_a;
cvar_t *gl_particle_att_b;
cvar_t *gl_particle_att_c;
#if defined(GLES1)
cvar_t *gl_particle_point;
cvar_t *gl_particle_sprite;
#endif

cvar_t *gl_shadows;
cvar_t *gl_stencilshadow;

cvar_t *gl_stereo;
cvar_t *gl_stereo_separation;
cvar_t *gl_stereo_convergence;
cvar_t *gl_stereo_anaglyph_colors;

cvar_t *gl_hudscale; /* named for consistency with R1Q2 */
cvar_t *gl_consolescale;
cvar_t *gl_menuscale;

int R_Init()
{
	Swap_Init();

	/* Options */
	VID_Printf(PRINT_ALL, "Refresher build options:\n");

	VID_Printf(PRINT_ALL, " + Retexturing support\n");

	VID_Printf(PRINT_ALL, "Refresh: " REF_VERSION "\n");

	Draw_GetPalette();

	R_Register();

	/* initialize OS-specific parts of OpenGL */
	if (!Renderer_Init())
	{
		return -1;
	}

	/* set our "safe" mode */
	gl_state.prev_mode = GL_MODE_DEFAULT;
	gl_state.stereo_mode = gl_stereo->value;

	/* create the window and set up the context */
	if (!Renderer_initializeWindow())
	{
		VID_Printf(PRINT_ALL, "Could not set video mode\n");
		return -1;
	}

	Cvar_Set("scr_drawall", "0");

	R_Strings();

	VID_Printf(PRINT_ALL, "\n\nProbing for OpenGL extensions:\n");
	const char *extensions_string = (const char *)glGetString(GL_EXTENSIONS);

	if (strstr(extensions_string, "GL_EXT_discard_framebuffer"))
	{
		VID_Printf(PRINT_ALL, "Using GL_EXT_discard_framebuffer\n");
		gl_config.discardFramebuffer = (PFNGLDISCARDFRAMEBUFFEREXTPROC)eglGetProcAddress("glDiscardFramebufferEXT");
	}
	else
	{
		VID_Printf(PRINT_ALL, "GL_EXT_discard_framebuffer not found\n");
		gl_config.discardFramebuffer = NULL;
	}

	if (strstr(extensions_string, "GL_EXT_texture_filter_anisotropic"))
	{
		VID_Printf(PRINT_ALL, "Using GL_EXT_texture_filter_anisotropic\n");
		gl_config.anisotropic = true;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_config.max_anisotropy);
		Cvar_SetValue("gl_anisotropic_avail", gl_config.max_anisotropy);
	}
	else
	{
		VID_Printf(PRINT_ALL, "GL_EXT_texture_filter_anisotropic not found\n");
		gl_config.anisotropic = false;
		gl_config.max_anisotropy = 0.0f;
		Cvar_SetValue("gl_anisotropic_avail", 0.0f);
	}

	#if 0
	if (strstr(extensions_string, "OES_texture_npot"))
	{
		VID_Printf(PRINT_ALL, "Using OES_texture_npot\n");
		gl_config.tex_npot = true;
	}
	#endif

	R_SetDefaultState();

	R_InitImages();
	Mod_Init();
	R_NoTexture_Init();
	R_Particles_Init();
	Draw_InitLocal();

	int err = glGetError();
	if (err != GL_NO_ERROR)
	{
		VID_Printf(PRINT_ALL, "glGetError() = 0x%x\n", err);
	}

	return true;
}

void R_Shutdown()
{
	Cmd_RemoveCommand("modellist");
	Cmd_RemoveCommand("screenshot");
	Cmd_RemoveCommand("imagelist");
	Cmd_RemoveCommand("gl_strings");

	Mod_FreeAll();

	R_ShutdownImages();

	/* shutdown OS specific OpenGL stuff like contexts, etc.  */
	Renderer_Shutdown();
}

static void R_Strings()
{
	VID_Printf(PRINT_ALL, "GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	VID_Printf(PRINT_ALL, "GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	VID_Printf(PRINT_ALL, "GL_VERSION: %s\n", glGetString(GL_VERSION));
	VID_Printf(PRINT_ALL, "GL_EXTENSIONS: %s\n", glGetString(GL_EXTENSIONS));
}

static void R_Register()
{
	gl_mode = Cvar_Get("gl_mode", GL_MODE_DEFAULT_STRING, CVAR_ARCHIVE);
	gl_customwidth = Cvar_Get("gl_customwidth", "320", CVAR_ARCHIVE);
	gl_customheight = Cvar_Get("gl_customheight", "240", CVAR_ARCHIVE);
	gl_msaa_samples = Cvar_Get("gl_msaa_samples", "0", CVAR_ARCHIVE);
	vid_fullscreen = Cvar_Get("vid_fullscreen", GL_FULLSCREEN_DEFAULT_STRING, CVAR_ARCHIVE);
	vid_gamma = Cvar_Get("vid_gamma", "1.0f", CVAR_ARCHIVE);

	gl_discardframebuffer = Cvar_Get("gl_discardframebuffer", "1", CVAR_ARCHIVE);
    #if defined(__RASPBERRY_PI_) || defined(__GCW_ZERO__) || defined(__CREATOR_CI20__)
	gl_clear = Cvar_Get("gl_clear", "1", CVAR_ARCHIVE); // Enabled by default for embedded GPU.
    #else
	gl_clear = Cvar_Get("gl_clear", "0", CVAR_ARCHIVE);
    #endif
	gl_ztrick = Cvar_Get("gl_ztrick", "0", CVAR_ARCHIVE);
	gl_zfix = Cvar_Get("gl_zfix", "0", 0);
	gl_swapinterval = Cvar_Get("gl_swapinterval", "1", CVAR_ARCHIVE);

	gl_speeds = Cvar_Get("gl_speeds", "0", 0);
	gl_norefresh = Cvar_Get("gl_norefresh", "0", 0);
	gl_drawentities = Cvar_Get("gl_drawentities", "1", 0);
	gl_drawworld = Cvar_Get("gl_drawworld", "1", 0);

	gl_novis = Cvar_Get("gl_novis", "0", 0);
	gl_nocull = Cvar_Get("gl_nocull", "0", 0);
	gl_cull = Cvar_Get("gl_cull", "1", 0);
	gl_lockpvs = Cvar_Get("gl_lockpvs", "0", 0);

	gl_lefthand = Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	gl_farsee = Cvar_Get("gl_farsee", "0", CVAR_LATCH | CVAR_ARCHIVE);
	gl_lerpmodels = Cvar_Get("gl_lerpmodels", "1", 0);
	gl_lightlevel = Cvar_Get("gl_lightlevel", "0", 0);
	gl_modulate = Cvar_Get("gl_modulate", "1", CVAR_ARCHIVE);
	gl_saturatelighting = Cvar_Get("gl_saturatelighting", "0", 0);

	gl_particle_min_size = Cvar_Get("gl_particle_min_size", "2", CVAR_ARCHIVE);
	gl_particle_max_size = Cvar_Get("gl_particle_max_size", "40", CVAR_ARCHIVE);
	gl_particle_size = Cvar_Get("gl_particle_size", "40", CVAR_ARCHIVE);
	gl_particle_att_a = Cvar_Get("gl_particle_att_a", "0.01", CVAR_ARCHIVE);
	gl_particle_att_b = Cvar_Get("gl_particle_att_b", "0.0", CVAR_ARCHIVE);
	gl_particle_att_c = Cvar_Get("gl_particle_att_c", "0.01", CVAR_ARCHIVE);
	#if defined(GLES1)
    // Points and point sprites are disabled by default because they are not well supported by most GLES 1 implementations.
	gl_particle_point = Cvar_Get("gl_particle_point", "0", CVAR_ARCHIVE);
	gl_particle_sprite = Cvar_Get("gl_particle_sprite", "0", CVAR_ARCHIVE);
	#endif

    // Multitexturing is disabled by default because it is slow on embedded platforms. When used, textures cannot be sorted so it causes more state changes.
	gl_multitexturing = Cvar_Get("gl_multitexturing", "0", CVAR_ARCHIVE);
	gl_fullbright = Cvar_Get("gl_fullbright", "0", 0);
	gl_lightmap = Cvar_Get("gl_lightmap", "0", 0);
    #if defined(__RASPBERRY_PI_) || defined(__GCW_ZERO__) || defined(__CREATOR_CI20__)
    // Dynamic lightmaps are disabled by default on embedded platforms because it currently causes huge slow downs, or glitches (GCW Zero).
    // With tile rendering, used by most embedded GPU, the whole frame must be flushed before updating a used texture.
	gl_dynamic = Cvar_Get("gl_dynamic", "0", CVAR_ARCHIVE);
	gl_flashblend = Cvar_Get("gl_flashblend", "1", CVAR_ARCHIVE);
    #else
	gl_dynamic = Cvar_Get("gl_dynamic", "1", CVAR_ARCHIVE);
	gl_flashblend = Cvar_Get("gl_flashblend", "0", CVAR_ARCHIVE);
    #endif
	gl_nobind = Cvar_Get("gl_nobind", "0", 0);

	gl_showtris = Cvar_Get("gl_showtris", "0", 0);
	gl_fullscreenflash = Cvar_Get("gl_fullscreenflash", "1", 0);

	gl_retexturing = Cvar_Get("gl_retexturing", "1", CVAR_ARCHIVE);
	gl_texturefilter = Cvar_Get("gl_texturefilter", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE);
	gl_anisotropic = Cvar_Get("gl_anisotropic", "2", CVAR_ARCHIVE);
	gl_anisotropic_avail = Cvar_Get("gl_anisotropic_avail", "0", 0);
	gl_texturealphaformat = Cvar_Get("gl_texturealphaformat", "default", CVAR_ARCHIVE);
	gl_texturesolidformat = Cvar_Get("gl_texturesolidformat", "default", CVAR_ARCHIVE);
	gl_round_down = Cvar_Get("gl_round_down", "1", 0);
	gl_picmip = Cvar_Get("gl_picmip", "0", 0);

	gl_shadows = Cvar_Get("gl_shadows", "1", CVAR_ARCHIVE);
	gl_stencilshadow = Cvar_Get("gl_stencilshadow", "1", CVAR_ARCHIVE);

	gl_stereo = Cvar_Get("gl_stereo", "0", CVAR_ARCHIVE);
	gl_stereo_separation = Cvar_Get("gl_stereo_separation", "-0.4", CVAR_ARCHIVE);
	gl_stereo_convergence = Cvar_Get("gl_stereo_convergence", "1", CVAR_ARCHIVE);
	gl_stereo_anaglyph_colors = Cvar_Get("gl_stereo_anaglyph_colors", "rc", CVAR_ARCHIVE);

	Cmd_AddCommand("imagelist", R_ImageList_f);
	Cmd_AddCommand("screenshot", R_ScreenShot);
	Cmd_AddCommand("modellist", Mod_Modellist_f);
	Cmd_AddCommand("gl_strings", R_Strings);
}

static void R_SetDefaultState()
{
	glCullFace(GL_FRONT);
	glDisable(GL_CULL_FACE);

	oglwEnableSmoothShading(false);

	oglwEnableTexturing(0, true);

	R_TextureMode(gl_texturefilter->string);
	R_TextureAlphaMode(gl_texturealphaformat->string);
	R_TextureSolidMode(gl_texturesolidformat->string);

	oglwSetTextureBlending(0, GL_REPLACE);

	#if defined(GLES1)
	{
		float attenuations[3];

		attenuations[0] = gl_particle_att_a->value;
		attenuations[1] = gl_particle_att_b->value;
		attenuations[2] = gl_particle_att_c->value;

		/* GL_POINT_SMOOTH is not implemented by some OpenGL
		   drivers, especially the crappy Mesa3D backends like
		   i915.so. That the points are squares and not circles
		   is not a problem by Quake II! */
		glEnable(GL_POINT_SMOOTH);
		glPointParameterf(GL_POINT_SIZE_MIN,
			gl_particle_min_size->value);
		glPointParameterf(GL_POINT_SIZE_MAX,
			gl_particle_max_size->value);
		glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION, attenuations);
	}
	#endif

	#if defined(GLES1)
	if (gl_msaa_samples->value)
		glEnable(GL_MULTISAMPLE);
	else
		glDisable(GL_MULTISAMPLE);
	#endif

	oglwEnableBlending(false);
	oglwSetBlendingFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	oglwEnableAlphaTest(true);

	oglwEnableDepthTest(false);

	oglwEnableStencilTest(false);

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	oglwEnableDepthWrite(true);
	glStencilMask(0xffffffff);

	glClearColor(1, 0, 0.5f, 0.5f);
	glClearDepthf(1.0f);
	glClearStencil(0);
	oglwClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void R_SetPalette(const unsigned char *palette)
{
	int i;

	byte *rp = (byte *)r_rawpalette;

	if (palette)
	{
		for (i = 0; i < 256; i++)
		{
			rp[i * 4 + 0] = palette[i * 3 + 0];
			rp[i * 4 + 1] = palette[i * 3 + 1];
			rp[i * 4 + 2] = palette[i * 3 + 2];
			rp[i * 4 + 3] = 0xff;
		}
	}
	else
	{
		for (i = 0; i < 256; i++)
		{
			unsigned char *pc = d_8to24table[i];
			rp[i * 4 + 0] = pc[0];
			rp[i * 4 + 1] = pc[1];
			rp[i * 4 + 2] = pc[2];
			rp[i * 4 + 3] = 0xff;
		}
	}
}

extern void Renderer_Gamma_Update();

void R_BeginFrame(float camera_separation, int eyeFrame)
{
	gl_state.camera_separation = camera_separation;

	/* change modes if necessary */
	if (gl_mode->modified)
	{
		vid_fullscreen->modified = true;
	}

	// force a vid_restart if gl_stereo has been modified.
	if (gl_state.stereo_mode != gl_stereo->value)
	{
		gl_state.stereo_mode = gl_stereo->value;
	}

	if (vid_gamma->modified)
	{
		vid_gamma->modified = false;

		if (gl_state.hwgamma)
		{
			Renderer_Gamma_Update();
		}
	}

	/* go into 2D mode */

	qboolean stereo_split_tb = ((gl_state.stereo_mode == STEREO_SPLIT_VERTICAL) && gl_state.camera_separation);
	qboolean stereo_split_lr = ((gl_state.stereo_mode == STEREO_SPLIT_HORIZONTAL) && gl_state.camera_separation);

	int x = 0;
	int w = vid.width;
	int y = 0;
	int h = vid.height;

	if (stereo_split_lr)
	{
		w = w / 2;
		x = eyeFrame == 0 ? 0 : w;
	}

	if (stereo_split_tb)
	{
		h = h / 2;
		y = eyeFrame == 0 ? h : 0;
	}

	glViewport(x, y, w, h);

	oglwMatrixMode(GL_PROJECTION);
	oglwLoadIdentity();
	oglwOrtho(0, vid.width, vid.height, 0, -99999, 99999);
	oglwMatrixMode(GL_MODELVIEW);
	oglwLoadIdentity();

	glDisable(GL_CULL_FACE);

	/* texturemode stuff */
	if (gl_texturefilter->modified || (gl_config.anisotropic && gl_anisotropic->modified))
	{
		R_TextureMode(gl_texturefilter->string);
		gl_texturefilter->modified = false;
		gl_anisotropic->modified = false;
	}

	if (gl_texturealphaformat->modified)
	{
		R_TextureAlphaMode(gl_texturealphaformat->string);
		gl_texturealphaformat->modified = false;
	}

	if (gl_texturesolidformat->modified)
	{
		R_TextureSolidMode(gl_texturesolidformat->string);
		gl_texturesolidformat->modified = false;
	}

	oglwEnableBlending(false);
	oglwEnableAlphaTest(true);
	oglwEnableDepthTest(false);
	oglwEnableStencilTest(false);

	oglwEnableDepthWrite(true);

	/* clear screen if desired */
	R_Clear(eyeFrame);
}

void R_Clear(int eyeFrame)
{
	GLbitfield clearFlags = 0;

	// Color buffer.
	if (gl_clear->value && eyeFrame == 0)
	{
		#if defined(DEBUG)
		glClearColor(1.0f, 0.0f, 0.5f, 0.5f);
		#else
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		#endif
		clearFlags |= GL_COLOR_BUFFER_BIT;
	}

	// Depth buffer.
	if (gl_ztrick->value)
	{
		static int trickframe = 0;
		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			glDepthFunc(GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5f;
			glDepthFunc(GL_GEQUAL);
		}
	}
	else
	{
		clearFlags |= GL_DEPTH_BUFFER_BIT;
		gldepthmin = 0;
		gldepthmax = 1;
		glClearDepthf(1.0f);
		glDepthFunc(GL_LEQUAL);
	}

	// Stencil buffer shadows.
	if (gl_shadows->value && graphics_has_stencil && gl_stencilshadow->value)
	{
		glClearStencil(0);
		clearFlags |= GL_STENCIL_BUFFER_BIT;
	}

	if (clearFlags != 0)
	{
		oglwClear(clearFlags);
	}

	glDepthRangef(gldepthmin, gldepthmax);

	if (gl_zfix->value)
	{
		if (gldepthmax > gldepthmin)
		{
			glPolygonOffset(0.05f, 1);
		}
		else
		{
			glPolygonOffset(-0.05f, -1);
		}
	}
}

//----------------------------------------
// 2D rendering.
//----------------------------------------
void R_SetGL2D()
{
	/* set 2D virtual screen size */
	qboolean drawing_left_eye = gl_state.camera_separation < 0;
	qboolean stereo_split_tb = ((gl_state.stereo_mode == STEREO_SPLIT_VERTICAL) && gl_state.camera_separation);
	qboolean stereo_split_lr = ((gl_state.stereo_mode == STEREO_SPLIT_HORIZONTAL) && gl_state.camera_separation);

	int x = 0;
	int w = vid.width;
	int y = 0;
	int h = vid.height;

	if (stereo_split_lr)
	{
		w = w / 2;
		x = drawing_left_eye ? 0 : w;
	}

	if (stereo_split_tb)
	{
		h = h / 2;
		y = drawing_left_eye ? h : 0;
	}

	glViewport(x, y, w, h);
	oglwMatrixMode(GL_PROJECTION);
	oglwLoadIdentity();
	oglwOrtho(0, vid.width, vid.height, 0, -99999, 99999);
	oglwMatrixMode(GL_MODELVIEW);
	oglwLoadIdentity();

	glDisable(GL_CULL_FACE);

	oglwEnableBlending(false);
	oglwEnableAlphaTest(true);
	oglwEnableDepthTest(false);
	oglwEnableStencilTest(false);

	oglwEnableDepthWrite(false);
}

//----------------------------------------
// 3D rendering.
//----------------------------------------
static void R_SetupFrame();
static void R_SetFrustum();
static void R_SetupGL();
static void R_DrawEntitiesOnList();
static void R_DrawBeam(entity_t *e);
static void R_PolyBlend();

void R_RenderFrame(refdef_t *fd)
{
	R_RenderView(fd);
	R_SetLightLevel();
	R_SetGL2D();
}

// r_newrefdef must be set before the first call
void R_RenderView(refdef_t *fd)
{
	if (gl_norefresh->value)
	{
		return;
	}

	if ((gl_state.stereo_mode != STEREO_MODE_NONE) && gl_state.camera_separation)
	{
		qboolean drawing_left_eye = gl_state.camera_separation < 0;
		switch (gl_state.stereo_mode)
		{
		case STEREO_MODE_ANAGLYPH:
		{
			// Work out the colour for each eye.
			int anaglyph_colours[] = { 0x4, 0x3 };                 // Left = red, right = cyan.

			if (Q_strlen(gl_stereo_anaglyph_colors->string) == 2)
			{
				int eye, colour, missing_bits;
				// Decode the colour name from its character.
				for (eye = 0; eye < 2; ++eye)
				{
					colour = 0;
					switch (toupper(gl_stereo_anaglyph_colors->string[eye]))
					{
					case 'B': ++colour;                         // 001 Blue
					case 'G': ++colour;                         // 010 Green
					case 'C': ++colour;                         // 011 Cyan
					case 'R': ++colour;                         // 100 Red
					case 'M': ++colour;                         // 101 Magenta
					case 'Y': ++colour;                         // 110 Yellow
						anaglyph_colours[eye] = colour;
						break;
					}
				}
				// Fill in any missing bits.
				missing_bits = ~(anaglyph_colours[0] | anaglyph_colours[1]) & 0x3;
				for (eye = 0; eye < 2; ++eye)
				{
					anaglyph_colours[eye] |= missing_bits;
				}
			}

			// Set the current colour.
			glColorMask(
				!!(anaglyph_colours[drawing_left_eye] & 0x4),
				!!(anaglyph_colours[drawing_left_eye] & 0x2),
				!!(anaglyph_colours[drawing_left_eye] & 0x1),
				GL_TRUE
			        );
		}
		break;
		default:
			break;
		}
	}

	r_newrefdef = *fd;

	if (!r_worldmodel && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		VID_Error(ERR_DROP, "R_RenderView: NULL worldmodel");
	}

	if (gl_speeds->value)
	{
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	R_PushDlights();

	R_SetupFrame();

	R_SetFrustum();

	R_SetupGL();

	R_MarkLeaves(); /* done here so we know if we're in water */

	R_DrawWorld();

	R_DrawEntitiesOnList();

	R_RenderDlights();

	R_Particles_Draw();

	R_DrawAlphaSurfaces(1.0f, 0);

	R_PolyBlend();

	if (gl_speeds->value)
	{
		VID_Printf(PRINT_ALL, "%4i wpoly %4i epoly %i tex %i lmaps\n",
			c_brush_polys, c_alias_polys, c_visible_textures,
			c_visible_lightmaps);
	}

	switch (gl_state.stereo_mode)
	{
	case STEREO_MODE_ANAGLYPH:
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		break;
	default:
		break;
	}
}

static void R_SetupFrame()
{
	r_framecount++;

	/* build the transformation matrix for the given view angles */
	VectorCopy(r_newrefdef.vieworg, r_origin);

	AngleVectors(r_newrefdef.viewangles, vpn, vright, vup);

	/* current viewcluster */
	if (!(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		mleaf_t *leaf = Mod_PointInLeaf(r_origin, r_worldmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		/* check above and below so crossing solid water doesn't draw wrong */
		if (!leaf->contents)
		{
			/* look down a bit */
			vec3_t temp;

			VectorCopy(r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf(temp, r_worldmodel);

			if (!(leaf->contents & CONTENTS_SOLID) &&
			    (leaf->cluster != r_viewcluster2))
			{
				r_viewcluster2 = leaf->cluster;
			}
		}
		else
		{
			/* look up a bit */
			vec3_t temp;

			VectorCopy(r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf(temp, r_worldmodel);

			if (!(leaf->contents & CONTENTS_SOLID) &&
			    (leaf->cluster != r_viewcluster2))
			{
				r_viewcluster2 = leaf->cluster;
			}
		}
	}

	for (int i = 0; i < 4; i++)
	{
		v_blend[i] = r_newrefdef.blend[i];
	}

	c_brush_polys = 0;
	c_alias_polys = 0;

	/* clear out the portion of the screen that the NOWORLDMODEL defines */
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		oglwEnableDepthWrite(true);
		glEnable(GL_SCISSOR_TEST);
		glClearColor(0.3, 0.3, 0.3, 1);
		glScissor(r_newrefdef.x,
			vid.height - r_newrefdef.height - r_newrefdef.y,
			r_newrefdef.width, r_newrefdef.height);
		oglwClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glClearColor(1, 0, 0.5f, 0.5f);
		glDisable(GL_SCISSOR_TEST);
	}
}

void R_MYgluPerspective(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar)
{
	GLfloat xmin, xmax, ymin, ymax;

	ymax = zNear * tanf(fovy * Q_PI / 360.0f);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	xmin += -gl_stereo_convergence->value * (2 * gl_state.camera_separation) / zNear;
	xmax += -gl_stereo_convergence->value * (2 * gl_state.camera_separation) / zNear;

	oglwFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

static int R_SignbitsForPlane(cplane_t *out)
{
	/* for fast box on planeside test */
	int bits = 0;
	for (int j = 0; j < 3; j++)
	{
		if (out->normal[j] < 0)
		{
			bits |= 1 << j;
		}
	}
	return bits;
}

static void R_SetFrustum()
{
	/* rotate VPN right by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[0].normal, vup, vpn,
		-(90 - r_newrefdef.fov_x / 2));
	/* rotate VPN left by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[1].normal,
		vup, vpn, 90 - r_newrefdef.fov_x / 2);
	/* rotate VPN up by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[2].normal,
		vright, vpn, 90 - r_newrefdef.fov_y / 2);
	/* rotate VPN down by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[3].normal, vright, vpn,
		-(90 - r_newrefdef.fov_y / 2));

	for (int i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
		frustum[i].signbits = R_SignbitsForPlane(&frustum[i]);
	}
}

static void R_SetupGL()
{
	/* set up viewport */
	int x = floorf(r_newrefdef.x * vid.width / vid.width);
	int x2 = ceilf((r_newrefdef.x + r_newrefdef.width) * vid.width / vid.width);
	int y = floorf(vid.height - r_newrefdef.y * vid.height / vid.height);
	int y2 = ceilf(vid.height -
			(r_newrefdef.y + r_newrefdef.height) * vid.height / vid.height);

	int w = x2 - x;
	int h = y - y2;

	qboolean drawing_left_eye = gl_state.camera_separation < 0;
	qboolean stereo_split_tb = ((gl_state.stereo_mode == STEREO_SPLIT_VERTICAL) && gl_state.camera_separation);
	qboolean stereo_split_lr = ((gl_state.stereo_mode == STEREO_SPLIT_HORIZONTAL) && gl_state.camera_separation);

	if (stereo_split_lr)
	{
		w = w / 2;
		x = drawing_left_eye ? (x / 2) : (x + vid.width) / 2;
	}

	if (stereo_split_tb)
	{
		h = h / 2;
		y2 = drawing_left_eye ? (y2 + vid.height) / 2 : (y2 / 2);
	}

	glViewport(x, y2, w, h);

	/* set up projection matrix */
	float screenaspect = (float)r_newrefdef.width / r_newrefdef.height;
	oglwMatrixMode(GL_PROJECTION);
	oglwLoadIdentity();

	if (gl_farsee->value == 0)
	{
		R_MYgluPerspective(r_newrefdef.fov_y, screenaspect, 4, 4096);
	}
	else
	{
		R_MYgluPerspective(r_newrefdef.fov_y, screenaspect, 4, 8192);
	}

	oglwMatrixMode(GL_MODELVIEW);
	oglwLoadIdentity();

	oglwRotate(-90, 1, 0, 0); /* put Z going up */
	oglwRotate(90, 0, 0, 1); /* put Z going up */
	oglwRotate(-r_newrefdef.viewangles[2], 1, 0, 0);
	oglwRotate(-r_newrefdef.viewangles[0], 0, 1, 0);
	oglwRotate(-r_newrefdef.viewangles[1], 0, 0, 1);
	oglwTranslate(-r_newrefdef.vieworg[0], -r_newrefdef.vieworg[1],
		-r_newrefdef.vieworg[2]);

	oglwGetMatrix(GL_MODELVIEW, r_world_matrix);

	glCullFace(GL_FRONT);
	if (gl_cull->value)
	{
		glEnable(GL_CULL_FACE);
	}
	else
	{
		glDisable(GL_CULL_FACE);
	}

	oglwEnableBlending(false);
	oglwEnableAlphaTest(false);
	oglwEnableDepthTest(true);
	oglwEnableStencilTest(false);

	oglwEnableDepthWrite(true);
	if (gl_shadows->value && graphics_has_stencil && gl_stencilshadow->value)
	{
		glStencilFunc(GL_EQUAL, 0, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
	}
}

// Returns true if the box is completely outside the frustom
qboolean R_CullBox(vec3_t mins, vec3_t maxs)
{
	if (gl_nocull->value)
	{
		return false;
	}

	for (int i = 0; i < 4; i++)
	{
		if (BOX_ON_PLANE_SIDE(mins, maxs, &frustum[i]) == 2)
		{
			return true;
		}
	}

	return false;
}

void R_RotateForEntity(entity_t *e)
{
	oglwTranslate(e->origin[0], e->origin[1], e->origin[2]);

	oglwRotate(e->angles[1], 0, 0, 1);
	oglwRotate(-e->angles[0], 0, 1, 0);
	oglwRotate(-e->angles[2], 1, 0, 0);
}

#define NUM_BEAM_SEGS 6

static void R_DrawBeam(entity_t *e)
{
	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if (VectorNormalize(normalized_direction) == 0)
	{
		return;
	}

	PerpendicularVector(perpvec, normalized_direction);
	VectorScale(perpvec, e->frame / 2, perpvec);

	for (int i = 0; i < 6; i++)
	{
		RotatePointAroundVector(start_points[i], normalized_direction, perpvec,
			(360.0f / NUM_BEAM_SEGS) * i);
		VectorAdd(start_points[i], origin, start_points[i]);
		VectorAdd(start_points[i], direction, end_points[i]);
	}

	oglwEnableTexturing(0, GL_FALSE);

	oglwBegin(GL_TRIANGLES);

	unsigned char *pc = d_8to24table[e->skinnum & 0xff];
	float r = pc[0] * (1 / 255.0f);
	float g = pc[1] * (1 / 255.0f);
	float b = pc[2] * (1 / 255.0f);
	float a = e->alpha;

	OpenGLWrapperVertex *vtx;
	vtx = oglwAllocateTriangleStrip(NUM_BEAM_SEGS * 4);
	for (int i = 0; i < NUM_BEAM_SEGS; i++)
	{
		float *p;
		p = start_points[i];
		vtx = AddVertex3D_C(vtx, p[0], p[1], p[2], r, g, b, a);
		p = end_points[i];
		vtx = AddVertex3D_C(vtx, p[0], p[1], p[2], r, g, b, a);
		p = start_points[(i + 1) % NUM_BEAM_SEGS];
		vtx = AddVertex3D_C(vtx, p[0], p[1], p[2], r, g, b, a);
		p = end_points[(i + 1) % NUM_BEAM_SEGS];
		vtx = AddVertex3D_C(vtx, p[0], p[1], p[2], r, g, b, a);
	}

	oglwEnd();

	oglwEnableTexturing(0, GL_TRUE);
}

void R_DrawSpriteModel(entity_t *e)
{
	/* don't even bother culling, because it's just
	   a single polygon without a surface cache */
	dsprite_t *psprite = (dsprite_t *)currentmodel->extradata;

	e->frame %= psprite->numframes;
	dsprframe_t *frame = &psprite->frames[e->frame];

	/* normal sprite */
	float *up = vup;
	float *right = vright;

	oglwBindTexture(0, currentmodel->skins[e->frame]->texnum);
	oglwSetTextureBlending(0, GL_MODULATE);

	float alpha = 1.0F;
	if (e->flags & RF_TRANSLUCENT)
	{
		alpha = e->alpha;
	}
	if (alpha == 1.0f)
	{
		oglwEnableAlphaTest(true);
	}

	oglwBegin(GL_TRIANGLES);

	OpenGLWrapperVertex *vtx = oglwAllocateQuad(4);

	vec3_t p;

	VectorMA(e->origin, -frame->origin_y, up, p);
	VectorMA(p, -frame->origin_x, right, p);
	vtx = AddVertex3D_CT1(vtx, p[0], p[1], p[2], 1.0f, 1.0f, 1.0f, alpha, 0.0f, 1.0f);

	VectorMA(e->origin, frame->height - frame->origin_y, up, p);
	VectorMA(p, -frame->origin_x, right, p);
	vtx = AddVertex3D_CT1(vtx, p[0], p[1], p[2], 1.0f, 1.0f, 1.0f, alpha, 0.0f, 0.0f);

	VectorMA(e->origin, frame->height - frame->origin_y, up, p);
	VectorMA(p, frame->width - frame->origin_x, right, p);
	vtx = AddVertex3D_CT1(vtx, p[0], p[1], p[2], 1.0f, 1.0f, 1.0f, alpha, 1.0f, 0.0f);

	VectorMA(e->origin, -frame->origin_y, up, p);
	VectorMA(p, frame->width - frame->origin_x, right, p);
	vtx = AddVertex3D_CT1(vtx, p[0], p[1], p[2], 1.0f, 1.0f, 1.0f, alpha, 1.0f, 1.0f);

	oglwEnd();

	if (alpha == 1.0f)
	{
		oglwEnableAlphaTest(false);
	}

	oglwSetTextureBlending(0, GL_REPLACE);
}

void R_DrawNullModel()
{
	vec3_t shadelight;
	if (currententity->flags & RF_FULLBRIGHT)
	{
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0F;
	}
	else
	{
		R_LightPoint(currententity->origin, shadelight);
	}

	float alpha = 1.0F;
	if (currententity->flags & RF_TRANSLUCENT)
	{
		alpha = currententity->alpha;
	}

	oglwPushMatrix();
	R_RotateForEntity(currententity);

	oglwEnableTexturing(0, GL_FALSE);

	oglwBegin(GL_TRIANGLES);

	OpenGLWrapperVertex *vtx;

	vtx = oglwAllocateTriangleFan(6);
	vtx = AddVertex3D_C(vtx, 0.0f, 0.0f, -16.0f, shadelight[0], shadelight[1], shadelight[2], alpha);
	for (int i = 0; i <= 4; i++)
	{
		vtx = AddVertex3D_C(vtx, 16 * cosf(i * Q_PI / 2), 16 * sinf(i * Q_PI / 2), 0.0f, shadelight[0], shadelight[1], shadelight[2], alpha);
	}

	vtx = oglwAllocateTriangleFan(6);
	vtx = AddVertex3D_C(vtx, 0.0f, 0.0f, 16.0f, shadelight[0], shadelight[1], shadelight[2], alpha);
	for (int i = 4; i >= 0; i--)
	{
		vtx = AddVertex3D_C(vtx, 16 * cosf(i * Q_PI / 2), 16 * sinf(i * Q_PI / 2), 0.0f, shadelight[0], shadelight[1], shadelight[2], alpha);
	}

	oglwEnd();

	oglwPopMatrix();

	oglwEnableTexturing(0, GL_TRUE);
}

static void R_DrawCurrentEntity()
{
	if (currententity->flags & RF_BEAM)
	{
		R_DrawBeam(currententity);
	}
	else
	{
		currentmodel = currententity->model;
		if (!currentmodel)
		{
			R_DrawNullModel();
			return;
		}
		switch (currentmodel->type)
		{
		case mod_alias:
			R_DrawAliasModel(currententity);
			break;
		case mod_brush:
			R_DrawBrushModel(currententity);
			break;
		case mod_sprite:
			R_DrawSpriteModel(currententity);
			break;
		default:
			VID_Error(ERR_DROP, "Bad modeltype");
			break;
		}
	}
}

static void R_DrawEntitiesOnList()
{
	if (!gl_drawentities->value)
	{
		return;
	}

	/* draw non-transparent first */
	for (int i = 0; i < r_newrefdef.num_entities; i++)
	{
		currententity = &r_newrefdef.entities[i];
		if (currententity->flags & RF_TRANSLUCENT)
		{
			continue;
		}
		R_DrawCurrentEntity();
	}

	/* draw transparent entities
	   we could sort these if it ever
	   becomes a problem... */
	oglwEnableBlending(true);
	oglwEnableDepthWrite(false);
	for (int i = 0; i < r_newrefdef.num_entities; i++)
	{
		currententity = &r_newrefdef.entities[i];
		if (!(currententity->flags & RF_TRANSLUCENT))
		{
			continue;
		}
		R_DrawCurrentEntity();
	}
	oglwEnableBlending(false);
	oglwEnableDepthWrite(true);
}

static void R_SetLightLevel()
{
	vec3_t shadelight;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		return;
	}

	/* save off light value for server to look at */
	R_LightPoint(r_newrefdef.vieworg, shadelight);

	/* pick the greatest component, which should be the
	 * same as the mono value returned by software */
	if (shadelight[0] > shadelight[1])
	{
		if (shadelight[0] > shadelight[2])
		{
			gl_lightlevel->value = 150 * shadelight[0];
		}
		else
		{
			gl_lightlevel->value = 150 * shadelight[2];
		}
	}
	else
	{
		if (shadelight[1] > shadelight[2])
		{
			gl_lightlevel->value = 150 * shadelight[1];
		}
		else
		{
			gl_lightlevel->value = 150 * shadelight[2];
		}
	}
}

static void R_PolyBlend()
{
	if (!gl_fullscreenflash->value)
	{
		return;
	}

	if (!v_blend[3])
	{
		return;
	}

	oglwEnableBlending(true);
	oglwEnableDepthTest(false);
	oglwEnableDepthWrite(false);

	oglwEnableTexturing(0, GL_FALSE);

	oglwLoadIdentity();

	oglwRotate(-90, 1, 0, 0); /* put Z going up */
	oglwRotate(90, 0, 0, 1); /* put Z going up */

	oglwBegin(GL_TRIANGLES);
	OpenGLWrapperVertex *vtx = oglwAllocateQuad(4);
	vtx = AddVertex3D_C(vtx, 10, 100, 100, v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
	vtx = AddVertex3D_C(vtx, 10, -100, 100, v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
	vtx = AddVertex3D_C(vtx, 10, -100, -100, v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
	vtx = AddVertex3D_C(vtx, 10, 100, -100, v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
	oglwEnd();

	oglwEnableTexturing(0, GL_TRUE);

	oglwEnableBlending(false);
	oglwEnableDepthTest(true);
	oglwEnableDepthWrite(true);
}

//--------------------------------------------------------------------------------
// No texture.
//--------------------------------------------------------------------------------
static void R_NoTexture_Init()
{
    static const byte dottexture[8][8] =
    {
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 1, 1, 0, 0, 0 },
        { 0, 0, 1, 1, 1, 1, 0, 0 },
        { 0, 1, 1, 1, 1, 1, 1, 0 },
        { 0, 1, 1, 1, 1, 1, 1, 0 },
        { 0, 0, 1, 1, 1, 1, 0, 0 },
        { 0, 0, 0, 1, 1, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
    };

	// Use the dot for bad textures, without alpha.
	byte data[8][8][4];
	for (int y = 0; y < 8; y++)
	{
		for (int x = 0; x < 8; x++)
		{
			data[y][x][0] = dottexture[x][y] * 255;
			data[y][x][1] = 0;
			data[y][x][2] = 0;
			data[y][x][3] = 255;
		}
	}
	r_notexture = R_LoadPic("***r_notexture***", (byte *)data, 8, 0, 8, 0, it_wall, 32);
}

//--------------------------------------------------------------------------------
// Particles.
//--------------------------------------------------------------------------------
static void R_Particles_Init()
{
	// Particle texture.
	{
		int particleSize = 16;
		byte *particleData = malloc(particleSize * particleSize * 4);
		float radius = (float)((particleSize >> 1) - 2), radius2 = radius * radius, center = (float)(particleSize >> 1);
		for (int y = 0; y < particleSize; y++)
		{
			for (int x = 0; x < particleSize; x++)
			{
				float rx = (float)x - center, ry = (float)y - center;
				float rd2 = rx * rx + ry * ry;
				float a;
				if (rd2 > radius2)
				{
					a = 1.0f - (sqrtf(rd2) - radius);
					if (a < 0.0f)
						a = 0.0f;
					if (a > 1.0f)
						a = 1.0f;
				}
				else
					a = 1.0f;
				byte *d = &particleData[(x + y * particleSize) * 4];
				d[0] = 255;
				d[1] = 255;
				d[2] = 255;
				d[3] = (byte)(a * 255.0f);
			}
		}
		r_particletexture = R_LoadPic("***particle***", particleData, particleSize, 0, particleSize, 0, it_sprite, 32);
		free(particleData);
	}
}

static float R_Particles_computeSize(float distance2, float distance)
{
	float attenuation = gl_particle_att_a->value;
	float factorB = gl_particle_att_b->value;
	float factorC = gl_particle_att_c->value;
	float particleSize = gl_particle_size->value;
	float particleMinSize = gl_particle_min_size->value;
	float particleMaxSize = gl_particle_max_size->value;

	if (factorB > 0.0f || factorC > 0.0f)
	{
		attenuation += factorC * distance2;
		if (factorB > 0.0f)
		{
			attenuation += factorB * distance;
		}
	}
	float size;
	if (attenuation <= 0.0f)
	{
		size = particleSize;
	}
	else
	{
		attenuation = 1.0f / sqrtf(attenuation);
		size = particleSize * attenuation;
		if (size < particleMinSize)
			size = particleMinSize;
		if (size > particleMaxSize)
			size = particleMaxSize;
	}
	if (size < 1.0f)
		size = 1.0f;

	return size;
}

static void R_Particles_DrawWithQuads(int num_particles, const particle_t particles[])
{
	oglwBindTexture(0, r_particletexture->texnum);
	oglwSetTextureBlending(0, GL_MODULATE);

	vec3_t up, right;
	VectorScale(vup, 1.0f, up);
	VectorScale(vright, 1.0f, right);

	oglwBegin(GL_TRIANGLES);
	OpenGLWrapperVertex *vtx = oglwAllocateQuad(num_particles * 4);

	float pixelWidthAtDepth1 = 2.0f * tanf(r_newrefdef.fov_x * Q_PI / 360.0f) / (float)r_newrefdef.width; // Pixel width if the near plane is at depth 1.0.

	// The dot is 14 pixels instead of 16 because of borders. Take it into account.
	pixelWidthAtDepth1 *= 16.0f / 14.0f;

	int i;
	const particle_t *p;
	for (p = particles, i = 0; i < num_particles; i++, p++)
	{
		float dx = p->origin[0] - r_origin[0], dy = p->origin[1] - r_origin[1], dz = p->origin[2] - r_origin[2];
		float distance2 = dx * dx + dy * dy + dz * dz;
		float distance = sqrtf(distance2);

		// Size in pixels, like OpenGLES.
		float size = R_Particles_computeSize(distance2, distance);

		// Size in world space.
		size = size * pixelWidthAtDepth1 * distance;

		unsigned char *pc = d_8to24table[p->color & 0xff];
		float r = pc[0] * (1.0f / 255.0f);
		float g = pc[1] * (1.0f / 255.0f);
		float b = pc[2] * (1.0f / 255.0f);
		float a = p->alpha;

		float rx = size * right[0], ry = size * right[1], rz = size * right[2];
		float ux = size * up[0], uy = size * up[1], uz = size * up[2];

		// Center each quad.
		float px = p->origin[0] - 0.5f * (rx + ux);
		float py = p->origin[1] - 0.5f * (ry + uy);
		float pz = p->origin[2] - 0.5f * (rz + uz);

		vtx = AddVertex3D_CT1(vtx, px, py, pz, r, g, b, a, 0.0f, 0.0f);
		vtx = AddVertex3D_CT1(vtx, px + ux, py + uy, pz + uz, r, g, b, a, 1.0f, 0.0f);
		vtx = AddVertex3D_CT1(vtx, px + ux + rx, py + uy + ry, pz + uz + rz, r, g, b, a, 1.0f, 1.0f);
		vtx = AddVertex3D_CT1(vtx, px + rx, py + ry, pz + rz, r, g, b, a, 0.0f, 1.0f);
	}
	oglwEnd();

	oglwSetTextureBlending(0, GL_REPLACE);
}

#if !defined(GLES2)
static void R_Particles_DrawWithPoints(int num_particles, const particle_t particles[])
{
	bool particleTextureUsed = gl_particle_sprite->value != 0.0f;
	if (particleTextureUsed)
	{
		glEnable(GL_POINT_SPRITE_OES);
		glTexEnvi(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE);
		oglwBindTexture(0, r_particletexture->texnum);
		oglwSetTextureBlending(0, GL_MODULATE);
	}
	else
	{
		oglwEnableTexturing(0, GL_FALSE);
	}

	glPointSize(gl_particle_size->value);

	oglwBegin(GL_POINTS);
	OpenGLWrapperVertex *vtx = oglwAllocateVertex(num_particles);

	int i;
	const particle_t *p;
	for (i = 0, p = particles; i < num_particles; i++, p++)
	{
		unsigned char *pc = d_8to24table[p->color & 0xff];
		float r = pc[0] * (1.0f / 255.0f);
		float g = pc[1] * (1.0f / 255.0f);
		float b = pc[2] * (1.0f / 255.0f);
		float a = p->alpha;

		vtx = AddVertex3D_C(vtx, p->origin[0], p->origin[1], p->origin[2], r, g, b, a);
	}

	oglwEnd();

	if (particleTextureUsed)
	{
		glDisable(GL_POINT_SPRITE_OES);
		glTexEnvi(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_FALSE);
		oglwSetTextureBlending(0, GL_REPLACE);
	}
	else
	{
		oglwEnableTexturing(0, GL_TRUE);
	}
}
#endif

static void R_Particles_Draw()
{
	int particleNb = r_newrefdef.num_particles;
	if (particleNb <= 0)
		return;

	oglwEnableBlending(true);
	oglwEnableDepthWrite(false);

	#if !defined(GLES2)
	qboolean stereo_split_tb = ((gl_state.stereo_mode == STEREO_SPLIT_VERTICAL) && gl_state.camera_separation);
	qboolean stereo_split_lr = ((gl_state.stereo_mode == STEREO_SPLIT_HORIZONTAL) && gl_state.camera_separation);
	if (gl_particle_point->value && !(stereo_split_tb || stereo_split_lr))
	{
		R_Particles_DrawWithPoints(particleNb, r_newrefdef.particles);
	}
	else
	#endif
	{
		R_Particles_DrawWithQuads(particleNb, r_newrefdef.particles);
	}

	oglwEnableBlending(false);
	oglwEnableDepthWrite(true);
}

//--------------------------------------------------------------------------------
// Screenshot.
//--------------------------------------------------------------------------------
typedef struct _TargaHeader
{
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
} TargaHeader;

static void R_ScreenShot()
{
	byte *buffer, temp;
	char picname[80];
	char checkname[MAX_OSPATH];
	int i, c;
	FILE *f;

	/* create the scrnshots directory if it doesn't exist */
	Com_sprintf(checkname, sizeof(checkname), "%s/screenshots", FS_WritableGamedir());
	Sys_Mkdir(checkname);

	/* find a file name to save it to */
	strcpy(picname, "quake00.tga");

	for (i = 0; i <= 99; i++)
	{
		picname[5] = i / 10 + '0';
		picname[6] = i % 10 + '0';
		Com_sprintf(checkname, sizeof(checkname), "%s/screenshots/%s",
			FS_WritableGamedir(), picname);
		f = fopen(checkname, "rb");

		if (!f)
		{
			break; /* file doesn't exist */
		}

		fclose(f);
	}

	if (i == 100)
	{
		VID_Printf(PRINT_ALL, "SCR_ScreenShot_f: Couldn't create a file\n");
		return;
	}

	static const int headerLength = 18 + 4;

	c = headerLength + vid.width * vid.height * 3;

	buffer = malloc(c);
	if (!buffer)
	{
		VID_Printf(PRINT_ALL, "SCR_ScreenShot_f: Couldn't malloc %d bytes\n", c);
		return;
	}

	memset(buffer, 0, headerLength);
	buffer[0] = 4; // image ID: "yq2\0"
	buffer[2] = 2; /* uncompressed type */
	buffer[12] = vid.width & 255;
	buffer[13] = vid.width >> 8;
	buffer[14] = vid.height & 255;
	buffer[15] = vid.height >> 8;
	buffer[16] = 24; /* pixel size */
	buffer[17] = 0; // image descriptor
	buffer[18] = 'y'; // following: the 4 image ID fields
	buffer[19] = 'q';
	buffer[20] = '2';
	buffer[21] = '\0';

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, vid.width, vid.height, GL_RGB,
		GL_UNSIGNED_BYTE, buffer + headerLength);

	/* swap rgb to bgr */
	for (i = headerLength; i < c; i += 3)
	{
		temp = buffer[i];
		buffer[i] = buffer[i + 2];
		buffer[i + 2] = temp;
	}

	f = fopen(checkname, "wb");
	if (f)
	{
		fwrite(buffer, 1, c, f);
		fclose(f);
		VID_Printf(PRINT_ALL, "Wrote %s\n", picname);
	}
	else
	{
		VID_Printf(PRINT_ALL, "SCR_ScreenShot_f: Couldn't write %s\n", picname);
	}

	free(buffer);
}
