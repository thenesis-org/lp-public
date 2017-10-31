/*
   Copyright (C) 1996-1997 Id Software, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 */

#include "Client/client.h"
#include "Client/console.h"
#include "Client/view.h"
#include "Common/cmd.h"
#include "Common/cvar.h"
#include "Common/quakedef.h"
#include "Common/sys.h"
#include "Rendering/r_model.h"
#include "Rendering/r_private.h"
#include "Rendering/r_public.h"
#include "Sound/sound.h"

#include "OpenGLES/OpenGLWrapper.h"

#include <stdlib.h>
#include <string.h>

const GLubyte *gl_vendor;
const GLubyte *gl_renderer;
const GLubyte *gl_version;
const GLubyte *gl_extensions;

bool r_stencilAvailable = false;

static float r_depthMin = 0.0f;
static float r_depthMax = 0.0f;
static mplane_t r_frustum[4];

//
// view origin
//
vec3_t r_viewUp;
vec3_t r_viewZ;
vec3_t r_viewRight;
vec3_t r_viewOrigin;

float r_world_matrix[16];
float r_base_world_matrix[16];

//
// screen size info
//
refdef_t r_refdef;

static mleaf_t *r_viewLeaf, *r_oldviewleaf;

static entity_t r_worldentity;
static int r_visframecount; // bumped when going to a new PVS
static int r_framecount; // used for dlight push checking

static qboolean r_envMap; // true during r_envMap command capture

//--------------------------------------------------------------------------------
// Statistics.
//--------------------------------------------------------------------------------
int r_spriteCount, r_aliasModelCount, r_brushModelCount;

int r_surfaceTextureCount;

int r_surfaceTotalCount, r_surfaceBaseCount, r_surfaceLightmappedCount, r_surfaceMultitexturedCount;
int r_surfaceUnderwaterTotalCount, r_surfaceUnderWaterBaseCount, r_surfaceUnderWaterLightmappedCount, r_surfaceUnderWaterMultitexturedCount;
int r_surfaceWaterCount;
int r_surfaceSkyCount;
int r_surfaceGlobalCount;
int r_surfaceBaseTotalCount;
int r_surfaceLightmappedTotalCount;
int r_surfaceMultitexturedTotalCount;

int r_surfaceTotalPolyCount, r_surfaceBasePolyCount, r_surfaceLightmappedPolyCount, r_surfaceMultitexturedPolyCount;
int r_surfaceUnderWaterTotalPolyCount, r_surfaceUnderWaterBasePolyCount, r_surfaceUnderWaterLightmappedPolyCount, r_surfaceUnderWaterMultitexturedPolyCount;
int r_surfaceWaterPolyCount;
int r_surfaceSkyPolyCount;
int r_surfaceGlobalPolyCount;
int r_surfaceBaseTotalPolyCount;
int r_surfaceLightmappedTotalPolyCount;
int r_surfaceMultitexturedTotalPolyCount;

int r_aliasModelPolyCount;

//--------------------------------------------------------------------------------
// Cvars.
//--------------------------------------------------------------------------------
cvar_t r_fullscreen = { "r_fullscreen", "0", true };

cvar_t r_texture_nobind = { "r_texture_nobind", "0" };
cvar_t r_texture_maxsize = { "r_texture_maxsize", "1024", true };
cvar_t r_texture_downsampling = { "r_texture_downsampling", "0", true };

cvar_t r_disabled = { "r_disabled", "0" };

cvar_t r_clear = { "r_clear", "1", true };
cvar_t r_ztrick = { "r_ztrick", "0", true };
cvar_t r_zfix = { "r_zfix", "0", true };

cvar_t r_cull = { "r_cull", "1" };

cvar_t r_novis = { "r_novis", "0" };
cvar_t r_draw_world = { "r_draw_world", "1" };
cvar_t r_draw_entities = { "r_draw_entities", "1" };
cvar_t r_draw_viewmodel = { "r_draw_viewmodel", "1" };

// Lightmaps and lighting.
cvar_t r_lightmap_disabled = { "r_lightmap_disabled", "0" };
cvar_t r_lightmap_only = { "r_lightmap_only", "0" };
cvar_t r_lightmap_dynamic = { "r_lightmap_dynamic", "1", true };
cvar_t r_lightmap_backface_lighting = { "r_lightmap_backface_lighting", "1", true };
cvar_t r_lightmap_upload_full = { "r_lightmap_upload_full", "0", true };
cvar_t r_lightmap_upload_delayed = { "r_lightmap_upload_delayed", "1", true };
cvar_t r_lightmap_mipmap = { "r_lightmap_mipmap", "0", true }; // On most embedded platforms, the in-game mipmap generation causes important slow downs.
cvar_t r_lightflash = { "r_lightflash", "0", true }; // Better have either dynamic lightmap, either light flashes.

// Other world and brush model rendering.
cvar_t r_tjunctions_keep = { "r_tjunctions_keep", "0", true };
cvar_t r_tjunctions_report = { "r_tjunctions_report", "0", true };
cvar_t r_sky_subdivision = { "r_sky_subdivision", "256", true };
cvar_t r_water_subdivision = { "r_water_subdivision", "32", true };
cvar_t r_texture_sort = { "r_texture_sort", "1", true };
cvar_t r_multitexturing = { "r_multitexturing", "0", true };

// Alias model.
cvar_t r_meshmodel_shadow = { "r_meshmodel_shadow", "1", true };
cvar_t r_meshmodel_shadow_stencil = { "r_meshmodel_shadow_stencil", "1", true };
cvar_t r_meshmodel_smooth_shading = { "r_meshmodel_smooth_shading", "1", true };
cvar_t r_meshmodel_affine_filtering = { "r_meshmodel_affine_filtering", "0", true };
cvar_t r_meshmodel_double_eyes = { "r_meshmodel_double_eyes", "1", true };

// Player model.
cvar_t r_player_downsampling = { "r_player_downsampling", "0", true };
cvar_t r_player_nocolors = { "r_player_nocolors", "0", true };

// Full screen.
cvar_t r_fullscreenfx = { "r_fullscreenfx", "1", true };

//--------------------------------------------------------------------------------
// Common.
//--------------------------------------------------------------------------------
// Returns true if the box is completely outside the frustom
static bool R_CullBox(vec3_t mins, vec3_t maxs)
{
	for (int i = 0; i < 4; i++)
		if (BoxOnPlaneSide(mins, maxs, &r_frustum[i]) == 2)
			return true;
	return false;
}

static void R_RotateForEntity(entity_t *e)
{
	oglwTranslate(e->origin[0], e->origin[1], e->origin[2]);
	oglwRotate(e->angles[1], 0, 0, 1);
	oglwRotate(-e->angles[0], 0, 1, 0);
	oglwRotate(e->angles[2], 1, 0, 0);
}

//--------------------------------------------------------------------------------
// Textures.
//--------------------------------------------------------------------------------
typedef struct
{
	int texnum;
	short width, height;
	qboolean mipmap;
	char identifier[64];
} TextureGL;

#define MAX_GLTEXTURES 1024
static TextureGL r_textures[MAX_GLTEXTURES];
static int r_textureNb = 0;

static int r_texels = 0;

static int r_filterMin = GL_LINEAR_MIPMAP_LINEAR;
static int r_filterMax = GL_LINEAR;

texture_t *r_notexture_mip;
int r_playerTextures; // up to 16 color translated skins

static TextureGL* R_Texture_find(char *identifier)
{
	int i, n = r_textureNb;
	for (i = 0; i < n; i++)
	{
        TextureGL *t = &r_textures[i];
		if (!strcmp(identifier, t->identifier))
			return t;
	}
	return NULL;
}


static void R_Texture_translate8(byte *in, int inwidth, int inheight, unsigned *out, const unsigned *palette)
{
    int s = inwidth * inheight;
	for (int i = 0; i < s; i++)
	{
        int paletteIndex = in[i];
        out[i] = palette[paletteIndex];
	}
}

static void R_Texture_resample8(byte *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight, const unsigned *palette)
{
	unsigned fracstep = inwidth * 0x10000 / outwidth;
	for (int i = 0; i < outheight; i++, out += outwidth)
	{
		byte *inrow = in + inwidth * (i * inheight / outheight);
		unsigned frac = fracstep >> 1;
		for (int j = 0; j < outwidth; j ++)
		{
            int paletteIndex = inrow[frac >> 16];
			out[j] = palette[paletteIndex];
			frac += fracstep;
		}
	}
}

static void R_Texture_resample32(unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight)
{
	unsigned fracstep = inwidth * 0x10000 / outwidth;
	for (int i = 0; i < outheight; i++, out += outwidth)
	{
		unsigned *inrow = in + inwidth * (i * inheight / outheight);
		unsigned frac = fracstep >> 1;
		for (int j = 0; j < outwidth; j ++)
		{
			out[j] = inrow[frac >> 16];
			frac += fracstep;
		}
	}
}

bool R_Texture_load(void *data, int width, int height, bool mipmap, bool alpha, int downsampling, const unsigned *palette)
{
	unsigned *scaled = NULL;
    
	int scaled_width, scaled_height;
    for (scaled_width = 1; scaled_width < width; scaled_width <<= 1)
        ;
    for (scaled_height = 1; scaled_height < height; scaled_height <<= 1)
        ;

    if (downsampling > 0)
    {
        scaled_width >>= downsampling;
        scaled_height >>= downsampling;
    }
    
    if (scaled_width < 1)
        scaled_width = 1;
    if (scaled_height < 1)
        scaled_height = 1;

    int maxSize = r_texture_maxsize.value;
    if (maxSize > 0)
    {
        if (scaled_width > maxSize)
            scaled_width = maxSize;
        if (scaled_height > maxSize)
            scaled_height = maxSize;
    }
    
    if (data != NULL)
    {
        bool resizeFlag = scaled_width != width || scaled_height != height;
        if (resizeFlag || palette)
        {
            scaled = malloc(scaled_width * scaled_height * sizeof(unsigned));
            if (scaled == NULL)
                return true;
            if (resizeFlag)
            {
                if (palette)
                    R_Texture_resample8(data, width, height, scaled, scaled_width, scaled_height, palette);
                else
                    R_Texture_resample32(data, width, height, scaled, scaled_width, scaled_height);
            }
            else
                R_Texture_translate8(data, width, height, scaled, palette);
            data = scaled;
        }
    }

	r_texels += scaled_width * scaled_height;

	#if defined(EGLW_GLES1)
    if (mipmap)
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	#endif
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	#if defined(EGLW_GLES1)
    if (mipmap)
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	#else
	if (mipmap)
        glGenerateMipmap(GL_TEXTURE_2D);
	#endif

	if (mipmap)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filterMin);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filterMax);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filterMax);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filterMax);
	}

	free(scaled);
    
    return false;
}

unsigned int R_Texture_create(char *identifier, int width, int height, void *data, bool mipmap, bool alpha, bool downsampling, bool paletted)
{
	TextureGL *t;

	if (identifier[0])
	{
        // See if the texture is already present
        t = R_Texture_find(identifier);
        if (t)
        {
            if (width != t->width || height != t->height)
                Sys_Error("R_Texture_create: cache mismatch");
            return t->texnum;
        }
	}
	
    int texnum = r_textureNb;
    if (texnum >= MAX_GLTEXTURES)
    {
        Sys_Error("R_Texture_create: too many textures");
        return -1;
    }
    oglwSetCurrentTextureUnitForced(0);
    oglwBindTextureForced(0, texnum);
    if (R_Texture_load(data, width, height, mipmap, alpha, (int)r_texture_downsampling.value, paletted ? d_8to24table : NULL))
        return -1;
    t = &r_textures[texnum];
    strcpy(t->identifier, identifier);
    t->texnum = texnum;
    t->width = width;
    t->height = height;
    t->mipmap = mipmap;
    r_textureNb++;

	return texnum;
}

void R_Texture_clearAll()
{
    // TODO
}

typedef struct
{
	char *name;
	int minimize, maximize;
} glmode_t;

static const glmode_t modes[] =
{
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
};

void R_Texture_filtering_f()
{
	int i;

	if (Cmd_Argc() == 1)
	{
		for (i = 0; i < 6; i++)
			if (r_filterMin == modes[i].minimize)
			{
				Con_Printf("%s\n", modes[i].name);
				return;
			}
		Con_Printf("current filter is unknown\n");
		return;
	}

	for (i = 0; i < 6; i++)
	{
		if (!Q_strcasecmp(modes[i].name, Cmd_Argv(1)))
			break;
	}
	if (i == 6)
	{
		Con_Printf("bad filter name\n");
		return;
	}

	r_filterMin = modes[i].minimize;
	r_filterMax = modes[i].maximize;

	// change all the existing mipmap texture objects
    oglwSetCurrentTextureUnitForced(0);
	TextureGL *glt;
	for (i = 0, glt = r_textures; i < r_textureNb; i++, glt++)
	{
		oglwBindTextureForced(0, glt->texnum);
		if (glt->mipmap)
		{
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filterMin);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filterMax);
		}
        else
        {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filterMax);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filterMax);
        }
	}
}

void R_initTextures()
{
	// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName(sizeof(texture_t) + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2, "notexture");
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16 * 16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8 * 8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4 * 4;

	for (int m = 0; m < 4; m++)
	{
		byte *dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (int y = 0; y < (16 >> m); y++)
			for (int x = 0; x < (16 >> m); x++)
			{
				if ((y < (8 >> m)) ^ (x < (8 >> m)))
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}
}

//--------------------------------------------------------------------------------
// 2D
//--------------------------------------------------------------------------------
// Setup as if the screen was 320*200
void R_setup2D()
{
	oglwSetViewport(r_viewportX, r_viewportY, r_viewportWidth, r_viewportHeight);

	oglwMatrixMode(GL_PROJECTION);
	oglwLoadIdentity();
	oglwOrtho(0, vid.width, vid.height, 0, -99999, 99999);

	oglwMatrixMode(GL_MODELVIEW);
	oglwLoadIdentity();

	glDisable(GL_CULL_FACE);

    oglwEnableBlending(true);
	oglwSetBlendingFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    oglwEnableAlphaTest(true);
    oglwSetAlphaFunc(GL_GREATER, 0.666f);
	oglwEnableDepthTest(false);

    oglwEnableSmoothShading(false);
    oglwEnableTexturing(0, true);
    oglwEnableTexturing(1, false);
	oglwSetTextureBlending(0, GL_REPLACE);
}

//--------------------------------------------------------------------------------
// Light styles.
//--------------------------------------------------------------------------------
static int r_lightStyleValue[256]; // 8.8 fraction of base light value

static void R_LightStyles_animate()
{
	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	int i = (int)(cl.time * 10);
	for (int j = 0; j < MAX_LIGHTSTYLES; j++)
	{
        lightstyle_t *lightStyle = &cl_lightstyle[j];
        int length = lightStyle->length;
        int k;
		if (!length)
            k = 256;
        else
        {
            k = i % length;
            k = lightStyle->map[k] - 'a';
            k = k * 22;
        }
		r_lightStyleValue[j] = k;
	}
}

//--------------------------------------------------------------------------------
// Dynamic lighting.
//--------------------------------------------------------------------------------
static void R_DynamicLights_mark(model_t *worldModel, dlight_t *light, int bit, int frameCount, mnode_t *node)
{
	if (node->contents < 0)
		return;

    {
        mplane_t *splitplane = node->plane;
        float dist = DotProduct(light->origin, splitplane->normal) - splitplane->dist;
        if (dist > light->radius)
        {
            R_DynamicLights_mark(worldModel, light, bit, frameCount, node->children[0]);
            return;
        }
        if (dist < -light->radius)
        {
            R_DynamicLights_mark(worldModel, light, bit, frameCount, node->children[1]);
            return;
        }
    }

	// mark the polygons
	msurface_t *surface = worldModel->surfaces + node->firstsurface;
	for (int i = 0; i < node->numsurfaces; i++, surface++)
	{
        if (!r_lightmap_backface_lighting.value)
        {
            // This avoids to light backfacing surfaces but this still does not handle occlusion.
            // As a result this does not always look better because one backfacing surface is not lit whereas a nearby one is, and no shadow are cast.
            // With backfacing surfaces lit, it fakes global illumination, which looks nicer.
            // But it may be faster with this enabled because less surfaces are lit.
            mplane_t *surfacePlane = surface->plane;
            float dist = DotProduct(light->origin, surfacePlane->normal) - surfacePlane->dist;
            int sidebit;
            if (dist >= 0)
                sidebit = 0;
            else
                sidebit = SURF_PLANEBACK;
            if ((surface->flags & SURF_PLANEBACK) != sidebit)
                continue;
        }
        
        if (surface->dlightframe != frameCount)
        {
            surface->dlightbits = 0;
            surface->dlightframe = frameCount;
        }       
        surface->dlightbits |= bit;
	}

	R_DynamicLights_mark(worldModel, light, bit, frameCount, node->children[0]);
	R_DynamicLights_mark(worldModel, light, bit, frameCount, node->children[1]);
}

static void R_DynamicLights_push(model_t *worldModel, mnode_t *nodes)
{
	if (!r_lightmap_dynamic.value)
		return;

    int frameCount = r_framecount; // because the count hasn't advanced yet for this frame
	dlight_t *l = cl_dlights;
	for (int i = 0; i < MAX_DLIGHTS; i++, l++)
	{
		if (l->die < cl.time || !l->radius)
			continue;
		R_DynamicLights_mark(worldModel, l, 1 << i, frameCount, nodes);
	}
}

//--------------------------------------------------------------------------------
// Normal surface.
//--------------------------------------------------------------------------------
static void R_Surface_draw(msurface_t *surface, float alpha)
{
    r_surfaceBaseCount++;

	for (glpoly_t *poly = surface->polys; poly; poly = poly->next)
	{
        int n = poly->numverts;
        OglwVertex *vtx = oglwAllocateTriangleFan(n);
        if (vtx == NULL)
            continue; // Skip in case of degenerated poly or out of memory.

        r_surfaceBasePolyCount += n - 2;

        float *v = poly->verts[0];
        for (int i = 0; i < n; i++, v += VERTEXSIZE)
        {
            vtx = AddVertex3D_CT1(vtx, v[0], v[1], v[2], 1.0f, 1.0f, 1.0f, alpha, v[3], v[4]);
        }
    }
}

static void R_Surface_drawLightmapped(msurface_t *surface, float alpha)
{
    r_surfaceLightmappedCount++;

	for (glpoly_t *poly = surface->polys; poly; poly = poly->next)
	{
        int n = poly->numverts;
        OglwVertex *vtx = oglwAllocateTriangleFan(n);
        if (vtx == NULL)
            continue; // Skip in case of degenerated poly or out of memory.

        r_surfaceLightmappedPolyCount += n - 2;

        float *v = poly->verts[0];
        for (int i = 0; i < n; i++, v += VERTEXSIZE)
        {
            vtx = AddVertex3D_CT1(vtx, v[0], v[1], v[2], 1.0f, 1.0f, 1.0f, alpha, v[5], v[6]);
        }
    }
}

static void R_Surface_drawMultitextured(msurface_t *surface, float alpha)
{
    r_surfaceMultitexturedCount++;

	for (glpoly_t *poly = surface->polys; poly; poly = poly->next)
	{
        int n = poly->numverts;
        OglwVertex *vtx = oglwAllocateTriangleFan(n);
        if (vtx == NULL)
            continue; // Skip in case of degenerated poly or out of memory.

        r_surfaceMultitexturedPolyCount += n - 2;

        float *v = poly->verts[0];
        for (int i = 0; i < n; i++, v += VERTEXSIZE)
        {
            vtx = AddVertex3D_CT2(vtx, v[0], v[1], v[2], 1.0f, 1.0f, 1.0f, alpha, v[3], v[4], v[5], v[6]);
        }
    }
}

//--------------------------------------------------------------------------------
// Surface subdivision.
//--------------------------------------------------------------------------------
static void R_Surface_boundPolygon(int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	float *v = verts;
	for (int i = 0; i < numverts; i++)
		for (int j = 0; j < 3; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

static void R_Surface_subdividePolygon(msurface_t *warpface, int numverts, float *verts, float subdivisionSize)
{
	if (numverts > 60)
    {
		Sys_Error("numverts = %i", numverts);
        return;
    }

	vec3_t mins, maxs;
	R_Surface_boundPolygon(numverts, verts, mins, maxs);

	for (int i = 0; i < 3; i++)
	{
		float m = (mins[i] + maxs[i]) * 0.5f;
		m = subdivisionSize * floorf(m / subdivisionSize + 0.5f);
		if (maxs[i] - m < 8 || m - mins[i] < 8)
			continue;

		// cut it
		float *v = verts + i;
		float dist[64];
		for (int j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;

		// wrap cases
		dist[numverts] = dist[0];
		v -= i;
		VectorCopy(verts, v);

		vec3_t front[64], back[64];
		int f = 0, b = 0;
		v = verts;
		for (int j = 0; j < numverts; j++, v += 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy(v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy(v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j + 1] > 0))
			{
				// clip point
				float frac = dist[j] / (dist[j] - dist[j + 1]);
				for (int k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}

		R_Surface_subdividePolygon(warpface, f, front[0], subdivisionSize);
		R_Surface_subdividePolygon(warpface, b, back[0], subdivisionSize);
		return;
	}

	glpoly_t *poly = Hunk_Alloc(sizeof(glpoly_t) + (numverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (int i = 0; i < numverts; i++, verts += 3)
	{
		VectorCopy(verts, poly->verts[i]);
		float s = DotProduct(verts, warpface->texinfo->vecs[0]);
		float t = DotProduct(verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}

/*
   Breaks a polygon up along axial 64 unit
   boundaries so that turbulent and sky warps
   can be done reasonably.
 */
void R_Surface_subdivide(model_t *model, msurface_t *surface, float subdivisionSize)
{
	vec3_t verts[64];

	//
	// convert edges back to a normal polygon
	//
	int numverts = 0;
	for (int i = 0; i < surface->numedges; i++)
	{
		int lindex = model->surfedges[surface->firstedge + i];

		float *vec;
		if (lindex > 0)
			vec = model->vertexes[model->edges[lindex].v[0]].position;
		else
			vec = model->vertexes[model->edges[-lindex].v[1]].position;
		VectorCopy(vec, verts[numverts]);
		numverts++;
	}

	R_Surface_subdividePolygon(surface, numverts, verts[0], subdivisionSize);
}

//--------------------------------------------------------------------------------
// Under water surface.
//--------------------------------------------------------------------------------
// Warp the vertex coordinates
static void R_UnderWaterSurface_draw(msurface_t *surface, float alpha)
{
    r_surfaceUnderWaterBaseCount++;

    float time = realtime;
	for (glpoly_t *poly = surface->polys; poly; poly = poly->next)
	{
        int n = poly->numverts;
        OglwVertex *vtx = oglwAllocateTriangleFan(n);
        if (vtx == NULL)
            continue; // Skip in case of degenerated poly or out of memory.

        r_surfaceUnderWaterBasePolyCount += n - 2;

        float *v = poly->verts[0];
        for (int i = 0; i < n; i++, v += VERTEXSIZE)
        {
            float sinz = sinf(v[2] * 0.05f + time);
            float px = v[0] + 8 * sinf(v[1] * 0.05f + time) * sinz;
            float py = v[1] + 8 * sinf(v[0] * 0.05f + time) * sinz;
            float pz = v[2];
            vtx = AddVertex3D_CT1(vtx, px, py, pz, 1.0f, 1.0f, 1.0f, alpha, v[3], v[4]);
        }
    }
}

static void R_UnderWaterSurface_drawLightmapped(msurface_t *surface, float alpha)
{
    r_surfaceUnderWaterLightmappedCount++;

    float time = realtime;
	for (glpoly_t *poly = surface->polys; poly; poly = poly->next)
	{
        int n = poly->numverts;
        OglwVertex *vtx = oglwAllocateTriangleFan(n);
        if (vtx == NULL)
            continue; // Skip in case of degenerated poly or out of memory.

        r_surfaceUnderWaterLightmappedPolyCount += n - 2;

        float *v = poly->verts[0];
        for (int i = 0; i < n; i++, v += VERTEXSIZE)
        {
            float sinz = sinf(v[2] * 0.05f + time);
            float px = v[0] + 8 * sinf(v[1] * 0.05f + time) * sinz;
            float py = v[1] + 8 * sinf(v[0] * 0.05f + time) * sinz;
            float pz = v[2];
            vtx = AddVertex3D_CT1(vtx, px, py, pz, 1.0f, 1.0f, 1.0f, alpha, v[5], v[6]);
        }
    }
}

static void R_UnderWaterSurface_drawMultitextured(msurface_t *surface, float alpha)
{
    r_surfaceUnderWaterMultitexturedCount++;

    float time = realtime;
	for (glpoly_t *poly = surface->polys; poly; poly = poly->next)
	{
        int n = poly->numverts;
        OglwVertex *vtx = oglwAllocateTriangleFan(n);
        if (vtx == NULL)
            continue; // Skip in case of degenerated poly or out of memory.

        r_surfaceUnderWaterMultitexturedPolyCount += n - 2;

        float *v = poly->verts[0];
        for (int i = 0; i < n; i++, v += VERTEXSIZE)
        {
            float sinz = sinf(v[2] * 0.05f + time);
            float px = v[0] + 8 * sinf(v[1] * 0.05f + time) * sinz;
            float py = v[1] + 8 * sinf(v[0] * 0.05f + time) * sinz;
            float pz = v[2];
            vtx = AddVertex3D_CT2(vtx, px, py, pz, 1.0f, 1.0f, 1.0f, alpha, v[3], v[4], v[5], v[6]);
        }
    }
}

//--------------------------------------------------------------------------------
// Water surface.
//--------------------------------------------------------------------------------
// Does a water warp on the pre-fragmented glpoly_t chain
static void R_WaterSurface_draw(msurface_t *surface, float alpha)
{
	static const float turbsin[] =
	{
		0, 0.19633, 0.392541, 0.588517, 0.784137, 0.979285, 1.17384, 1.3677,
		1.56072, 1.75281, 1.94384, 2.1337, 2.32228, 2.50945, 2.69512, 2.87916,
		3.06147, 3.24193, 3.42044, 3.59689, 3.77117, 3.94319, 4.11282, 4.27998,
		4.44456, 4.60647, 4.76559, 4.92185, 5.07515, 5.22538, 5.37247, 5.51632,
		5.65685, 5.79398, 5.92761, 6.05767, 6.18408, 6.30677, 6.42566, 6.54068,
		6.65176, 6.75883, 6.86183, 6.9607, 7.05537, 7.14579, 7.23191, 7.31368,
		7.39104, 7.46394, 7.53235, 7.59623, 7.65552, 7.71021, 7.76025, 7.80562,
		7.84628, 7.88222, 7.91341, 7.93984, 7.96148, 7.97832, 7.99036, 7.99759,
		8, 7.99759, 7.99036, 7.97832, 7.96148, 7.93984, 7.91341, 7.88222,
		7.84628, 7.80562, 7.76025, 7.71021, 7.65552, 7.59623, 7.53235, 7.46394,
		7.39104, 7.31368, 7.23191, 7.14579, 7.05537, 6.9607, 6.86183, 6.75883,
		6.65176, 6.54068, 6.42566, 6.30677, 6.18408, 6.05767, 5.92761, 5.79398,
		5.65685, 5.51632, 5.37247, 5.22538, 5.07515, 4.92185, 4.76559, 4.60647,
		4.44456, 4.27998, 4.11282, 3.94319, 3.77117, 3.59689, 3.42044, 3.24193,
		3.06147, 2.87916, 2.69512, 2.50945, 2.32228, 2.1337, 1.94384, 1.75281,
		1.56072, 1.3677, 1.17384, 0.979285, 0.784137, 0.588517, 0.392541, 0.19633,
		9.79717e-16, -0.19633, -0.392541, -0.588517, -0.784137, -0.979285, -1.17384, -1.3677,
		-1.56072, -1.75281, -1.94384, -2.1337, -2.32228, -2.50945, -2.69512, -2.87916,
		-3.06147, -3.24193, -3.42044, -3.59689, -3.77117, -3.94319, -4.11282, -4.27998,
		-4.44456, -4.60647, -4.76559, -4.92185, -5.07515, -5.22538, -5.37247, -5.51632,
		-5.65685, -5.79398, -5.92761, -6.05767, -6.18408, -6.30677, -6.42566, -6.54068,
		-6.65176, -6.75883, -6.86183, -6.9607, -7.05537, -7.14579, -7.23191, -7.31368,
		-7.39104, -7.46394, -7.53235, -7.59623, -7.65552, -7.71021, -7.76025, -7.80562,
		-7.84628, -7.88222, -7.91341, -7.93984, -7.96148, -7.97832, -7.99036, -7.99759,
		-8, -7.99759, -7.99036, -7.97832, -7.96148, -7.93984, -7.91341, -7.88222,
		-7.84628, -7.80562, -7.76025, -7.71021, -7.65552, -7.59623, -7.53235, -7.46394,
		-7.39104, -7.31368, -7.23191, -7.14579, -7.05537, -6.9607, -6.86183, -6.75883,
		-6.65176, -6.54068, -6.42566, -6.30677, -6.18408, -6.05767, -5.92761, -5.79398,
		-5.65685, -5.51632, -5.37247, -5.22538, -5.07515, -4.92185, -4.76559, -4.60647,
		-4.44456, -4.27998, -4.11282, -3.94319, -3.77117, -3.59689, -3.42044, -3.24193,
		-3.06147, -2.87916, -2.69512, -2.50945, -2.32228, -2.1337, -1.94384, -1.75281,
		-1.56072, -1.3677, -1.17384, -0.979285, -0.784137, -0.588517, -0.392541, -0.19633,
	};

    r_surfaceWaterCount++;

	float TURBSCALE = (256.0f / (2 * Q_PI));
	float time = realtime;

	for (glpoly_t *poly = surface->polys; poly; poly = poly->next)
	{
		int n = poly->numverts;
		OglwVertex *vtx = oglwAllocateTriangleFan(n);
		if (vtx == NULL)
            continue; // Skip in case of degenerated poly or out of memory.

        r_surfaceWaterPolyCount += n - 2;
        float *v;
        int i;
        for (i = 0, v = poly->verts[0]; i < n; i++, v += VERTEXSIZE)
        {
            float os = v[3];
            float ot = v[4];
            float s = os + turbsin[(int)((ot * 0.125f + time) * TURBSCALE) & 255];
            float t = ot + turbsin[(int)((os * 0.125f + time) * TURBSCALE) & 255];
            s *= (1.0f / 64);
            t *= (1.0f / 64);
            vtx = AddVertex3D_CT1(vtx, v[0], v[1], v[2], 1.0f, 1.0f, 1.0f, alpha, s, t);
        }
	}
}

//--------------------------------------------------------------------------------
// Sky.
//--------------------------------------------------------------------------------
static texture_t *r_skyTexture = NULL;
static int r_skyOpaqueTexture = -1;
static int r_skyAlphaTexture = -1;

static void R_Sky_drawSurface(msurface_t *surface, float speedscale)
{
    r_surfaceSkyCount++;
	for (glpoly_t *poly = surface->polys; poly; poly = poly->next)
	{
		int n = poly->numverts;
		OglwVertex *vtx = oglwAllocateTriangleFan(n);
		if (vtx == NULL)
            continue; // Skip in case of degenerated poly or out of memory.
            
        r_surfaceSkyPolyCount += n - 2;

        int i;
        float *v;
        for (i = 0, v = poly->verts[0]; i < n; i++, v += VERTEXSIZE)
        {
            vec3_t dir;
            VectorSubtract(v, r_viewOrigin, dir);
            dir[2] *= 3; // flatten the sphere

            float length = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
            length = sqrtf(length);
            length = 6 * 63 / length;

            dir[0] *= length;
            dir[1] *= length;

            float s = (speedscale + dir[0]) * (1.0f / 128);
            float t = (speedscale + dir[1]) * (1.0f / 128);

            vtx = AddVertex3D_CT1(vtx, v[0], v[1], v[2], 1.0f, 1.0f, 1.0f, 1.0f, s, t);
        }
	}
}

static void R_Sky_drawChain(msurface_t *surface)
{
    float speedscale;
    
	oglwBindTexture(0, r_skyOpaqueTexture);
	oglwBegin(GL_TRIANGLES);
	speedscale = realtime * 8;
	speedscale -= (int)speedscale & ~127;
	for (msurface_t *s = surface; s; s = s->textureChain)
    {
		R_Sky_drawSurface(s, speedscale);
    }
	oglwEnd();

	oglwEnableBlending(true);
	oglwBindTexture(0, r_skyAlphaTexture);
	oglwBegin(GL_TRIANGLES);
	speedscale = realtime * 16;
	speedscale -= (int)speedscale & ~127;
	for (msurface_t *s = surface; s; )
    {
		R_Sky_drawSurface(s, speedscale);
        msurface_t *sn = s->textureChain;
        s->textureChain = NULL;
        s->textureChained = false;
        s = sn;
    }
	oglwEnd();
	oglwEnableBlending(false);
}

// A sky texture is 256*128, with the right side being a masked overlay
void R_Sky_init(texture_t *mt)
{
    int width = mt->width, widthO2 = width >> 1, height = mt->height;
    if (widthO2 <= 0 || height <= 0)
        return;
    
	byte *src = (byte *)mt + mt->offsets[0];
	unsigned *trans = malloc(widthO2 * height * sizeof(unsigned));
    if (!trans)
        return;

	// make an average value for the back to avoid a fringe on the top level
	int r = 0, g = 0, b = 0;
	for (int i = 0; i < height; i++)
		for (int j = 0; j < widthO2; j++)
		{
			int p = src[i * width + widthO2 + j];
			unsigned color = d_8to24table[p];
			trans[i * widthO2 + j] = color;
			r += (color >>  0) & 0xff;
			g += (color >>  8) & 0xff;
			b += (color >> 16) & 0xff;
		}

    int pixelNb = widthO2 * height;
    r /= pixelNb;
    g /= pixelNb;
    b /= pixelNb;
	unsigned transpix = (b <<  16) | (g <<  8) | (r <<  0);

    oglwSetCurrentTextureUnitForced(0);

	if (r_skyOpaqueTexture <= 0)
        r_skyOpaqueTexture = R_Texture_create("/sky-solid", widthO2, height, trans, true, false, true, false);
    else
    {
        oglwBindTextureForced(0, r_skyOpaqueTexture);
        R_Texture_load(trans, widthO2, height, true, false, (int)r_texture_downsampling.value, NULL);
    }

	for (int i = 0; i < height; i++)
		for (int j = 0; j < widthO2; j++)
		{
			int p = src[i * width + j];
			if (p == 0)
				trans[(i * widthO2) + j] = transpix;
			else
				trans[(i * widthO2) + j] = d_8to24table[p];
		}

	if (r_skyAlphaTexture <= 0)
        r_skyAlphaTexture = R_Texture_create("/sky-alpha", widthO2, height, trans, true, true, true, false);
    else
    {
        oglwBindTextureForced(0, r_skyAlphaTexture);
        R_Texture_load(trans, widthO2, height, true, true, (int)r_texture_downsampling.value, NULL);
    }

	free(trans);
}

//--------------------------------------------------------------------------------
// Lightmap.
//--------------------------------------------------------------------------------
#define LightmapBlockWidth 128
#define LightmapBlockHeight 128

#define LightmapMaxNb 64

typedef struct LightmapRect_
{
	unsigned char l, t, w, h;
} LightmapRect;

struct Lightmap_
{
    struct Lightmap_ *chain;
    GLuint textureId;
    
    struct Lightmap_ *modifiedChain;
    LightmapRect changedRect;
    int *allocated;
    bool modified;

    struct Lightmap_ *usedChain;
    msurface_t *surfacesList;
       
    byte *data; // The lightmap texture data needs to be kept in main memory so texsubimage can update properly
};

static int r_lightmap_textures;

static int r_lightmapNb = 0;
static Lightmap *r_lightmaps = NULL;
static Lightmap *r_lightmap_modifiedList = NULL;
static Lightmap *r_lightmap_usedList = NULL;

static int R_Lighmap_lightPointR(model_t *worldModel, mnode_t *node, vec3_t start, vec3_t end, vec3_t lightspot)
{
	if (node->contents < 0)
		return -1; // didn't hit anything

	// calculate mid point
	mplane_t *plane = node->plane;
	float front = DotProduct(start, plane->normal) - plane->dist;
	float back = DotProduct(end, plane->normal) - plane->dist;
	int side = front < 0;

	if ((back < 0) == side)
		return R_Lighmap_lightPointR(worldModel, node->children[side], start, end, lightspot);

	float frac = front / (front - back);
	vec3_t mid;
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

	// go down front side
	int r = R_Lighmap_lightPointR(worldModel, node->children[side], start, mid, lightspot);
	if (r >= 0)
		return r; // hit something

	if ((back < 0) == side)
		return -1; // didn't hit anything

	// check for impact on this node
	VectorCopy(mid, lightspot);

	msurface_t *surface = worldModel->surfaces + node->firstsurface;
    int surfaceNb = node->numsurfaces;
	for (int surfaceIndex = 0; surfaceIndex < surfaceNb; surfaceIndex++, surface++)
	{
		if (surface->flags & SURF_DRAWTILED)
			continue; // no lightmap

		mtexinfo_t *tex = surface->texinfo;

		int s = DotProduct(mid, tex->vecs[0]) + tex->vecs[0][3];
		int t = DotProduct(mid, tex->vecs[1]) + tex->vecs[1][3];;
		if (s < surface->texturemins[0] || t < surface->texturemins[1])
			continue;

		int ds = s - surface->texturemins[0];
		int dt = t - surface->texturemins[1];
		if (ds > surface->extents[0] || dt > surface->extents[1])
			continue;

		if (!surface->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		byte *lightmap = surface->samples;
		r = 0;
		if (lightmap)
		{
			lightmap += dt * ((surface->extents[0] >> 4) + 1) + ds;
			for (int maps = 0; maps < MAXLIGHTMAPS; maps++)
			{
                if (surface->styles[maps] == 255)
                    break;
				unsigned scale = r_lightStyleValue[surface->styles[maps]];
				r += *lightmap * scale;
				lightmap += ((surface->extents[0] >> 4) + 1) * ((surface->extents[1] >> 4) + 1);
			}
			r >>= 8;
		}

		return r;
	}

	// go down back side
	return R_Lighmap_lightPointR(worldModel, node->children[!side], mid, end, lightspot);
}

static int R_Lighmap_lightPoint(model_t *worldModel, vec3_t p, vec3_t lightspot)
{
	if (!worldModel->lightdata)
		return 255;

	vec3_t end;
	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	int r = R_Lighmap_lightPointR(worldModel, worldModel->nodes, p, end, lightspot);
	if (r < 0)
		r = 0;

	return r;
}

static void R_Lightmap_free(Lightmap *lightmap)
{
    if (!lightmap)
        return;
    glDeleteTextures(1, &lightmap->textureId);
    free(lightmap->allocated);
    free(lightmap->data);
    free(lightmap);
    r_lightmapNb--;
}

static void R_Lightmap_freeAll()
{
    Lightmap *lightmap = r_lightmaps;
    while (lightmap)
    {
        Lightmap *lightmapNext = lightmap->chain;
        R_Lightmap_free(lightmap);
        lightmap = lightmapNext;
    }
    r_lightmaps = NULL;
    r_lightmapNb = 0;
    r_lightmap_modifiedList = NULL;
    r_lightmap_usedList = NULL;
}


static Lightmap* R_Lightmap_allocate()
{
    Lightmap *lightmap = NULL;
    
    if (r_lightmapNb >= LightmapMaxNb)
        goto on_error; // Too many lightmaps.

    lightmap = (Lightmap*)malloc(sizeof(Lightmap));
    if (lightmap == NULL)
        goto on_error;
    memset(lightmap, 0, sizeof(Lightmap));
    
    {
        int allocatedSize = sizeof(int) * LightmapBlockWidth;
        int *allocated = (int *)malloc(allocatedSize);
        lightmap->allocated = allocated;
        if (allocated == NULL)
            goto on_error;
        memset(allocated, 0, allocatedSize);
    }

    {
        int dataSize = 4 * LightmapBlockWidth * LightmapBlockHeight;
        byte *data = (byte *)malloc(dataSize);
        lightmap->data = data;
        if (data == NULL)
            goto on_error;
        memset(data, 0, dataSize);
    }
    
    lightmap->textureId = r_lightmap_textures + r_lightmapNb;

    lightmap->chain = r_lightmaps;
    r_lightmaps = lightmap;
    r_lightmapNb++;
    
    return lightmap;
on_error:
    R_Lightmap_free(lightmap);
    return NULL;
}

static Lightmap* R_Lightmap_allocBlock(int w, int h, int *x_, int *y_)
{
    if (w <= 0 || w > LightmapBlockWidth || h <= 0 || h > LightmapBlockHeight)
        return NULL;
    
    int x = 0, y = 0;
    Lightmap *lightmap = r_lightmaps;
	while (true)
	{
        if (!lightmap)
        {
            lightmap = R_Lightmap_allocate();
            if (!lightmap)
                break;
        }
        
        int *lightmapAllocated = lightmap->allocated;

		int best = LightmapBlockHeight;
		for (int i = 0; i < LightmapBlockWidth - w; i++)
		{
			int best2 = 0;
            int j;
			for (j = 0; j < w; j++)
			{
                int allocated = lightmapAllocated[i + j];
				if (allocated >= best)
					break;
				if (allocated > best2)
					best2 = allocated;
			}
			if (j == w) // this is a valid spot
			{
				x = i;
				y = best = best2;
			}
		}

		if (best + h > LightmapBlockHeight)
            lightmap = lightmap->chain; // No room in this lightmap.
        else
        {
            for (int i = 0; i < w; i++)
                lightmapAllocated[x + i] = best + h;
            
            *x_ = x;
            *y_ = y;
            return lightmap;
        }
	}

    // There is not enough space for the lightmap.
	Con_Printf("Cannot allocate a lightmap block\n");
	return NULL;
}

static void R_Lightmap_addDynamicLights(msurface_t *surface, unsigned *blocklights)
{
	int smax = (surface->extents[0] >> 4) + 1;
	int tmax = (surface->extents[1] >> 4) + 1;
	mtexinfo_t *tex = surface->texinfo;

	for (int lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (!(surface->dlightbits & (1 << lnum)))
			continue; // not lit by this light

        mplane_t *plane = surface->plane;
        dlight_t *dlight = &cl_dlights[lnum];
		float dist = DotProduct(dlight->origin, plane->normal) - plane->dist;
		float rad = dlight->radius - fabsf(dist);
		float minlight = dlight->minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

        vec3_t impact;
        VectorMA(dlight->origin, -dist, plane->normal, impact);

		float localX = DotProduct(impact, tex->vecs[0]) + tex->vecs[0][3] - surface->texturemins[0];
		float localY = DotProduct(impact, tex->vecs[1]) + tex->vecs[1][3] - surface->texturemins[1];

		for (int t = 0; t < tmax; t++)
		{
			float td = localY - (t << 4);
			for (int s = 0; s < smax; s++)
			{
				float sd = localX - (s << 4);
                float distance = sqrtf(td * td + sd * sd); // TODO: Could use sqrt approximation here (or rsqrt(x)*x).
				if (distance < minlight)
				{
                    float attenuation = rad - distance;
					blocklights[t * smax + s] += (unsigned)(attenuation * 256);
                }
			}
		}
	}
}

static void R_Lightmap_build(msurface_t *surface, byte *dest, int stride)
{
	surface->cached_dlight = (surface->dlightframe == r_framecount);

	int smax = (surface->extents[0] >> 4) + 1;
	int tmax = (surface->extents[1] >> 4) + 1;
	int size = smax * tmax;

	// set to full bright if no light data
    unsigned l_blocklights[18 * 18];
    unsigned *blocklights = l_blocklights;
	if (r_lightmap_disabled.value || !cl.worldmodel->lightdata)
	{
		for (int i = 0; i < size; i++)
			blocklights[i] = 255 * 256;
	}
    else
    {
        // clear to no light
        for (int i = 0; i < size; i++)
            blocklights[i] = 0;

        // add all the lightmaps
        byte *lightmap = surface->samples;
        if (lightmap)
        {
            for (int maps = 0; maps < MAXLIGHTMAPS && surface->styles[maps] != 255; maps++)
            {
                unsigned scale = r_lightStyleValue[surface->styles[maps]];
                surface->cached_light[maps] = scale; // 8.8 fraction
                for (int i = 0; i < size; i++)
                    blocklights[i] += lightmap[i] * scale;
                lightmap += size; // skip to next lightmap
            }
        }
        // add all the dynamic lights
        if (surface->dlightframe == r_framecount)
            R_Lightmap_addDynamicLights(surface, blocklights);
    }
    
	// bound, invert, and shift
	stride -= (smax << 2);
	unsigned *bl = blocklights;
	for (int i = 0; i < tmax; i++, dest += stride)
	{
		for (int j = 0; j < smax; j++)
		{
			int t = *bl++;
			t >>= 7;
			if (t > 255)
				t = 255;
            dest[0] = t;
            dest[1] = t;
            dest[2] = t;
			dest[3] = 0xff;
			dest += 4;
		}
	}
}

static void R_Lightmap_update(msurface_t *surface)
{
    // Check for dynamic lights.
    bool updateNeeded = false;
    if (surface->dlightframe == r_framecount)
        updateNeeded = true; // dynamic this frame
    if (surface->cached_dlight)
        updateNeeded = true; // dynamic previously

    // Check for lightmap styles changes.
	for (int maps = 0; maps < MAXLIGHTMAPS; maps++)
    {
        int style = surface->styles[maps];
        if (style == 255)
            break;
		if (r_lightStyleValue[style] != surface->cached_light[maps])
        {
            updateNeeded = true;
            break;
        }
    }
    
    Lightmap *lightmap = surface->lightmap;
	if (lightmap && updateNeeded) 
	{
        if (r_lightmap_upload_delayed.value && !lightmap->modified)
        {
            lightmap->modifiedChain = r_lightmap_modifiedList;
            r_lightmap_modifiedList = lightmap;
        }
        lightmap->modified = true;
        LightmapRect *changedRect = &lightmap->changedRect;
        int lightS = surface->light_s, lightT = surface->light_t;
        int rectT = changedRect->t, rectL = changedRect->l, rectW = changedRect->w, rectH = changedRect->h;
        if (lightT < rectT)
        {
            if (rectH)
                rectH += rectT - lightT;
            rectT = lightT;
        }
        if (lightS < rectL)
        {
            if (rectW)
                rectW += rectL - lightS;
            rectL = lightS;
        }
        int smax = (surface->extents[0] >> 4) + 1;
        int tmax = (surface->extents[1] >> 4) + 1;
        if ((rectW + rectL) < (lightS + smax))
            rectW = (lightS - rectL) + smax;
        if ((rectH + rectT) < (lightT + tmax))
            rectH = (lightT - rectT) + tmax;
        changedRect->t = rectT; changedRect->l = rectL; changedRect->w = rectW; changedRect->h = rectH;
        byte *base = lightmap->data + lightT * LightmapBlockWidth * 4 + lightS * 4;
        R_Lightmap_build(surface, base, LightmapBlockWidth * 4);
	}
}

static void R_Lightmap_createSurface(msurface_t *surface)
{
	if (surface->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	int smax = (surface->extents[0] >> 4) + 1;
	int tmax = (surface->extents[1] >> 4) + 1;
    Lightmap *lightmap = R_Lightmap_allocBlock(smax, tmax, &surface->light_s, &surface->light_t);
	surface->lightmap = lightmap;
    if (lightmap)
    {
        byte *base = lightmap->data + (surface->light_t * LightmapBlockWidth + surface->light_s) * 4;
        R_Lightmap_build(surface, base, LightmapBlockWidth * 4);
    }
}

static void R_Lightmap_buildSurfaceDisplayList(model_t *currentModel, msurface_t *surface)
{
	medge_t *pedges = currentModel->edges;
	int lnumverts = surface->numedges;

	glpoly_t *poly = Hunk_Alloc(sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = surface->polys;
	poly->flags = surface->flags;
	poly->numverts = lnumverts;
	surface->polys = poly;

	for (int i = 0; i < lnumverts; i++)
	{
		int lindex = currentModel->surfedges[surface->firstedge + i];
        float s, t;

        float *vec;
		if (lindex > 0)
		{
			medge_t *r_pedge = &pedges[lindex];
			vec = currentModel->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			medge_t *r_pedge = &pedges[-lindex];
			vec = currentModel->vertexes[r_pedge->v[1]].position;
		}
		s = DotProduct(vec, surface->texinfo->vecs[0]) + surface->texinfo->vecs[0][3];
		s /= surface->texinfo->texture->width;

		t = DotProduct(vec, surface->texinfo->vecs[1]) + surface->texinfo->vecs[1][3];
		t /= surface->texinfo->texture->height;

		VectorCopy(vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		//
		// lightmap texture coordinates
		//
		s = DotProduct(vec, surface->texinfo->vecs[0]) + surface->texinfo->vecs[0][3];
		s -= surface->texturemins[0];
		s += surface->light_s * 16;
		s += 8;
		s /= LightmapBlockWidth * 16; //s->texinfo->texture->width;

		t = DotProduct(vec, surface->texinfo->vecs[1]) + surface->texinfo->vecs[1][3];
		t -= surface->texturemins[1];
		t += surface->light_t * 16;
		t += 8;
		t /= LightmapBlockHeight * 16; //s->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	// remove co-linear points - Ed
	if (!r_tjunctions_keep.value && !(surface->flags & SURF_UNDERWATER))
	{
		for (int i = 0; i < lnumverts; ++i)
		{
			vec3_t v1, v2;
			float *prev, *this, *next;

			prev = poly->verts[(i + lnumverts - 1) % lnumverts];
			this = poly->verts[i];
			next = poly->verts[(i + 1) % lnumverts];

			VectorSubtract(this, prev, v1);
			VectorNormalize(v1);
			VectorSubtract(next, prev, v2);
			VectorNormalize(v2);

			// skip co-linear points
#define COLINEAR_EPSILON 0.001
			if ((fabsf(v1[0] - v2[0]) <= COLINEAR_EPSILON) &&
			    (fabsf(v1[1] - v2[1]) <= COLINEAR_EPSILON) &&
			    (fabsf(v1[2] - v2[2]) <= COLINEAR_EPSILON))
			{
				for (int j = i + 1; j < lnumverts; ++j)
				{
					for (int k = 0; k < VERTEXSIZE; ++k)
						poly->verts[j - 1][k] = poly->verts[j][k];
				}
				--lnumverts;
				// retry next vertex next time, which is now current vertex
				--i;
			}
		}
	}
	poly->numverts = lnumverts;
}

static void R_Lightmap_upload(int texturingUnit, Lightmap *lightmap, bool forceFullUpload)
{
    if (lightmap->modified || forceFullUpload)
    {
        lightmap->modified = false;
        oglwSetCurrentTextureUnitForced(texturingUnit);
        oglwBindTextureForced(texturingUnit, lightmap->textureId);
        LightmapRect *changedRect = &lightmap->changedRect;
        bool mipmap = r_lightmap_mipmap.value != 0;
   		#if defined(EGLW_GLES1)
        if (mipmap)
            glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
        #endif
        if (r_lightmap_upload_full.value || forceFullUpload)
        {
            byte *lightmapData = lightmap->data;
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LightmapBlockWidth, LightmapBlockHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, lightmapData);
        }
        else
        {
            byte *lightmapData = lightmap->data + changedRect->t * LightmapBlockWidth * 4;
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, changedRect->t, LightmapBlockWidth, changedRect->h, GL_RGBA, GL_UNSIGNED_BYTE, lightmapData);
        }
        changedRect->l = LightmapBlockWidth;
        changedRect->t = LightmapBlockHeight;
        changedRect->h = 0;
        changedRect->w = 0;
		#if defined(EGLW_GLES1)
        if (mipmap)
            glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
		#elif defined(EGLW_GLES2)
        if (mipmap)
            glGenerateMipmap(GL_TEXTURE_2D);
        #endif
        if (mipmap)
        {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filterMin);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filterMax);
        }
        else
        {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filterMax);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filterMax);
        }
        oglwSetCurrentTextureUnit(0);
    }
}

// Builds the lightmap texture with all the surfaces from all brush models
static void R_Lightmap_buildAllSurfaces()
{
    R_Lightmap_freeAll();

	r_framecount = 1; // no dlightcache

	if (!r_lightmap_textures)
	{
		r_lightmap_textures = r_textureNb;
		r_textureNb += LightmapMaxNb;
	}

	for (int j = 1; j < MAX_MODELS; j++)
	{
		model_t *m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;
		for (int i = 0; i < m->numsurfaces; i++)
		{
			R_Lightmap_createSurface(m->surfaces + i);
			if (m->surfaces[i].flags & (SURF_DRAWTURB | SURF_DRAWSKY))
				continue;
			R_Lightmap_buildSurfaceDisplayList(m, m->surfaces + i);
		}
	}

	// upload all lightmaps that were filled
	for (Lightmap *lightmap = r_lightmaps; lightmap; lightmap = lightmap->chain)
        R_Lightmap_upload(0, lightmap, true);
}

static void R_Lightmap_uploadDelayed()
{
    if (r_lightmap_upload_delayed.value)
    {
        for (Lightmap *lightmap = r_lightmap_modifiedList; lightmap; )
        {
            R_Lightmap_upload(0, lightmap, false);
            Lightmap *lightmapNext = lightmap->modifiedChain;
            lightmap->modifiedChain = NULL;
            lightmap = lightmapNext;
        }
        r_lightmap_modifiedList = NULL;
    }
}

static void R_Lightmap_bind(int texturingUnit, Lightmap *lightmap)
{
    if (lightmap)
    {
        oglwBindTexture(texturingUnit, lightmap->textureId);
        if (!r_lightmap_upload_delayed.value)
            R_Lightmap_upload(texturingUnit, lightmap, false);
    }
    else
        oglwBindTexture(texturingUnit, 0);
}

static void R_Lightmap_chainSurface(msurface_t *surface)
{
	if (r_lightmap_disabled.value || !r_texture_sort.value)
		return;
    if (!surface->lightmapChained)
    {
        Lightmap *lightmap = surface->lightmap;
        if (lightmap)
        {
            if (!lightmap->surfacesList)
            {
                lightmap->usedChain = r_lightmap_usedList;
                r_lightmap_usedList = lightmap;
            }
            surface->lightmapChain = lightmap->surfacesList;
            surface->lightmapChained = true;
            lightmap->surfacesList = surface;
        }
    }
}

static void R_Lightmap_drawChains()
{
	if (r_lightmap_disabled.value || !r_texture_sort.value)
		return;

	if (!r_lightmap_only.value)
    {
        oglwEnableBlending(true);
        oglwSetBlendingFunction(GL_ZERO, GL_SRC_COLOR);
    }
    oglwEnableDepthWrite(false);
    
	for (Lightmap *lightmap = r_lightmap_usedList; lightmap; )
	{
        msurface_t *surface = lightmap->surfacesList;
        if (surface != NULL)
        {
            lightmap->surfacesList = NULL;
            R_Lightmap_bind(0, lightmap);
            oglwBegin(GL_TRIANGLES);
            do
            {
                if (surface->flags & SURF_UNDERWATER)
                    R_UnderWaterSurface_drawLightmapped(surface, 1.0f);
                else
                    R_Surface_drawLightmapped(surface, 1.0f);
                msurface_t *surfaceNext = surface->lightmapChain;
                surface->lightmapChain = NULL;
                surface->lightmapChained = false;
                surface = surfaceNext;
            }
            while (surface);
            oglwEnd();
        }
        Lightmap *lightmapNext = lightmap->usedChain;
        lightmap->usedChain = NULL;
        lightmap = lightmapNext;
	}
    r_lightmap_usedList = NULL;

	if (!r_lightmap_only.value)
    {
        oglwEnableBlending(false);
        oglwSetBlendingFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    oglwEnableDepthWrite(true);
}

//--------------------------------------------------------------------------------
// Light flashes.
//--------------------------------------------------------------------------------
static void R_LightFlash_addFullscreen(float r, float g, float b, float a, float *outputColor)
{
	float ta = outputColor[3];
    ta = ta + a * (1 - ta);
	outputColor[3] = ta;

	a = a / ta;
	outputColor[0] = outputColor[1] * (1 - a) + r * a;
	outputColor[1] = outputColor[1] * (1 - a) + g * a;
	outputColor[2] = outputColor[2] * (1 - a) + b * a;
}

static void R_LightFlash_render(dlight_t *light)
{
	float rad = light->radius * 0.35f;

	vec3_t v;
	VectorSubtract(light->origin, r_viewOrigin, v);
	if (Length(v) < rad) // view is inside the dlight
	{
		R_LightFlash_addFullscreen(1, 0.5f, 0, light->radius * 0.0003f, v_blend);
		return;
	}

	OglwVertex *vtx = oglwAllocateTriangleFan(1 + 16 + 1);
    if (vtx == NULL)
        return;

	for (int i = 0; i < 3; i++)
		v[i] = light->origin[i] - r_viewZ[i] * rad;
	vtx = AddVertex3D_C(vtx, v[0], v[1], v[2], 0.2f, 0.1f, 0.0f, 1.0f);

	for (int i = 16; i >= 0; i--)
	{
		float a = i / 16.0f * Q_PI * 2;
		for (int j = 0; j < 3; j++)
			v[j] = light->origin[j] + r_viewRight[j] * cosf(a) * rad + r_viewUp[j] * sinf(a) * rad;
		vtx = AddVertex3D_C(vtx, v[0], v[1], v[2], 0.0f, 0.0f, 0.0f, 1.0f);
	}
}

static void R_LightFlash_renderAll()
{
	if (!r_lightflash.value)
		return;

	oglwEnableDepthWrite(false);
	oglwEnableTexturing(0, false);
	oglwEnableSmoothShading(true);
	oglwEnableBlending(true);
	oglwSetBlendingFunction(GL_ONE, GL_ONE);

	oglwBegin(GL_TRIANGLES);
	dlight_t *l = cl_dlights;
	for (int i = 0; i < MAX_DLIGHTS; i++, l++)
	{
		if (l->die < cl.time || !l->radius)
			continue;
		R_LightFlash_render(l);
	}
	oglwEnd();

	oglwSetBlendingFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	oglwEnableBlending(false);
	oglwEnableSmoothShading(false);
	oglwEnableTexturing(0, true);
	oglwEnableDepthWrite(true);
}

//--------------------------------------------------------------------------------
// Brush model.
//--------------------------------------------------------------------------------
static texture_t *r_usedTextureList = NULL;

// Returns the proper texture for a given time and base texture
static texture_t* R_BrushModel_getAnimatedTexture(entity_t *entity, texture_t *base)
{
	if (entity->frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	int reletive = (int)(cl.time * 10) % base->anim_total;
	int count = 0;
	while (base->anim_min > reletive || base->anim_max <= reletive)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error("R_BrushModel_getAnimatedTexture: broken cycle");
		if (++count > 100)
			Sys_Error("R_BrushModel_getAnimatedTexture: infinite cycle");
	}

	return base;
}

static void R_BrushModel_chainSurface(texture_t *texture, msurface_t *surface)
{
    if (surface->textureChained)
        return;

    if (!texture->surfaceList)
    {
        texture->usedTextureChain = r_usedTextureList;
        r_usedTextureList = texture;
    }

    surface->textureChain = texture->surfaceList;
    surface->textureChained = true;
    texture->surfaceList = surface;
}

static void R_BrushModel_drawSurface(entity_t *entity, msurface_t *surface, float alpha)
{
    if (r_texture_sort.value)
    {
        // Sorting by texture, just store it out.
        texture_t *texture = R_BrushModel_getAnimatedTexture(entity, surface->texinfo->texture);
        R_BrushModel_chainSurface(texture, surface);
    }
    else if (surface->flags & SURF_DRAWTURB)
	{
        // Subdivided water surface warp
		texture_t *texture = R_BrushModel_getAnimatedTexture(entity, surface->texinfo->texture);
        oglwBindTexture(0, texture->textureId);
        oglwBegin(GL_TRIANGLES);
        R_WaterSurface_draw(surface, alpha);
        oglwEnd();
	}
    else if (surface->flags & SURF_DRAWSKY)
	{
        // Subdivided sky warp
        R_Sky_drawChain(surface);
	}
    else if (surface->flags & SURF_UNDERWATER)
    {
        // Underwater warped with lightmap
        R_Lightmap_update(surface);
        if (r_multitexturing.value)
        {
            oglwEnableTexturing(1, true);
			texture_t *texture = R_BrushModel_getAnimatedTexture(entity, surface->texinfo->texture);
            oglwBindTexture(0, texture->textureId);
            R_Lightmap_bind(1, surface->lightmap);
            oglwBegin(GL_TRIANGLES);
            R_UnderWaterSurface_drawMultitextured(surface, alpha);
            oglwEnd();
            oglwEnableTexturing(1, false);
        }
        else
        {
			texture_t *texture = R_BrushModel_getAnimatedTexture(entity, surface->texinfo->texture);
            oglwBindTexture(0, texture->textureId);
            oglwBegin(GL_TRIANGLES);
            R_UnderWaterSurface_draw(surface, alpha);
            oglwEnd();

            R_Lightmap_bind(0, surface->lightmap);
            oglwEnableBlending(true);
            oglwSetBlendingFunction(GL_ZERO, GL_SRC_COLOR);
            oglwBegin(GL_TRIANGLES);
            R_UnderWaterSurface_drawLightmapped(surface, alpha);
            oglwEnd();
            oglwSetBlendingFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            oglwEnableBlending(false);
        }
    }
    else
    {
        // Normal lightmaped poly
        R_Lightmap_update(surface);
		if (r_multitexturing.value)
		{
            oglwEnableTexturing(1, true);
			texture_t *texture = R_BrushModel_getAnimatedTexture(entity, surface->texinfo->texture);
            oglwBindTexture(0, texture->textureId); // Binds world to texture env 0
            R_Lightmap_bind(1, surface->lightmap);
			oglwBegin(GL_TRIANGLES);
			R_Surface_drawMultitextured(surface, alpha);
			oglwEnd();
            oglwEnableTexturing(1, false);
		}
		else
		{
			texture_t *texture = R_BrushModel_getAnimatedTexture(entity, surface->texinfo->texture);
            oglwBindTexture(0, texture->textureId);
			oglwBegin(GL_TRIANGLES);
			R_Surface_draw(surface, alpha);
			oglwEnd();

			oglwEnableBlending(true);
            oglwSetBlendingFunction(GL_ZERO, GL_SRC_COLOR);
            R_Lightmap_bind(0, surface->lightmap);
			oglwBegin(GL_TRIANGLES);
			R_Surface_drawLightmapped(surface, alpha);
			oglwEnd();
            oglwSetBlendingFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			oglwEnableBlending(false);
		}
    }
}

static void R_BrushModel_drawChains(model_t *model)
{
    texture_t *texture = r_usedTextureList;
    while (texture != NULL)
    {
        r_surfaceTextureCount++;

        msurface_t *surface = texture->surfaceList;
        texture->surfaceList = NULL;
        if (texture == r_skyTexture)
            R_Sky_drawChain(surface);
        else
        {
            oglwBindTexture(0, texture->textureId);
            oglwBegin(GL_TRIANGLES);
            while (surface)
            {
                if (surface->flags & SURF_DRAWSKY) // warp texture, no lightmap
                {
                    // This should not occur because sky surfaces are already processed just above with R_Sky_drawChain().
                    float speedscale = realtime * 8;
                    speedscale -= (int)speedscale & ~127;
                    R_Sky_drawSurface(surface, speedscale);
                }
                else if (surface->flags & SURF_DRAWTURB)
                {
                    R_WaterSurface_draw(surface, 1.0f);
                }
                else
                {
                    if (surface->flags & SURF_UNDERWATER)
                        R_UnderWaterSurface_draw(surface, 1.0f);
                    else
                        R_Surface_draw(surface, 1.0f);
                    R_Lightmap_chainSurface(surface);
                    R_Lightmap_update(surface);
                }
                msurface_t *surfaceNext = surface->textureChain;
                surface->textureChain = NULL;
                surface->textureChained = false;
                surface = surfaceNext;
            }
            oglwEnd();
        }

        texture_t *textureNext = texture->usedTextureChain;
        texture->usedTextureChain = NULL;
        texture = textureNext;
    }
    r_usedTextureList = NULL;
}

static void R_BrushModel_draw(model_t *worldModel, entity_t *brushModelEntity)
{
	model_t *brushModel = brushModelEntity->model;
	vec3_t mins, maxs;
	qboolean rotated;
	if (brushModelEntity->angles[0] || brushModelEntity->angles[1] || brushModelEntity->angles[2])
	{
		rotated = true;
		for (int i = 0; i < 3; i++)
		{
			mins[i] = brushModelEntity->origin[i] - brushModel->radius;
			maxs[i] = brushModelEntity->origin[i] + brushModel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd(brushModelEntity->origin, brushModel->mins, mins);
		VectorAdd(brushModelEntity->origin, brushModel->maxs, maxs);
	}

	if (R_CullBox(mins, maxs))
		return;

    r_brushModelCount++;

    vec3_t modelOrigin;
	VectorSubtract(r_refdef.vieworg, brushModelEntity->origin, modelOrigin);
	if (rotated)
	{
		vec3_t temp;
		vec3_t forward, right, up;
		VectorCopy(modelOrigin, temp);
		AngleVectors(brushModelEntity->angles, forward, right, up);
		modelOrigin[0] = DotProduct(temp, forward);
		modelOrigin[1] = -DotProduct(temp, right);
		modelOrigin[2] = DotProduct(temp, up);
	}

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (brushModel->firstmodelsurface != 0)
        R_DynamicLights_push(worldModel, brushModel->nodes + brushModel->hulls[0].firstclipnode);

	oglwPushMatrix();

	brushModelEntity->angles[0] = -brushModelEntity->angles[0]; // stupid quake bug
	R_RotateForEntity(brushModelEntity);
	brushModelEntity->angles[0] = -brushModelEntity->angles[0]; // stupid quake bug

	msurface_t *surface = &brushModel->surfaces[brushModel->firstmodelsurface];
	for (int i = 0; i < brushModel->nummodelsurfaces; i++, surface++)
	{
		// find which side of the node we are on
        mplane_t *pplane = surface->plane;
        float dot = DotProduct(modelOrigin, pplane->normal) - pplane->dist;
		if (((surface->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || (!(surface->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			R_BrushModel_drawSurface(brushModelEntity, surface, 1.0f);
		}
	}

	R_BrushModel_drawChains(brushModel);
	R_Lightmap_drawChains();

	oglwPopMatrix();
}

//--------------------------------------------------------------------------------
// Entity fragment.
//--------------------------------------------------------------------------------
static mnode_t *r_efrag_topNode;
static efrag_t **r_efrag_lastlink;
static vec3_t r_efrag_mins, r_efrag_maxs;
static entity_t *r_efrag_entity;

static void R_Efrag_splitEntityOnNode(mnode_t *node)
{
	if (node->contents == CONTENTS_SOLID)
		return;

	// add an efrag if the node is a leaf
	if (node->contents < 0)
	{
		if (!r_efrag_topNode)
			r_efrag_topNode = node;

		mleaf_t *leaf = (mleaf_t *)node;

		// grab an efrag off the free list
		efrag_t *ef = cl.free_efrags;
		if (!ef)
		{
			Con_Printf("Too many efrags!\n");
			return; // no free fragments...
		}
		cl.free_efrags = cl.free_efrags->entnext;

		ef->entity = r_efrag_entity;

		// add the entity link
		*r_efrag_lastlink = ef;
		r_efrag_lastlink = &ef->entnext;
		ef->entnext = NULL;

		// set the leaf links
		ef->leaf = leaf;
		ef->leafnext = leaf->efrags;
		leaf->efrags = ef;

		return;
	}

	// NODE_MIXED

	mplane_t *splitplane = node->plane;
	int sides = BOX_ON_PLANE_SIDE(r_efrag_mins, r_efrag_maxs, splitplane);
	if (sides == 3)
	{
		// split on this plane
		// if this is the first splitter of this bmodel, remember it
		if (!r_efrag_topNode)
			r_efrag_topNode = node;
	}

	// recurse down the contacted sides
	if (sides & 1)
		R_Efrag_splitEntityOnNode(node->children[0]);

	if (sides & 2)
		R_Efrag_splitEntityOnNode(node->children[1]);
}

void R_Efrag_add(entity_t *ent)
{
	if (!ent->model)
		return;

	r_efrag_entity = ent;
	r_efrag_lastlink = &ent->efrag;
	r_efrag_topNode = NULL;

	model_t *entmodel = ent->model;
	for (int i = 0; i < 3; i++)
	{
		r_efrag_mins[i] = ent->origin[i] + entmodel->mins[i];
		r_efrag_maxs[i] = ent->origin[i] + entmodel->maxs[i];
	}

	R_Efrag_splitEntityOnNode(cl.worldmodel->nodes);

	ent->topnode = r_efrag_topNode;
}

// Call when removing an object from the world or moving it to another position
void R_Efrag_remove(entity_t *ent)
{
	efrag_t *ef = ent->efrag;
	while (ef)
	{
		efrag_t **prev = &ef->leaf->efrags;
		while (1)
		{
			efrag_t *walk = *prev;
			if (!walk)
				break;
			if (walk == ef) // remove this fragment
			{
				*prev = ef->leafnext;
				break;
			}
			else
				prev = &walk->leafnext;
		}

		efrag_t *old = ef;
		ef = ef->entnext;

		// put it on the free list
		old->entnext = cl.free_efrags;
		cl.free_efrags = old;
	}

	ent->efrag = NULL;
}

static void R_Efrag_store(efrag_t **ppefrag)
{
	efrag_t *pefrag;
	while ((pefrag = *ppefrag) != NULL)
	{
		entity_t *pent = pefrag->entity;
		model_t *clmodel = pent->model;
		switch (clmodel->type)
		{
		default:
			Sys_Error("R_Efrag_store: Bad entity type %d\n", clmodel->type);
            break;
		case mod_alias:
		case mod_brush:
		case mod_sprite:
			if ((pent->visframe != r_framecount) && (cl_numvisedicts < MAX_VISEDICTS))
			{
				cl_visedicts[cl_numvisedicts++] = pent;
				// mark that we've recorded this entity for this frame
				pent->visframe = r_framecount;
			}
			ppefrag = &pefrag->leafnext;
			break;
		}
	}
}

//--------------------------------------------------------------------------------
// World model.
//--------------------------------------------------------------------------------
static void R_WorldModel_markLeaves(model_t *worldModel)
{
	if (r_oldviewleaf == r_viewLeaf && !r_novis.value)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewLeaf;

	byte *vis;
	byte solid[4096];
	if (r_novis.value)
	{
		vis = solid;
		memset(solid, 0xff, (worldModel->numleafs + 7) >> 3);
	}
	else
		vis = Mod_LeafPVS(r_viewLeaf, worldModel);

    int n = worldModel->numleafs;
	for (int i = 0; i < n; i++)
	{
		if (vis[i >> 3] & (1 << (i & 7)))
		{
			mnode_t *node = (mnode_t *)&worldModel->leafs[i + 1];
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			}
			while (node);
		}
	}
}

static void R_WorldModel_drawR(model_t *worldModel, mnode_t *node, vec3_t modelOrigin)
{
	if (node->contents == CONTENTS_SOLID)
		return; // solid

	if (node->visframe != r_visframecount)
		return;

	if (R_CullBox(node->minmaxs, node->minmaxs + 3))
		return;

	// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		mleaf_t *pleaf = (mleaf_t *)node;
		msurface_t **mark = pleaf->firstmarksurface;
		int c = pleaf->nummarksurfaces;
		if (c)
		{
			do
			{
				(*mark)->visframe = r_framecount;
				mark++;
			}
			while (--c);
		}

		// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_Efrag_store(&pleaf->efrags);

		return;
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	mplane_t *plane = node->plane;
	float dot;
	switch (plane->type)
	{
	case PLANE_X:
		dot = modelOrigin[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelOrigin[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelOrigin[2] - plane->dist;
		break;
	default:
		dot = DotProduct(modelOrigin, plane->normal) - plane->dist;
		break;
	}

	int side = dot < 0;

	// recurse down the children, front side first
	R_WorldModel_drawR(worldModel, node->children[side], modelOrigin);

	// draw stuff
	int c = node->numsurfaces;
	if (c)
	{
        msurface_t *surf = worldModel->surfaces + node->firstsurface;
        for (; c; c--, surf++)
        {
            if (surf->visframe != r_framecount)
                continue;

            // don't backface underwater surfaces, because they warp
            if (!(surf->flags & SURF_UNDERWATER) && (side != ((surf->flags & SURF_PLANEBACK) != 0)))
                continue; // wrong side

            R_BrushModel_drawSurface(&r_worldentity, surf, 1.0f);
        }
	}

	// recurse down the back side
	R_WorldModel_drawR(worldModel, node->children[!side], modelOrigin);
}

static void R_WorldModel_draw(model_t *worldModel)
{
    if (r_draw_world.value)
    {
        r_brushModelCount++;
        R_WorldModel_drawR(worldModel, worldModel->nodes, r_refdef.vieworg);
        R_BrushModel_drawChains(worldModel);
        R_Lightmap_drawChains();
    }
}

//--------------------------------------------------------------------------------
// Sprite model.
//--------------------------------------------------------------------------------
static mspriteframe_t* R_SpriteModel_getFrame(entity_t *e)
{
	msprite_t *psprite = e->model->cache.data;
	int frame = e->frame;

	if (frame >= psprite->numframes || frame < 0)
	{
		Con_Printf("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	mspriteframe_t *pspriteframe;
	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		mspritegroup_t *pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		float *pintervals = pspritegroup->intervals;
		int numframes = pspritegroup->numframes;
		float fullinterval = pintervals[numframes - 1];
		float time = cl.time + e->syncbase;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		float targettime = time - ((int)(time / fullinterval)) * fullinterval;

        int i;
		for (i = 0; i < (numframes - 1); i++)
		{
			if (pintervals[i] > targettime)
				break;
		}
		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}

static void R_SpriteModel_draw(entity_t *e)
{
    r_spriteCount++;

	// don't even bother culling, because it's just a single polygon without a surface cache
	mspriteframe_t *frame = R_SpriteModel_getFrame(e);
	msprite_t *psprite = e->model->cache.data;

	vec3_t v_forward, v_right, v_up;
	float *up, *right;
	if (psprite->type == SPR_ORIENTED) // bullet marks on walls
	{
		AngleVectors(e->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else     // normal sprite
	{
		up = r_viewUp;
		right = r_viewRight;
	}

	oglwBindTexture(0, frame->textureId);

    oglwEnableBlending(true);
    oglwEnableAlphaTest(true);

	oglwBegin(GL_TRIANGLES);

	OglwVertex *vtx = oglwAllocateQuad(4);
    if (vtx != NULL)
    {
        vec3_t p;

        VectorMA(e->origin, frame->down, up, p);
        VectorMA(p, frame->left, right, p);
        vtx = AddVertex3D_T1(vtx, p[0], p[1], p[2], 0.0f, 1.0f);

        VectorMA(e->origin, frame->up, up, p);
        VectorMA(p, frame->left, right, p);
        vtx = AddVertex3D_T1(vtx, p[0], p[1], p[2], 0.0f, 0.0f);

        VectorMA(e->origin, frame->up, up, p);
        VectorMA(p, frame->right, right, p);
        vtx = AddVertex3D_T1(vtx, p[0], p[1], p[2], 1.0f, 0.0f);

        VectorMA(e->origin, frame->down, up, p);
        VectorMA(p, frame->right, right, p);
        vtx = AddVertex3D_T1(vtx, p[0], p[1], p[2], 1.0f, 1.0f);
    }
	oglwEnd();

    oglwEnableBlending(false);
    oglwEnableAlphaTest(false);
}

//--------------------------------------------------------------------------------
// Alias model.
//--------------------------------------------------------------------------------
// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
static const float r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "Rendering/anorm_dots.h"
;

static void R_AliasModel_drawFrame(entity_t *e, aliashdr_t *paliashdr, int pose0, int pose1, float poseBlend, float light)
{
	const float *shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0f))) & (SHADEDOT_QUANT - 1)];

	trivertx_t *verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
    trivertx_t *verts0 = verts + pose0 * paliashdr->poseverts;
    trivertx_t *verts1 = verts + pose1 * paliashdr->poseverts;
	int *order = (int *)((byte *)paliashdr + paliashdr->commands);

	oglwBegin(GL_TRIANGLES);
	while (1)
	{
		// get the vertex count and primitive type
		int count = *order++;
		if (!count)
			break;

		OglwVertex *vtx;
		if (count < 0)
		{
			count = -count;
			vtx = oglwAllocateTriangleFan(count);
		}
		else
		{
			vtx = oglwAllocateTriangleStrip(count);
		}
        if (vtx == NULL)
            break;

        if (pose0 == pose1)
        {
            do
            {
                // normals and vertexes come from the frame list
                float l = shadedots[verts0->lightnormalindex] * light;
                // texture coordinates come from the draw list
                float *tc = (float *)order;
                vtx = AddVertex3D_CT1(vtx, verts0->v[0], verts0->v[1], verts0->v[2], l, l, l, 1.0f, tc[0], tc[1]);
                order += 2;
                verts0++;
            }
            while (--count);
        }
        else
        {
            do
            {
                // normals and vertexes come from the frame list
                float l = shadedots[verts0->lightnormalindex] * light;
                // texture coordinates come from the draw list
                float *tc = (float *)order;
                float vx = Lerp(verts0->v[0], verts1->v[0], poseBlend);
                float vy = Lerp(verts0->v[1], verts1->v[1], poseBlend);
                float vz = Lerp(verts0->v[2], verts1->v[2], poseBlend);
                vtx = AddVertex3D_CT1(vtx, vx, vy, vz, l, l, l, 1.0f, tc[0], tc[1]);
                order += 2;
                verts0++;
                verts1++;
            }
            while (--count);
        }
	}
	oglwEnd();
}

static void R_AliasModel_drawShadow(entity_t *e, aliashdr_t *paliashdr, int pose0, int pose1, float poseBlend, vec3_t lightspot)
{
	trivertx_t *verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += pose0 * paliashdr->poseverts;
	int *order = (int *)((byte *)paliashdr + paliashdr->commands);

	float lheight = e->origin[2] - lightspot[2];
	float height = -lheight + 1.0f;

    vec3_t shadevector;
    {
        float an = e->angles[1] / 180 * Q_PI;
        shadevector[0] = cosf(-an);
        shadevector[1] = sinf(-an);
        shadevector[2] = 1;
        VectorNormalize(shadevector);
    }
    
    vec3_t scale, scaleOrigin;
    VectorCopy(paliashdr->scale, scale);
    VectorCopy(paliashdr->scale_origin, scaleOrigin);

	if (r_stencilAvailable && r_meshmodel_shadow_stencil.value)
		oglwEnableStencilTest(true);

	oglwBegin(GL_TRIANGLES);
	while (1)
	{
		// get the vertex count and primitive type
		int count = *order++;
		if (!count)
			break;      // done

		OglwVertex *vtx;
		if (count < 0)
		{
			count = -count;
			vtx = oglwAllocateTriangleFan(count);
		}
		else
		{
			vtx = oglwAllocateTriangleStrip(count);
		}
        if (vtx == NULL)
            break;

		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadows) glTexCoord2fv ((float *)order);
			// normals and vertexes come from the frame list
			vec3_t p;
			p[0] = verts->v[0] * scale[0] + scaleOrigin[0];
			p[1] = verts->v[1] * scale[1] + scaleOrigin[1];
			p[2] = verts->v[2] * scale[2] + scaleOrigin[2];

			p[0] -= shadevector[0] * (p[2] + lheight);
			p[1] -= shadevector[1] * (p[2] + lheight);
			p[2] = height;

			vtx = AddVertex3D_C(vtx, p[0], p[1], p[2], 0.0f, 0.0f, 0.0f, 0.5f);

			order += 2;
			verts++;
		}
		while (--count);
	}
	oglwEnd();

	if (r_stencilAvailable && r_meshmodel_shadow_stencil.value)
		oglwEnableStencilTest(false);
}

static void R_AliasModel_draw(model_t *worldModel, entity_t *e)
{
	model_t *clmodel = e->model;
	vec3_t mins, maxs;
	VectorAdd(e->origin, clmodel->mins, mins);
	VectorAdd(e->origin, clmodel->maxs, maxs);

	if (R_CullBox(mins, maxs))
		return;

    r_aliasModelCount++;

    vec3_t lightspot;
    float light = R_Lighmap_lightPoint(worldModel, e->origin, lightspot);
	if (!strcmp(clmodel->name, "progs/flame2.mdl") || !strcmp(clmodel->name, "progs/flame.mdl"))
    {
		light = 256; // HACK HACK HACK -- no fullbright colors, so make torches full light
    }
    else
    {
        // always give the gun some light
        if (e == &cl.viewent && light < 24)
            light = 24;

        for (int lnum = 0; lnum < MAX_DLIGHTS; lnum++)
        {
            if (cl_dlights[lnum].die >= cl.time)
            {
                float distance = VectorDistance(e->origin, cl_dlights[lnum].origin);
                float add = cl_dlights[lnum].radius - distance;
                if (add > 0)
                    light += add;
            }
        }

        // clamp lighting so it doesn't overbright as much
        if (light > 128)
            light = 128;

        // ZOID: never allow players to go totally black
        {
            int i = e - cl_entities;
            if (i >= 1 && i <= cl.maxclients /* && !strcmp (e->model->name, "progs/player.mdl") */)
                if (light < 8)
                    light = 8;
        }
    }
	light *= (1.0f / 200.0f);
    
	//
	// locate the proper data
	//
	aliashdr_t *paliashdr = (aliashdr_t *)Mod_Extradata(clmodel);
	r_aliasModelPolyCount += paliashdr->numtris;

    int frame = e->frame;
	if (frame >= paliashdr->numframes || frame < 0)
	{
		Con_DPrintf("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}
	int pose = paliashdr->frames[frame].firstpose;
	int numposes = paliashdr->frames[frame].numposes;
	if (numposes > 1)
	{
		float interval = paliashdr->frames[frame].interval;
        float frameTime = cl.time / interval;
        int framePose = (int)frameTime;
		pose += framePose % numposes;
	}

	//
	// draw all the triangles
	//

	oglwPushMatrix();
	R_RotateForEntity(e);

	if (!strcmp(clmodel->name, "progs/eyes.mdl") && r_meshmodel_double_eyes.value)
	{
		oglwTranslate(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
		// double size of eyes, since they are really hard to see in gl
		oglwScale(paliashdr->scale[0] * 2, paliashdr->scale[1] * 2, paliashdr->scale[2] * 2);
	}
	else
	{
		oglwTranslate(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		oglwScale(paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}

    {
        int anim = (int)(cl.time * 10) & 3;
        int texture = paliashdr->textureId[e->skinnum][anim];
        // we can't dynamically colormap textures, so they are cached
        // seperately for the players.  Heads are just uncolored.
        if (e->colormap != vid.colormap && !r_player_nocolors.value)
        {
            int i = e - cl_entities;
            if (i >= 1 && i <= cl.maxclients /* && !strcmp (e->model->name, "progs/player.mdl") */)
                texture = r_playerTextures - 1 + i;
        }
        oglwBindTexture(0, texture);
    }

	if (r_meshmodel_smooth_shading.value)
		oglwEnableSmoothShading(true);
	oglwSetTextureBlending(0, GL_MODULATE);

    #if defined(EGLW_GLES1)
	if (r_meshmodel_affine_filtering.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    #endif

	R_AliasModel_drawFrame(e, paliashdr, pose, pose, 0.0f, light);

	oglwSetTextureBlending(0, GL_REPLACE);

    oglwEnableSmoothShading(false);
    #if defined(EGLW_GLES1)
	if (r_meshmodel_affine_filtering.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    #endif

	oglwPopMatrix();

	if (r_meshmodel_shadow.value)
	{
		oglwPushMatrix();
		R_RotateForEntity(e);
		oglwEnableTexturing(0, false);
        oglwEnableBlending(true);
		R_AliasModel_drawShadow(e, paliashdr, pose, pose, 0.0f, lightspot);
		oglwEnableTexturing(0, true);
        oglwEnableBlending(false);
		oglwPopMatrix();
	}
}

//--------------------------------------------------------------------------------
// Entities.
//--------------------------------------------------------------------------------
static void R_Entities_drawViewModel(model_t *worldModel)
{
	if (!r_draw_viewmodel.value)
		return;

	if (chase_active.value)
		return;

	if (r_envMap)
		return;

	if (!r_draw_entities.value)
		return;

	if (cl.items & IT_INVISIBILITY)
		return;

	if (cl.stats[STAT_HEALTH] <= 0)
		return;

    entity_t *e = &cl.viewent;
	if (!e->model)
		return;

	// hack the depth range to prevent view model from poking into walls
	oglwSetDepthRange(r_depthMin, r_depthMin + 0.3f * (r_depthMax - r_depthMin));
	R_AliasModel_draw(worldModel, e);
	oglwSetDepthRange(r_depthMin, r_depthMax);
}

static void R_Entities_draw(model_t *worldModel)
{
	if (!r_draw_entities.value)
		return;

	// draw sprites separately, because of alpha blending
	for (int i = 0; i < cl_numvisedicts; i++)
	{
        entity_t *e = cl_visedicts[i];
		switch (e->model->type)
		{
		default:
			break;
		case mod_alias:
			R_AliasModel_draw(worldModel, e);
			break;
		case mod_brush:
			R_BrushModel_draw(worldModel, e);
			break;
		}
	}

	for (int i = 0; i < cl_numvisedicts; i++)
	{
        entity_t *e = cl_visedicts[i];
		switch (e->model->type)
		{
		case mod_sprite:
			R_SpriteModel_draw(e);
			break;
		}
	}
}

//--------------------------------------------------------------------------------
// Clear.
//--------------------------------------------------------------------------------
static void R_clear()
{
    GLuint flags = 0;
    GLenum depthFunc;

	if (r_clear.value)
    {
		flags |= GL_COLOR_BUFFER_BIT;
		#if defined(DEBUG)
		glClearColor(1.0f, 0.0f, 0.5f, 0.5f);
		#else
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		#endif
    }

	if (r_ztrick.value)
	{
		static int trickframe;

		if (r_clear.value)
			flags = GL_COLOR_BUFFER_BIT;

		trickframe++;
		if (trickframe & 1)
		{
			r_depthMin = 0;
			r_depthMax = 0.49999f;
			depthFunc = GL_LEQUAL;
		}
		else
		{
			r_depthMin = 1;
			r_depthMax = 0.5f;
			depthFunc = GL_GEQUAL;
		}
	}
	else
	{
        flags |= GL_DEPTH_BUFFER_BIT;
		r_depthMin = 0;
		r_depthMax = 1;
		depthFunc = GL_LEQUAL;
        glClearDepthf(1.0f);
	}

	// Stencil buffer shadows.
	if (r_meshmodel_shadow.value && r_stencilAvailable && r_meshmodel_shadow_stencil.value)
	{
		flags |= GL_STENCIL_BUFFER_BIT;
		glClearStencil(0);
	}

    if (flags != 0)
        oglwClear(flags);
        
    glDepthFunc(depthFunc);
    
	oglwSetDepthRange(r_depthMin, r_depthMax);

	if (r_zfix.value)
	{
		if (r_depthMax > r_depthMin)
			glPolygonOffset(0.05f, 1);
		else
			glPolygonOffset(-0.05f, -1);
	}
}

//--------------------------------------------------------------------------------
// Full screen.
//--------------------------------------------------------------------------------
static void R_FullScreen_draw()
{
	if (!r_fullscreenfx.value)
		return;

	if (!v_blend[3])
		return;

    oglwEnableBlending(true);
    oglwEnableDepthTest(false);
	oglwEnableTexturing(0, false);

	oglwLoadIdentity();

	oglwRotate(-90, 1, 0, 0); // put Z going up
	oglwRotate(90, 0, 0, 1); // put Z going up

	oglwBegin(GL_TRIANGLES);
	OglwVertex *vtx = oglwAllocateQuad(4);
    if (vtx != NULL)
    {
        vtx = AddVertex3D_C(vtx, 10, 100, 100, v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
        vtx = AddVertex3D_C(vtx, 10, -100, 100, v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
        vtx = AddVertex3D_C(vtx, 10, -100, -100, v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
        vtx = AddVertex3D_C(vtx, 10, 100, -100, v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
    }
	oglwEnd();

    oglwEnableBlending(false);
	oglwEnableTexturing(0, true);
}

//--------------------------------------------------------------------------------
// Frustum.
//--------------------------------------------------------------------------------
static int R_Frustum_getSignBitsForPlane(mplane_t *out)
{
	// for fast box on planeside test
	int bits = 0;
	for (int j = 0; j < 3; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}

static void R_Frustum_setup()
{
	if (r_refdef.fov_x == 90)
	{
		// front side is visible

		VectorAdd(r_viewZ, r_viewRight, r_frustum[0].normal);
		VectorSubtract(r_viewZ, r_viewRight, r_frustum[1].normal);

		VectorAdd(r_viewZ, r_viewUp, r_frustum[2].normal);
		VectorSubtract(r_viewZ, r_viewUp, r_frustum[3].normal);
	}
	else
	{
		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector(r_frustum[0].normal, r_viewUp, r_viewZ, -(90 - r_refdef.fov_x / 2));
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector(r_frustum[1].normal, r_viewUp, r_viewZ, 90 - r_refdef.fov_x / 2);
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector(r_frustum[2].normal, r_viewRight, r_viewZ, 90 - r_refdef.fov_y / 2);
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector(r_frustum[3].normal, r_viewRight, r_viewZ, -(90 - r_refdef.fov_y / 2));
	}

	for (int i = 0; i < 4; i++)
	{
		r_frustum[i].type = PLANE_ANYZ;
		r_frustum[i].dist = DotProduct(r_viewOrigin, r_frustum[i].normal);
		r_frustum[i].signbits = R_Frustum_getSignBitsForPlane(&r_frustum[i]);
	}
}

//--------------------------------------------------------------------------------
// View.
//--------------------------------------------------------------------------------
void R_setupFrame()
{
	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy(r_refdef.vieworg, r_viewOrigin);
	AngleVectors(r_refdef.viewangles, r_viewZ, r_viewRight, r_viewUp);

    model_t *worldModel = cl.worldmodel;

	// current viewleaf
	r_oldviewleaf = r_viewLeaf;
	r_viewLeaf = Mod_PointInLeaf(r_viewOrigin, worldModel);

    r_spriteCount = 0; r_aliasModelCount = 0; r_brushModelCount = 0;

    r_surfaceTextureCount = 0;

    r_surfaceTotalCount = 0; r_surfaceBaseCount = 0; r_surfaceLightmappedCount = 0; r_surfaceMultitexturedCount = 0;
    r_surfaceUnderwaterTotalCount = 0; r_surfaceUnderWaterBaseCount = 0; r_surfaceUnderWaterLightmappedCount = 0; r_surfaceUnderWaterMultitexturedCount = 0;
    r_surfaceWaterCount = 0;
    r_surfaceSkyCount = 0;
    r_surfaceGlobalCount = 0;
    r_surfaceBaseTotalCount = 0;
    r_surfaceLightmappedTotalCount = 0;
    r_surfaceMultitexturedTotalCount = 0;

    r_surfaceTotalPolyCount = 0; r_surfaceBasePolyCount = 0; r_surfaceLightmappedPolyCount = 0; r_surfaceMultitexturedPolyCount = 0;
    r_surfaceUnderWaterTotalPolyCount = 0; r_surfaceUnderWaterBasePolyCount = 0; r_surfaceUnderWaterLightmappedPolyCount = 0; r_surfaceUnderWaterMultitexturedPolyCount = 0;
    r_surfaceWaterPolyCount = 0;
    r_surfaceSkyPolyCount = 0;
    r_surfaceGlobalPolyCount = 0;
    r_surfaceBaseTotalPolyCount = 0;
    r_surfaceLightmappedTotalPolyCount = 0;
    r_surfaceMultitexturedTotalPolyCount = 0;

    r_aliasModelPolyCount = 0;

	V_SetContentsColor(r_viewLeaf->contents);
	V_CalcBlend();

	// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
		Cvar_Set("r_lightmap_disabled", "0");

    R_Lightmap_uploadDelayed();
	R_LightStyles_animate();
    R_DynamicLights_push(worldModel, worldModel->nodes);
}

static void R_setupPerspective(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar)
{
	GLfloat ymax = zNear * tan(fovy * Q_PI / 360.0f);
	GLfloat ymin = -ymax;
	GLfloat xmin = ymin * aspect;
	GLfloat xmax = ymax * aspect;
	oglwFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

static void R_setup3D()
{
	// set up viewpoint
	oglwMatrixMode(GL_PROJECTION);
	oglwLoadIdentity();
	int x = r_refdef.vrect.x * r_viewportWidth / vid.width;
	int x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * r_viewportWidth / vid.width;
	int y = (vid.height - r_refdef.vrect.y) * r_viewportHeight / vid.height;
	int y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * r_viewportHeight / vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < r_viewportWidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < r_viewportHeight)
		y++;

	int w = x2 - x;
	int h = y - y2;

	if (r_envMap)
	{
		x = y2 = 0;
		w = h = 256;
	}

	oglwSetViewport(r_viewportX + x, r_viewportY + y2, w, h);
	float screenaspect = (float)r_refdef.vrect.width / r_refdef.vrect.height;
	//	yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/Q_PI;
	R_setupPerspective(r_refdef.fov_y, screenaspect, 4, 4096);

	oglwMatrixMode(GL_MODELVIEW);
	oglwLoadIdentity();

	oglwRotate(-90, 1, 0, 0); // put Z going up
	oglwRotate(90, 0, 0, 1); // put Z going up
	oglwRotate(-r_refdef.viewangles[2], 1, 0, 0);
	oglwRotate(-r_refdef.viewangles[0], 0, 1, 0);
	oglwRotate(-r_refdef.viewangles[1], 0, 0, 1);
	oglwTranslate(-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);

	oglwGetMatrix(GL_MODELVIEW, r_world_matrix);

	glCullFace(GL_FRONT);
	if (r_cull.value)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

    oglwEnableBlending(false);
	oglwSetBlendingFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    oglwEnableAlphaTest(false);
    oglwSetAlphaFunc(GL_GREATER, 0.666f);
	oglwEnableDepthTest(true);
    oglwEnableDepthWrite(true);
	oglwEnableStencilTest(false);
	if (r_meshmodel_shadow.value && r_stencilAvailable && r_meshmodel_shadow_stencil.value)
	{
		glStencilFunc(GL_EQUAL, 0, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
	}

    oglwEnableSmoothShading(false);
    oglwEnableTexturing(0, true);
    oglwEnableTexturing(1, false);
	oglwSetTextureBlending(0, GL_REPLACE);
	oglwSetTextureBlending(1, GL_MODULATE);
}

// r_refdef must be set before the first call
static void R_renderScene(model_t *worldModel)
{
	R_Frustum_setup();
	R_setup3D();
	R_WorldModel_markLeaves(worldModel); // done here so we know if we're in water
	R_WorldModel_draw(worldModel); // adds static entities to the list
	S_ExtraUpdate(); // don't let sound get messed up if going slow
	R_Entities_draw(worldModel);
	R_LightFlash_renderAll();
	R_Particles_draw();
}

void R_renderView()
{
	if (r_disabled.value)
		return;

    model_t *worldModel = cl.worldmodel;
	if (!worldModel)
		Sys_Error("R_renderView: NULL world model");

	R_clear();
	R_renderScene(worldModel);
	R_Entities_drawViewModel(worldModel);
	R_FullScreen_draw();

    r_surfaceTotalCount = r_surfaceBaseCount + r_surfaceLightmappedCount + r_surfaceMultitexturedCount;
    r_surfaceUnderwaterTotalCount = r_surfaceUnderWaterBaseCount + r_surfaceUnderWaterLightmappedCount + r_surfaceUnderWaterMultitexturedCount;
    r_surfaceGlobalCount = r_surfaceTotalCount + r_surfaceUnderwaterTotalCount + r_surfaceWaterCount + r_surfaceSkyCount;
    r_surfaceBaseTotalCount = r_surfaceBaseCount + r_surfaceUnderWaterBaseCount;
    r_surfaceLightmappedTotalCount = r_surfaceLightmappedCount + r_surfaceUnderWaterLightmappedCount;
    r_surfaceMultitexturedTotalCount = r_surfaceMultitexturedCount + r_surfaceUnderWaterMultitexturedCount;

    r_surfaceTotalPolyCount = r_surfaceBasePolyCount + r_surfaceLightmappedPolyCount + r_surfaceMultitexturedPolyCount;
    r_surfaceUnderWaterTotalPolyCount = r_surfaceUnderWaterBasePolyCount + r_surfaceUnderWaterLightmappedPolyCount + r_surfaceUnderWaterMultitexturedPolyCount;
    r_surfaceGlobalPolyCount = r_surfaceTotalPolyCount + r_surfaceUnderWaterTotalPolyCount + r_surfaceWaterPolyCount + r_surfaceSkyPolyCount;
    r_surfaceBaseTotalPolyCount = r_surfaceBasePolyCount + r_surfaceUnderWaterBasePolyCount;
    r_surfaceLightmappedTotalPolyCount = r_surfaceLightmappedPolyCount + r_surfaceUnderWaterLightmappedPolyCount;
    r_surfaceMultitexturedTotalPolyCount = r_surfaceMultitexturedPolyCount + r_surfaceUnderWaterMultitexturedPolyCount;
}

//--------------------------------------------------------------------------------
// Player skin.
//--------------------------------------------------------------------------------
// Translates a skin texture by the per-player color lookup
void R_TranslatePlayerSkin(int playernum)
{
	int top = cl.scores[playernum].colors & 0xf0;
	int bottom = (cl.scores[playernum].colors & 15) << 4;

	byte translate[256];
	for (int i = 0; i < 256; i++)
		translate[i] = i;

	for (int i = 0; i < 16; i++)
	{
		if (top < 128) // the artists made some backwards ranges.  sigh.
			translate[TOP_RANGE + i] = top + i;
		else
			translate[TOP_RANGE + i] = top + 15 - i;

		if (bottom < 128)
			translate[BOTTOM_RANGE + i] = bottom + i;
		else
			translate[BOTTOM_RANGE + i] = bottom + 15 - i;
	}

	//
	// locate the original skin pixels
	//
    entity_t *e = &cl_entities[1 + playernum];
	model_t *model = e->model;
	if (!model)
		return; // player doesn't have a model yet

	if (model->type != mod_alias)
		return; // only translate skins on alias models

	aliashdr_t *paliashdr = (aliashdr_t *)Mod_Extradata(model);
	int s = paliashdr->skinwidth * paliashdr->skinheight;
	byte *original;
	if (e->skinnum < 0 || e->skinnum >= paliashdr->numskins)
	{
		Con_Printf("(%d): Invalid player skin #%d\n", playernum, e->skinnum);
		original = (byte *)paliashdr + paliashdr->texels[0];
	}
	else
		original = (byte *)paliashdr + paliashdr->texels[e->skinnum];
	if (s & 3)
		Sys_Error("R_TranslateSkin: s&3");

	int inwidth = paliashdr->skinwidth;
	int inheight = paliashdr->skinheight;

	unsigned translate32[256];
	for (int i = 0; i < 256; i++)
		translate32[i] = d_8to24table[translate[i]];

    oglwSetCurrentTextureUnitForced(0);
	oglwBindTextureForced(0, r_playerTextures + playernum);
    R_Texture_load(original, inwidth, inheight, true, false, (int)r_player_downsampling.value, translate32);
}

//--------------------------------------------------------------------------------
// Profiling.
//--------------------------------------------------------------------------------
// For program optimization
static void R_TimeRefresh_f()
{
	glFinish();

	R_setupFrame();

	float start = Sys_FloatTime();
	for (int i = 0; i < 128; i++)
	{
		r_refdef.viewangles[1] = i / 128.0f * 360.0f;
		R_renderView();
	}

	glFinish();
	float stop = Sys_FloatTime();
	float time = stop - start;
	Con_Printf("%f seconds (%f fps)\n", time, 128 / time);

	R_endRendering();
}

//--------------------------------------------------------------------------------
// Environment map.
//--------------------------------------------------------------------------------
// Grab six views for environment mapping tests
static void R_Envmap_f()
{
	int bufferSize = 256 * 256 * 4;
	byte * buffer = malloc(bufferSize);

	r_envMap = true;

	R_setupFrame();

	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;
	r_refdef.vrect.width = 256;
	r_refdef.vrect.height = 256;

	r_refdef.viewangles[0] = 0;
	r_refdef.viewangles[1] = 0;
	r_refdef.viewangles[2] = 0;
	R_beginRendering(&r_viewportX, &r_viewportY, &r_viewportWidth, &r_viewportHeight);
	R_renderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env0.rgb", buffer, bufferSize);

	r_refdef.viewangles[1] = 90;
	R_beginRendering(&r_viewportX, &r_viewportY, &r_viewportWidth, &r_viewportHeight);
	R_renderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env1.rgb", buffer, bufferSize);

	r_refdef.viewangles[1] = 180;
	R_beginRendering(&r_viewportX, &r_viewportY, &r_viewportWidth, &r_viewportHeight);
	R_renderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env2.rgb", buffer, bufferSize);

	r_refdef.viewangles[1] = 270;
	R_beginRendering(&r_viewportX, &r_viewportY, &r_viewportWidth, &r_viewportHeight);
	R_renderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env3.rgb", buffer, bufferSize);

	r_refdef.viewangles[0] = -90;
	r_refdef.viewangles[1] = 0;
	R_beginRendering(&r_viewportX, &r_viewportY, &r_viewportWidth, &r_viewportHeight);
	R_renderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env4.rgb", buffer, bufferSize);

	r_refdef.viewangles[0] = 90;
	r_refdef.viewangles[1] = 0;
	R_beginRendering(&r_viewportX, &r_viewportY, &r_viewportWidth, &r_viewportHeight);
	R_renderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env5.rgb", buffer, bufferSize);

	r_envMap = false;
	R_endRendering();
}

//--------------------------------------------------------------------------------
// Map.
//--------------------------------------------------------------------------------
void R_beginNewMap()
{
	for (int i = 0; i < 256; i++)
		r_lightStyleValue[i] = 264; // normal light value

    entity_t *entity = &r_worldentity;
	memset(entity, 0, sizeof(entity_t));
    model_t *model = cl.worldmodel;
	entity->model = model;

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for (int i = 0; i < model->numleafs; i++)
		model->leafs[i].efrags = NULL;

	r_viewLeaf = NULL;

	R_Particles_clear();

	R_Lightmap_buildAllSurfaces();

	// identify sky texture
	r_skyTexture = NULL;
	for (int i = 0; i < model->numtextures; i++)
	{
        texture_t *texture = model->textures[i];
		if (texture)
        {
            if (!Q_strncmp(texture->name, "sky", 3))
                r_skyTexture = texture;
            texture->usedTextureChain = NULL;
            texture->surfaceList = NULL;
        }
	}
}

//--------------------------------------------------------------------------------
// Initialization.
//--------------------------------------------------------------------------------
void R_Init()
{
	Cmd_AddCommand("timerefresh", R_TimeRefresh_f);
	Cmd_AddCommand("r_envMap", R_Envmap_f);

	Cvar_RegisterVariable(&r_fullscreen);

	Cvar_RegisterVariable(&r_texture_nobind);
	Cvar_RegisterVariable(&r_texture_maxsize);
	Cvar_RegisterVariable(&r_texture_downsampling);
	Cmd_AddCommand("r_texture_filtering", &R_Texture_filtering_f);

	Cvar_RegisterVariable(&r_disabled);

	Cvar_RegisterVariable(&r_clear);
	Cvar_RegisterVariable(&r_ztrick);
	Cvar_RegisterVariable(&r_zfix);

	Cvar_RegisterVariable(&r_cull);

	Cvar_RegisterVariable(&r_novis);
	Cvar_RegisterVariable(&r_draw_world);
	Cvar_RegisterVariable(&r_draw_entities);
	Cvar_RegisterVariable(&r_draw_viewmodel);

	Cvar_RegisterVariable(&r_lightmap_disabled);
	Cvar_RegisterVariable(&r_lightmap_only);
	Cvar_RegisterVariable(&r_lightmap_dynamic);
    Cvar_RegisterVariable(&r_lightmap_backface_lighting);
	Cvar_RegisterVariable(&r_lightmap_upload_full);
	Cvar_RegisterVariable(&r_lightmap_upload_delayed);
    Cvar_RegisterVariable(&r_lightmap_mipmap);
	Cvar_RegisterVariable(&r_lightflash);

	Cvar_RegisterVariable(&r_tjunctions_keep);
	Cvar_RegisterVariable(&r_tjunctions_report);
	Cvar_RegisterVariable(&r_sky_subdivision);
	Cvar_RegisterVariable(&r_water_subdivision);
	Cvar_RegisterVariable(&r_texture_sort);
	Cvar_RegisterVariable(&r_multitexturing);

	Cvar_RegisterVariable(&r_meshmodel_shadow);
	Cvar_RegisterVariable(&r_meshmodel_shadow_stencil);
	Cvar_RegisterVariable(&r_meshmodel_smooth_shading);
	Cvar_RegisterVariable(&r_meshmodel_affine_filtering);
	Cvar_RegisterVariable(&r_meshmodel_double_eyes);
    
	Cvar_RegisterVariable(&r_player_downsampling);
	Cvar_RegisterVariable(&r_player_nocolors);

	Cvar_RegisterVariable(&r_fullscreenfx);

	R_Particles_initialize();

	#ifdef GLTEST
	Test_Init();
	#endif

	r_playerTextures = r_textureNb;
	r_textureNb += 16;
}
