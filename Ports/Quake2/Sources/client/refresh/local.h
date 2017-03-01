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
 * Local header for the refresher.
 *
 * =======================================================================
 */

#ifndef REF_LOCAL_H
#define REF_LOCAL_H

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "OpenGLES/OpenGLWrapper.h"

#include "client/ref.h"

#define TEXNUM_LIGHTMAPS 1024
#define TEXNUM_SCRAPS 1152
#define TEXNUM_IMAGES 1153
#define MAX_GLTEXTURES 1024
#define MAX_SCRAPS 1
#define BLOCK_WIDTH 128
#define BLOCK_HEIGHT 128
#define REF_VERSION QUAKE2_TEAM_NAME " OpenGL ES Renderer"
#define MAX_LBM_HEIGHT 480
#define BACKFACE_EPSILON 0.01f
#define DYNAMIC_LIGHT_WIDTH 128
#define DYNAMIC_LIGHT_HEIGHT 128
#define LIGHTMAP_BYTES 4
#define MAX_LIGHTMAPS 128
#define GL_LIGHTMAP_FORMAT GL_RGBA

/* up / down */
#define PITCH 0

/* left / right */
#define YAW 1

/* fall over */
#define ROLL 2

extern viddef_t vid;

/*
 * skins will be outline flood filled and mip mapped
 * pics and sprites with alpha will be outline flood filled
 * pic won't be mip mapped
 *
 * model skin
 * sprite frame
 * wall texture
 * pic
 */
typedef enum
{
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky
} imagetype_t;

enum stereo_modes
{
	STEREO_MODE_NONE,
	STEREO_MODE_ANAGLYPH,
	STEREO_SPLIT_HORIZONTAL,
	STEREO_SPLIT_VERTICAL,
};

typedef struct image_s
{
	char name[MAX_QPATH]; /* game path, including extension */
	imagetype_t type;
	int width, height; /* source image */
	int upload_width, upload_height; /* after power of two and picmip */
	int registration_sequence; /* 0 = free */

	// For sort-by-texture world and brush entity drawing.
	struct image_s *image_chain_node; // List node for the images currently used.
	struct msurface_s *texturechain; // List of surfaces using this image.
	qboolean used; // True if the image is used at least once during rendering.

	int texnum; /* gl texture binding */
	float sl, tl, sh, th; /* 0,0 - 1,1 unless part of the scrap */
	qboolean scrap;
	qboolean has_alpha;

	qboolean paletted;
} image_t;

#include "model.h"

void GL_BeginRendering(int *x, int *y, int *width, int *height);
void GL_EndRendering(void);

extern float gldepthmin, gldepthmax;

typedef struct
{
	float x, y, z;
	float s, t;
	float r, g, b;
} glvert_t;

extern image_t gltextures[MAX_GLTEXTURES];
extern int numgltextures;

extern image_t *r_notexture;
extern image_t *r_particletexture;
extern entity_t *currententity;
extern model_t *currentmodel;
extern int r_visframecount;
extern int r_framecount;
extern cplane_t frustum[4];
extern int c_brush_polys, c_alias_polys;
extern int gl_filter_min, gl_filter_max;

/* view origin */
extern vec3_t vup;
extern vec3_t vpn;
extern vec3_t vright;
extern vec3_t r_origin;

/* screen size info */
extern refdef_t r_newrefdef;
extern int r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

extern cvar_t *gl_mode;
extern cvar_t *gl_customwidth;
extern cvar_t *gl_customheight;
extern cvar_t *gl_msaa_samples;
extern cvar_t *vid_fullscreen;
extern cvar_t *vid_gamma;

extern cvar_t *gl_norefresh;
extern cvar_t *gl_discardframebuffer;
extern cvar_t *gl_clear;
extern cvar_t *gl_ztrick;
extern cvar_t *gl_zfix;
extern cvar_t *gl_swapinterval;

extern cvar_t *gl_stereo;
extern cvar_t *gl_stereo_separation;
extern cvar_t *gl_stereo_convergence;
extern cvar_t *gl_stereo_anaglyph_colors;

extern cvar_t *gl_farsee;
extern cvar_t *gl_drawentities;
extern cvar_t *gl_drawworld;
extern cvar_t *gl_speeds;

extern cvar_t *gl_novis;
extern cvar_t *gl_lockpvs;
extern cvar_t *gl_nocull;
extern cvar_t *gl_cull;

extern cvar_t *gl_lerpmodels;
extern cvar_t *gl_lefthand;
extern cvar_t *gl_lightlevel;

extern cvar_t *gl_particle_min_size;
extern cvar_t *gl_particle_max_size;
extern cvar_t *gl_particle_size;
extern cvar_t *gl_particle_att_a;
extern cvar_t *gl_particle_att_b;
extern cvar_t *gl_particle_att_c;
#if defined(GLES1)
extern cvar_t *gl_particle_point;
extern cvar_t *gl_particle_sprite;
#endif

extern cvar_t *gl_retexturing;
extern cvar_t *gl_texturealphaformat;
extern cvar_t *gl_texturesolidformat;
extern cvar_t *gl_round_down;
extern cvar_t *gl_picmip;
extern cvar_t *gl_texturefilter;
extern cvar_t *gl_anisotropic;
extern cvar_t *gl_anisotropic_avail;

extern cvar_t *gl_nobind;
extern cvar_t *gl_multitexturing;
extern cvar_t *gl_dynamic;
extern cvar_t *gl_fullbright;
extern cvar_t *gl_lightmap;
extern cvar_t *gl_saturatelighting;
extern cvar_t *gl_modulate;
extern cvar_t *intensity;

extern cvar_t *gl_shadows;
extern cvar_t *gl_stencilshadow;

extern cvar_t *gl_showtris;
extern cvar_t *gl_flashblend;
extern cvar_t *gl_fullscreenflash;

extern int gl_lightmap_format;
extern int gl_tex_solid_format;
extern int gl_tex_alpha_format;

extern int c_visible_lightmaps;
extern int c_visible_textures;

extern float r_world_matrix[16];

void R_TranslatePlayerSkin(int playernum);

void R_LightPoint(vec3_t p, vec3_t color);
void R_PushDlights(void);

extern model_t *r_worldmodel;
extern unsigned char d_8to24table[256][3];
extern int registration_sequence;

void V_AddBlend(float r, float g, float b, float a, float *v_blend);

void R_RenderView(refdef_t *fd);
void R_DrawAliasModel(entity_t *e);
void R_DrawBrushModel(entity_t *e);
void R_DrawSpriteModel(entity_t *e);
void R_DrawWorld();
void R_RenderDlights();
void R_DrawAlphaSurfaces(float alpha, int chain);
void Draw_InitLocal();
void R_SubdivideSurface(msurface_t *fa);
qboolean R_CullBox(vec3_t mins, vec3_t maxs);
void R_RotateForEntity(entity_t *e);
void R_MarkLeaves();

glpoly_t* WaterWarpPolyVerts(glpoly_t *p);
void R_DrawWaterPolys(msurface_t *fa, float intensity, float alpha);
void R_AddSkySurface(msurface_t *fa);
void R_ClearSkyBox();
void R_DrawSkyBox();
void R_MarkLights(dlight_t *light, int bit, mnode_t *node);

void COM_StripExtension(char *in, char *out);

void R_SwapBuffers(int);

int Draw_GetPalette();

void R_ResampleTexture(unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight);

void LoadPCX(char *filename, byte **pic, byte **palette, int *width, int *height);
image_t* LoadWal(char *name);
qboolean LoadSTB(const char *origname, const char * type, byte **pic, int *width, int *height);
void GetWalInfo(char *name, int *width, int *height);
void GetPCXInfo(char *filename, int *width, int *height);
image_t* R_LoadPic(char *name, byte *pic, int width, int realwidth, int height, int realheight, imagetype_t type, int bits);
image_t* R_FindImage(char *name, imagetype_t type);
void R_TextureMode(char *string);
void R_ImageList_f();

void R_InitImages();
void R_ShutdownImages();

void R_FreeUnusedImages();

void R_TextureAlphaMode(char *string);
void R_TextureSolidMode(char *string);
int Scrap_AllocBlock(int w, int h, int *x, int *y);

/*
 * GL config stuff
 */

typedef struct
{
	PFNGLDISCARDFRAMEBUFFEREXTPROC discardFramebuffer;
	qboolean anisotropic;
	qboolean tex_npot;
	float max_anisotropy;
} glconfig_t;

typedef struct
{
	float inverse_intensity;
	qboolean fullscreen;

	int prev_mode;

	unsigned char *d_16to8table;

	int lightmap_textures;

	float camera_separation;
	enum stereo_modes stereo_mode;

	qboolean hwgamma;

	unsigned char originalRedGammaTable[256];
	unsigned char originalGreenGammaTable[256];
	unsigned char originalBlueGammaTable[256];
} glstate_t;

typedef struct
{
	int internal_format;
	int current_lightmap_texture;

	msurface_t *lightmap_surfaces[MAX_LIGHTMAPS];

	int allocated[BLOCK_WIDTH];

	/* the lightmap texture data needs to be kept in
	   main memory so texsubimage can update properly */
	byte lightmap_buffer[4 * BLOCK_WIDTH * BLOCK_HEIGHT];
} gllightmapstate_t;

extern glconfig_t gl_config;
extern glstate_t gl_state;

/*
 * Initializes the SDL OpenGL context
 */
qboolean Renderer_Init();

/*
 * Shuts the SDL render backend down
 */
void Renderer_Shutdown();

/*
 * Changes the video mode
 */
int Renderer_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen);

/*
 * (Un)grab Input
 */
void In_Grab(qboolean grab);

#endif
