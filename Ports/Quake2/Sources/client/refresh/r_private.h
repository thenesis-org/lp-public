#ifndef r_private_h
#define r_private_h

#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "SDL/SDLWrapper.h"
#include "OpenGLES/EGLWrapper.h"
#include "OpenGLES/OpenGLWrapper.h"

#include "client/ref.h"

#define BACKFACE_EPSILON 0.01f

#define PITCH 0 // up / down
#define YAW 1 // left / right
#define ROLL 2 // fall over

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
	bool used; // True if the image is used at least once during rendering.

	int texnum; /* gl texture binding */
	float sl, tl, sh, th; /* 0,0 - 1,1 unless part of the scrap */
	bool scrap;
	bool has_alpha;

	bool paletted;
} image_t;

#include "model.h"

extern image_t *r_notexture;
extern image_t *r_particletexture;

extern bool r_stencilAvailable;
extern bool r_msaaAvailable;

extern float gldepthmin, gldepthmax;

/* view origin */
extern vec3_t vup;
extern vec3_t vpn;
extern vec3_t vright;
extern vec3_t r_origin;

extern float r_world_matrix[16];

/* screen size info */
extern refdef_t r_newrefdef;
extern int r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

extern cplane_t frustum[4];

extern int c_visible_lightmaps;
extern int c_visible_textures;
extern int c_brush_polys, c_alias_polys;

extern cvar_t *r_window_width;
extern cvar_t *r_window_height;
extern cvar_t *r_window_x;
extern cvar_t *r_window_y;
extern cvar_t *r_fullscreen;
extern cvar_t *r_fullscreen_width;
extern cvar_t *r_fullscreen_height;
extern cvar_t *r_fullscreen_bitsPerPixel;
extern cvar_t *r_fullscreen_frequency;
extern cvar_t *r_msaa_samples;
extern cvar_t *r_gamma;
extern cvar_t *r_intensity;

extern cvar_t *r_norefresh;
extern cvar_t *r_discardframebuffer;
extern cvar_t *gl_clear;
extern cvar_t *gl_ztrick;
extern cvar_t *gl_zfix;
extern cvar_t *gl_swapinterval;

extern cvar_t *r_texture_retexturing;
extern cvar_t *r_texture_alphaformat;
extern cvar_t *r_texture_solidformat;
extern cvar_t *r_texture_rounddown;
extern cvar_t *r_texture_scaledown;
extern cvar_t *r_texture_filter;
extern cvar_t *r_texture_anisotropy;
extern cvar_t *r_texture_anisotropy_available;

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
extern cvar_t *gl_shadows;
extern cvar_t *gl_stencilshadow;

extern cvar_t *gl_particle_min_size;
extern cvar_t *gl_particle_max_size;
extern cvar_t *gl_particle_size;
extern cvar_t *gl_particle_att_a;
extern cvar_t *gl_particle_att_b;
extern cvar_t *gl_particle_att_c;
#if defined(EGLW_GLES1)
extern cvar_t *gl_particle_point;
extern cvar_t *gl_particle_sprite;
#endif

extern cvar_t *r_nobind;
extern cvar_t *r_multitexturing;
extern cvar_t *r_lightmap_mipmap;
extern cvar_t *r_lightmap_dynamic;
extern cvar_t *r_lightmap_backface_lighting;
extern cvar_t *r_lightmap_disabled;
extern cvar_t *r_lightmap_only;
extern cvar_t *r_lightmap_saturate;
extern cvar_t *gl_modulate;
extern cvar_t *r_lightmap_outline;
extern cvar_t *r_subdivision;

extern cvar_t *r_lightflash;
extern cvar_t *r_fullscreenflash;

//--------------------------------------------------------------------------------
// Textures.
//--------------------------------------------------------------------------------
#define MAX_GLTEXTURES 1024

#define TEXNUM_LIGHTMAPS 1024
#define TEXNUM_SCRAPS (TEXNUM_LIGHTMAPS + LIGHTMAP_MAX_NB)
#define TEXNUM_IMAGES (TEXNUM_SCRAPS + SCRAP_MAX_NB)

void LoadPCX(char *filename, byte **pic, byte **palette, int *width, int *height);
image_t* LoadWal(char *name);
qboolean LoadSTB(const char *origname, const char * type, byte **pic, int *width, int *height);
void GetWalInfo(char *name, int *width, int *height);
void GetPCXInfo(char *filename, int *width, int *height);
image_t* R_LoadPic(char *name, byte *pic, int width, int realwidth, int height, int realheight, imagetype_t type, int bits);
image_t* R_FindImage(char *name, imagetype_t type);

extern bool scrap_dirty;
void Scrap_Upload();

void R_InitImages();
void R_ShutdownImages();
void R_FreeUnusedImages();
void R_ImageList_f();
void R_ResampleTexture(unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight);
void R_Texture_upload(void *data, int x, int y, int width, int height, bool fullUploadFlag, bool noFilteringFlag, bool mipmapFlag);
bool R_Texture_upload8(byte *data, int width, int height, bool noFilteringFlag, bool mipmapFlag, bool skyFlag, int *uploadWidth, int *uploadHeight);
void R_TranslatePlayerSkin(int playernum);
void R_TextureAlphaMode(char *string);
void R_TextureSolidMode(char *string);
void R_TextureMode(char *string);

bool R_CullBox(vec3_t mins, vec3_t maxs);
void R_Entity_rotate(entity_t *e);

extern unsigned char d_8to24table[256][3];
extern int registration_sequence;

void V_AddBlend(float r, float g, float b, float a, float *v_blend);

void R_View_setupProjection(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar);

void R_AliasModel_draw(entity_t *e);
void R_BrushModel_draw(entity_t *e);

void R_Lighmap_lightPoint(entity_t *e, vec3_t p, vec3_t color, vec3_t lightSpot, cplane_t **lightPlane);
void R_Lightmap_buildPolygonFromSurface(model_t *model, msurface_t *fa);
void R_Lightmap_createSurface(msurface_t *surf);
void R_Lightmap_endBuilding(void);
void R_Lightmap_beginBuilding(model_t *m);

void R_Surface_subdivide(model_t *model, msurface_t *fa, float subdivisionSize);

extern model_t *r_worldmodel;

int Draw_GetPalette();
void Draw_InitLocal();

/*
 * GL config stuff
 */

typedef struct
{
	PFNGLDISCARDFRAMEBUFFEREXTPROC discardFramebuffer;
	bool anisotropic;
	bool tex_npot;
	float max_anisotropy;
} glconfig_t;

enum stereo_modes
{
	STEREO_MODE_NONE,
	STEREO_MODE_ANAGLYPH,
	STEREO_SPLIT_HORIZONTAL,
	STEREO_SPLIT_VERTICAL,
};

typedef struct
{
	float inverse_intensity;

	unsigned char *d_16to8table;

	int lightmap_textures;

	float camera_separation;
	enum stereo_modes stereo_mode;
    char eyeIndex;

	bool hwgamma;
	unsigned char originalRedGammaTable[256];
	unsigned char originalGreenGammaTable[256];
	unsigned char originalBlueGammaTable[256];
} glstate_t;

#define LIGHTMAP_WIDTH 128
#define LIGHTMAP_HEIGHT 128
#define LIGHTMAP_STATIC_MAX_NB 128
#define LIGHTMAP_DYNAMIC_MAX_NB 16
#define LIGHTMAP_MAX_NB (LIGHTMAP_STATIC_MAX_NB + LIGHTMAP_DYNAMIC_MAX_NB)
#define LIGHTMAP_SURFACE_MAX_NB (LIGHTMAP_STATIC_MAX_NB + 1)

typedef struct
{
	msurface_t *lightmapSurfaces[LIGHTMAP_SURFACE_MAX_NB];

	short staticLightmapNb; // Number of static textures.
	short staticLightmapNbInFrame; // Number of static textures used in the current frame.
    short staticLightmapSurfacesInFrame[LIGHTMAP_STATIC_MAX_NB];

	short dynamicLightmapCurrent; // Index of the current dynamic texture (round robin, even between frames for performance).
    short dynamicLightmapNbInFrame; // Number of dynamic pictures used in the current frame.

	short allocated[LIGHTMAP_WIDTH];
	// The lightmap texture data needs to be kept in main memory so texsubimage can update properly.
	byte lightmap_buffer[4 * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT];
} gllightmapstate_t;

extern glconfig_t gl_config;
extern glstate_t gl_state;

/*
 * (Un)grab Input
 */
void In_Grab(bool grab);

#endif
