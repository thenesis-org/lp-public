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
// Surface-related refresh code

#include "Client/client.h"
#include "Common/quakedef.h"
#include "Common/sys.h"
#include "Rendering/gl_model.h"
#include "Rendering/glquake.h"

#include "OpenGLES/OpenGLWrapper.h"

#include <stdlib.h>
#include <string.h>

extern model_t *loadmodel;
extern cvar_t gl_subdivide_size;

qboolean R_CullBox(vec3_t mins, vec3_t maxs);
void R_MarkLights(dlight_t *light, int bit, mnode_t *node);
void R_RotateForEntity(entity_t *e);
void R_StoreEfrags(efrag_t **ppefrag);

int skytexturenum;
static int solidskytexture;
static int alphaskytexture;
static float speedscale; // for top sky and bottom sky
static msurface_t *warpface;
static int lightmap_textures;
static unsigned blocklights[18 * 18];

#define BLOCK_WIDTH 128
#define BLOCK_HEIGHT 128

#define MAX_LIGHTMAPS 64

typedef struct glRect_s
{
	unsigned char l, t, w, h;
} glRect_t;

static glpoly_t *lightmap_polys[MAX_LIGHTMAPS];
static qboolean lightmap_modified[MAX_LIGHTMAPS];
static glRect_t lightmap_rectchange[MAX_LIGHTMAPS];
static int allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
static byte lightmaps[4 * MAX_LIGHTMAPS * BLOCK_WIDTH * BLOCK_HEIGHT];

// For gl_texsort 0
static msurface_t *skychain = NULL;
static msurface_t *waterchain = NULL;

void R_AddDynamicLights(msurface_t *surf)
{
	int lnum;
	int sd, td;
	float dist, rad, minlight;
	vec3_t impact, local;
	int s, t;
	int i;
	int smax, tmax;
	mtexinfo_t *tex;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (!(surf->dlightbits & (1 << lnum)))
			continue;                                 // not lit by this light

		rad = cl_dlights[lnum].radius;
		dist = DotProduct(cl_dlights[lnum].origin, surf->plane->normal) - surf->plane->dist;
		rad -= fabsf(dist);
		minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		for (i = 0; i < 3; i++)
		{
			impact[i] = cl_dlights[lnum].origin[i] - surf->plane->normal[i] * dist;
		}

		local[0] = DotProduct(impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct(impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

		for (t = 0; t < tmax; t++)
		{
			td = local[1] - t * 16;
			if (td < 0)
				td = -td;
			for (s = 0; s < smax; s++)
			{
				sd = local[0] - s * 16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td >> 1);
				else
					dist = td + (sd >> 1);
				if (dist < minlight)
					blocklights[t * smax + s] += (rad - dist) * 256;
			}
		}
	}
}

/*
   Combine and scale multiple lightmaps into the 8.8 format in blocklights
 */
void R_BuildLightMap(msurface_t *surf, byte *dest, int stride)
{
	surf->cached_dlight = (surf->dlightframe == r_framecount);

	int smax = (surf->extents[0] >> 4) + 1;
	int tmax = (surf->extents[1] >> 4) + 1;
	int size = smax * tmax;

	// set to full bright if no light data
	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		for (int i = 0; i < size; i++)
			blocklights[i] = 255 * 256;
		goto store;
	}

	// clear to no light
	for (int i = 0; i < size; i++)
		blocklights[i] = 0;

	// add all the lightmaps
	byte *lightmap = surf->samples;
	if (lightmap)
		for (int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		{
			unsigned scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale; // 8.8 fraction
			for (int i = 0; i < size; i++)
				blocklights[i] += lightmap[i] * scale;
			lightmap += size; // skip to next lightmap
		}

	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights(surf);

	// bound, invert, and shift
store:
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
			dest[3] = 255 - t;
			dest += 4;
		}
	}
}

/*
   Returns the proper texture for a given time and base texture
 */
texture_t* R_TextureAnimation(texture_t *base)
{
	int reletive;
	int count;

	if (currententity->frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	reletive = (int)(cl.time * 10) % base->anim_total;

	count = 0;
	while (base->anim_min > reletive || base->anim_max <= reletive)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error("R_TextureAnimation: infinite cycle");
	}

	return base;
}

/*
   =============================================================

        BRUSH MODELS

   =============================================================
 */

static qboolean mtexenabled = false;

void GL_SelectTexture(GLenum target);

void GL_DisableMultitexture()
{
	if (mtexenabled)
	{
		oglwEnableTexturing(1, false);
		GL_SelectTexture(GL_TEXTURE0);
		mtexenabled = false;
	}
}

void GL_EnableMultitexture()
{
	if (gl_mtexable)
	{
		GL_SelectTexture(GL_TEXTURE1);
		oglwEnableTexturing(1, true);
		mtexenabled = true;
	}
}

/*
   Warp the vertex coordinates
 */
static void DrawGLWaterPoly(glpoly_t *p, float alpha)
{
	int n = p->numverts;
	OglwVertex *vtx = oglwAllocateTriangleFan(n);
	if (vtx == NULL)
		return;

	float time = realtime;
	float *v = p->verts[0];
	for (int i = 0; i < n; i++, v += VERTEXSIZE)
	{
		float sinz = sinf(v[2] * 0.05f + time);
		float px = v[0] + 8 * sinf(v[1] * 0.05f + time) * sinz;
		float py = v[1] + 8 * sinf(v[0] * 0.05f + time) * sinz;
		float pz = v[2];
		vtx = AddVertex3D_CT1(vtx, px, py, pz, 1.0f, 1.0f, 1.0f, alpha, v[3], v[4]);
	}
}

static void DrawGLWaterPolyLightmap(glpoly_t *p, float alpha)
{
	int n = p->numverts;
	OglwVertex *vtx = oglwAllocateTriangleFan(n);
	if (vtx == NULL)
		return;

	float time = realtime;
	float *v = p->verts[0];
	for (int i = 0; i < n; i++, v += VERTEXSIZE)
	{
		float sinz = sinf(v[2] * 0.05f + time);
		float px = v[0] + 8 * sinf(v[1] * 0.05f + time) * sinz;
		float py = v[1] + 8 * sinf(v[0] * 0.05f + time) * sinz;
		float pz = v[2];
		vtx = AddVertex3D_CT1(vtx, px, py, pz, 1.0f, 1.0f, 1.0f, alpha, v[5], v[6]);
	}
}

static void DrawGLWaterPolyMultitextured(glpoly_t *p, float alpha)
{
	int n = p->numverts;
	OglwVertex *vtx = oglwAllocateTriangleFan(n);
	if (vtx == NULL)
		return;

	float time = realtime;
	float *v = p->verts[0];
	for (int i = 0; i < n; i++, v += VERTEXSIZE)
	{
		float sinz = sinf(v[2] * 0.05f + time);
		float px = v[0] + 8 * sinf(v[1] * 0.05f + time) * sinz;
		float py = v[1] + 8 * sinf(v[0] * 0.05f + time) * sinz;
		float pz = v[2];
		vtx = AddVertex3D_CT2(vtx, px, py, pz, 1.0f, 1.0f, 1.0f, alpha, v[3], v[4], v[5], v[6]);
	}
}

static void DrawGLPoly(glpoly_t *p, float alpha)
{
	int n = p->numverts;
	OglwVertex *vtx = oglwAllocateTriangleFan(n);
	if (vtx == NULL)
		return;

	float *v = p->verts[0];
	for (int i = 0; i < n; i++, v += VERTEXSIZE)
	{
		vtx = AddVertex3D_CT1(vtx, v[0], v[1], v[2], 1.0f, 1.0f, 1.0f, alpha, v[3], v[4]);
	}
}

static void DrawGLPolyLightmap(glpoly_t *p, float alpha)
{
	int n = p->numverts;
	OglwVertex *vtx = oglwAllocateTriangleFan(n);
	if (vtx == NULL)
		return;

	float *v = p->verts[0];
	for (int i = 0; i < n; i++, v += VERTEXSIZE)
	{
		vtx = AddVertex3D_CT1(vtx, v[0], v[1], v[2], 1.0f, 1.0f, 1.0f, alpha, v[5], v[6]);
	}
}

static void DrawGLPolyMultitextured(glpoly_t *p, float alpha)
{
	int n = p->numverts;
	OglwVertex *vtx = oglwAllocateTriangleFan(n);
	if (vtx == NULL)
		return;

	float *v = p->verts[0];
	for (int i = 0; i < n; i++, v += VERTEXSIZE)
	{
		vtx = AddVertex3D_CT2(vtx, v[0], v[1], v[2], 1.0f, 1.0f, 1.0f, alpha, v[3], v[4], v[5], v[6]);
	}
}

/*
   Does a water warp on the pre-fragmented glpoly_t chain
 */
static void EmitWaterPolys(msurface_t *fa, float alpha)
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

	float TURBSCALE = (256.0f / (2 * Q_PI));
	float time = realtime;

	for (glpoly_t *p = fa->polys; p; p = p->next)
	{
		int n = p->numverts;
		OglwVertex *vtx = oglwAllocateTriangleFan(n);
		if (vtx != NULL)
		{
			float *v;
			int i;
			for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE)
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
}

/*
   Multitexture
 */
static void R_RenderDynamicLightmaps(msurface_t *fa)
{
	c_brush_polys++;

	if (fa->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	// check for lightmap modification
	int maps;
	for (maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255;
	     maps++)
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
			goto dynamic;

	if (fa->dlightframe == r_framecount // dynamic this frame
	    || fa->cached_dlight) // dynamic previously
	{
dynamic:
		if (r_dynamic.value)
		{
			lightmap_modified[fa->lightmaptexturenum] = true;
			glRect_t *theRect = &lightmap_rectchange[fa->lightmaptexturenum];
			if (fa->light_t < theRect->t)
			{
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t;
				theRect->t = fa->light_t;
			}
			if (fa->light_s < theRect->l)
			{
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s;
				theRect->l = fa->light_s;
			}
			int smax = (fa->extents[0] >> 4) + 1;
			int tmax = (fa->extents[1] >> 4) + 1;
			if ((theRect->w + theRect->l) < (fa->light_s + smax))
				theRect->w = (fa->light_s - theRect->l) + smax;
			if ((theRect->h + theRect->t) < (fa->light_t + tmax))
				theRect->h = (fa->light_t - theRect->t) + tmax;
			byte *base = lightmaps + fa->lightmaptexturenum * 4 * BLOCK_WIDTH * BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * 4 + fa->light_s * 4;
			R_BuildLightMap(fa, base, BLOCK_WIDTH * 4);
		}
	}
}

static void EmitSkyPolys(msurface_t *fa, float alpha)
{
	for (glpoly_t *p = fa->polys; p; p = p->next)
	{
		int n = p->numverts;
		OglwVertex *vtx = oglwAllocateTriangleFan(n);
		if (vtx != NULL)
		{
			int i;
			float *v;
			for (i = 0, v = p->verts[0]; i < n; i++, v += VERTEXSIZE)
			{
				vec3_t dir;
				VectorSubtract(v, r_origin, dir);
				dir[2] *= 3; // flatten the sphere

				float length = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
				length = sqrtf(length);
				length = 6 * 63 / length;

				dir[0] *= length;
				dir[1] *= length;

				float s = (speedscale + dir[0]) * (1.0f / 128);
				float t = (speedscale + dir[1]) * (1.0f / 128);

				vtx = AddVertex3D_CT1(vtx, v[0], v[1], v[2], 1.0f, 1.0f, 1.0f, alpha, s, t);
			}
		}
	}
}

/*
   Systems that have fast state and texture changes can
   just do everything as it passes with no need to sort
 */
void R_DrawSequentialPoly(msurface_t *s)
{
	int i;
	texture_t *t;
	glRect_t *theRect;

	//
	// normal lightmaped poly
	//

	if (!(s->flags & (SURF_DRAWSKY | SURF_DRAWTURB | SURF_UNDERWATER)))
	{
		R_RenderDynamicLightmaps(s);
		if (gl_mtexable)
		{
			t = R_TextureAnimation(s->texinfo->texture);
			// Binds world to texture env 0
			GL_SelectTexture(GL_TEXTURE0);
			GL_Bind(t->gl_texturenum);
			oglwSetTextureBlending(0, GL_REPLACE);
			// Binds lightmap to texenv 1
			GL_EnableMultitexture(); // Same as SelectTexture (TEXTURE1)
			GL_Bind(lightmap_textures + s->lightmaptexturenum);
			i = s->lightmaptexturenum;
			if (lightmap_modified[i])
			{
				lightmap_modified[i] = false;
				theRect = &lightmap_rectchange[i];
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t,
					BLOCK_WIDTH, theRect->h, GL_RGBA, GL_UNSIGNED_BYTE,
					lightmaps + (i * BLOCK_HEIGHT + theRect->t) * BLOCK_WIDTH * 4);
				theRect->l = BLOCK_WIDTH;
				theRect->t = BLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
			oglwSetTextureBlending(1, GL_BLEND);
			oglwBegin(GL_TRIANGLES);
			DrawGLPolyMultitextured(s->polys, 1.0f);
			oglwEnd();
			return;
		}
		else
		{
			t = R_TextureAnimation(s->texinfo->texture);
			GL_Bind(t->gl_texturenum);
			oglwBegin(GL_TRIANGLES);
			DrawGLPoly(s->polys, 1.0f);
			oglwEnd();

			oglwEnableBlending(true);
			GL_Bind(lightmap_textures + s->lightmaptexturenum);
			oglwBegin(GL_TRIANGLES);
			DrawGLPolyLightmap(s->polys, 1.0f);
			oglwEnd();
			oglwEnableBlending(false);
		}

		return;
	}

	//
	// subdivided water surface warp
	//

	if (s->flags & SURF_DRAWTURB)
	{
		GL_DisableMultitexture();
		GL_Bind(s->texinfo->texture->gl_texturenum);
		oglwBegin(GL_TRIANGLES);
		EmitWaterPolys(s, 1.0f);
		oglwEnd();
		return;
	}

	//
	// subdivided sky warp
	//
	if (s->flags & SURF_DRAWSKY)
	{
		GL_DisableMultitexture();

		GL_Bind(solidskytexture);
		speedscale = realtime * 8;
		speedscale -= (int)speedscale & ~127;
		oglwBegin(GL_TRIANGLES);
		EmitSkyPolys(s, 1.0f);
		oglwEnd();

		oglwEnableBlending(true);
		GL_Bind(alphaskytexture);
		speedscale = realtime * 16;
		speedscale -= (int)speedscale & ~127;
		oglwBegin(GL_TRIANGLES);
		EmitSkyPolys(s, 1.0f);
		oglwEnd();
		oglwEnableBlending(false);
		return;
	}

	//
	// underwater warped with lightmap
	//
	R_RenderDynamicLightmaps(s);
	if (gl_mtexable)
	{
		t = R_TextureAnimation(s->texinfo->texture);
		GL_SelectTexture(GL_TEXTURE0);
		GL_Bind(t->gl_texturenum);
		oglwSetTextureBlending(0, GL_REPLACE);
		GL_EnableMultitexture();
		GL_Bind(lightmap_textures + s->lightmaptexturenum);
		i = s->lightmaptexturenum;
		if (lightmap_modified[i])
		{
			lightmap_modified[i] = false;
			theRect = &lightmap_rectchange[i];
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t,
				BLOCK_WIDTH, theRect->h, GL_RGBA, GL_UNSIGNED_BYTE,
				lightmaps + (i * BLOCK_HEIGHT + theRect->t) * BLOCK_WIDTH * 4);
			theRect->l = BLOCK_WIDTH;
			theRect->t = BLOCK_HEIGHT;
			theRect->h = 0;
			theRect->w = 0;
		}
		oglwSetTextureBlending(1, GL_BLEND);
		oglwBegin(GL_TRIANGLES);
		DrawGLWaterPolyMultitextured(s->polys, 1.0f);
		oglwEnd();
	}
	else
	{
		GL_DisableMultitexture();

		t = R_TextureAnimation(s->texinfo->texture);
		GL_Bind(t->gl_texturenum);
		oglwBegin(GL_TRIANGLES);
		DrawGLWaterPoly(s->polys, 1.0f);
		oglwEnd();

		GL_Bind(lightmap_textures + s->lightmaptexturenum);
		oglwEnableBlending(true);
		oglwBegin(GL_TRIANGLES);
		DrawGLWaterPolyLightmap(s->polys, 1.0f);
		oglwEnd();
		oglwEnableBlending(false);
	}
}

static void R_BlendLightmaps()
{
	if (r_fullbright.value || !gl_texsort.value)
		return;

	GL_DisableMultitexture();

	oglwEnableDepthWrite(false);
	if (!r_lightmap.value)
		oglwEnableBlending(true);

	for (int i = 0; i < MAX_LIGHTMAPS; i++)
	{
		glpoly_t *p = lightmap_polys[i];
		if (!p)
			continue;
		GL_Bind(lightmap_textures + i);
		if (lightmap_modified[i])
		{
			lightmap_modified[i] = false;
			glRect_t *theRect = &lightmap_rectchange[i];
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, BLOCK_WIDTH, theRect->h, GL_RGBA, GL_UNSIGNED_BYTE, lightmaps + (i * BLOCK_HEIGHT + theRect->t) * BLOCK_WIDTH * 4);
			theRect->l = BLOCK_WIDTH;
			theRect->t = BLOCK_HEIGHT;
			theRect->h = 0;
			theRect->w = 0;
		}
		oglwBegin(GL_TRIANGLES);
		for (; p; p = p->chain)
		{
			if (p->flags & SURF_UNDERWATER)
				DrawGLWaterPolyLightmap(p, 1.0f);
			else
				DrawGLPolyLightmap(p, 1.0f);
		}
		oglwEnd();
	}

	if (!r_lightmap.value)
		oglwEnableBlending(false);
	oglwEnableDepthWrite(true);
}

/*
   Does a sky warp on the pre-fragmented glpoly_t chain
   This will be called for brushmodels, the world
   will have them chained together.
 */
static void EmitBothSkyLayers(msurface_t *fa)
{
	GL_DisableMultitexture();

	GL_Bind(solidskytexture);
	speedscale = realtime * 8;
	speedscale -= (int)speedscale & ~127;

	oglwBegin(GL_TRIANGLES);
	EmitSkyPolys(fa, 1.0f);
	oglwEnd();

	oglwEnableBlending(true);
	GL_Bind(alphaskytexture);
	speedscale = realtime * 16;
	speedscale -= (int)speedscale & ~127;

	oglwBegin(GL_TRIANGLES);
	EmitSkyPolys(fa, 1.0f);
	oglwEnd();

	oglwEnableBlending(false);
}

void R_RenderBrushPoly(msurface_t *fa, float alpha)
{
	c_brush_polys++;

	if (fa->flags & SURF_DRAWSKY) // warp texture, no lightmaps
	{
		EmitBothSkyLayers(fa);
	}
	else
	{
		texture_t *t = R_TextureAnimation(fa->texinfo->texture);
		GL_Bind(t->gl_texturenum);

		if (fa->flags & SURF_DRAWTURB) // warp texture, no lightmaps
		{
			oglwBegin(GL_TRIANGLES);
			EmitWaterPolys(fa, alpha);
			oglwEnd();
		}
		else
		{
			oglwBegin(GL_TRIANGLES);
			if (fa->flags & SURF_UNDERWATER)
				DrawGLWaterPoly(fa->polys, alpha);
			else
				DrawGLPoly(fa->polys, alpha);
			oglwEnd();

			// add the poly to the proper lightmap chain
			fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
			lightmap_polys[fa->lightmaptexturenum] = fa->polys;

			// check for lightmap modification
			for (int maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255;
			     maps++)
				if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
					goto dynamic;

			if (fa->dlightframe == r_framecount // dynamic this frame
			    || fa->cached_dlight) // dynamic previously
			{
dynamic:
				if (r_dynamic.value)
				{
					lightmap_modified[fa->lightmaptexturenum] = true;
					glRect_t *theRect = &lightmap_rectchange[fa->lightmaptexturenum];
					if (fa->light_t < theRect->t)
					{
						if (theRect->h)
							theRect->h += theRect->t - fa->light_t;
						theRect->t = fa->light_t;
					}
					if (fa->light_s < theRect->l)
					{
						if (theRect->w)
							theRect->w += theRect->l - fa->light_s;
						theRect->l = fa->light_s;
					}
					int smax = (fa->extents[0] >> 4) + 1;
					int tmax = (fa->extents[1] >> 4) + 1;
					if ((theRect->w + theRect->l) < (fa->light_s + smax))
						theRect->w = (fa->light_s - theRect->l) + smax;
					if ((theRect->h + theRect->t) < (fa->light_t + tmax))
						theRect->h = (fa->light_t - theRect->t) + tmax;
					byte *base = lightmaps + fa->lightmaptexturenum * 4 * BLOCK_WIDTH * BLOCK_HEIGHT;
					base += fa->light_t * BLOCK_WIDTH * 4 + fa->light_s * 4;
					R_BuildLightMap(fa, base, BLOCK_WIDTH * 4);
				}
			}
		}
	}
}

static void R_MirrorChain(msurface_t *s)
{
	if (mirror)
		return;

	mirror = true;
	mirror_plane = s->plane;
}

void R_DrawWaterSurfaces()
{
	float alpha = r_wateralpha.value;
	if (alpha == 1.0f && gl_texsort.value)
		return;

	//
	// go back to the world matrix
	//

	oglwLoadMatrix(r_world_matrix);

	if (alpha < 1.0f)
	{
		oglwEnableBlending(true);
		oglwSetTextureBlending(0, GL_MODULATE);
	}

	if (!gl_texsort.value)
	{
		for (msurface_t *s = waterchain; s; s = s->texturechain)
		{
			GL_Bind(s->texinfo->texture->gl_texturenum);
			oglwBegin(GL_TRIANGLES);
			EmitWaterPolys(s, alpha);
			oglwEnd();
		}

		waterchain = NULL;
	}
	else
	{
		for (int i = 0; i < cl.worldmodel->numtextures; i++)
		{
			texture_t *t = cl.worldmodel->textures[i];
			if (!t)
				continue;
			msurface_t *s = t->texturechain;
			if (!s)
				continue;
			if (!(s->flags & SURF_DRAWTURB))
				continue;

			// set modulate mode explicitly

			GL_Bind(t->gl_texturenum);
			oglwBegin(GL_TRIANGLES);
			for (; s; s = s->texturechain)
				EmitWaterPolys(s, alpha);
			oglwEnd();

			t->texturechain = NULL;
		}
	}

	if (alpha < 1.0f)
	{
		oglwSetTextureBlending(0, GL_REPLACE);
		oglwEnableBlending(false);
	}
}

static void R_DrawSkyChain(msurface_t *s)
{
	GL_DisableMultitexture();

	// used when gl_texsort is on
	GL_Bind(solidskytexture);
	speedscale = realtime * 8;
	speedscale -= (int)speedscale & ~127;

	oglwBegin(GL_TRIANGLES);
	for (msurface_t *fa = s; fa; fa = fa->texturechain)
		EmitSkyPolys(fa, 1.0f);
	oglwEnd();

	oglwEnableBlending(true);

	GL_Bind(alphaskytexture);
	speedscale = realtime * 16;
	speedscale -= (int)speedscale & ~127;

	oglwBegin(GL_TRIANGLES);
	for (msurface_t *fa = s; fa; fa = fa->texturechain)
		EmitSkyPolys(fa, 1.0f);
	oglwEnd();

	oglwEnableBlending(false);
}

static void DrawTextureChains()
{
	if (gl_texsort.value)
	{
		for (int i = 0; i < cl.worldmodel->numtextures; i++)
		{
			texture_t *t = cl.worldmodel->textures[i];
			if (!t)
				continue;
			msurface_t *s = t->texturechain;
			if (!s)
				continue;
			if (i == skytexturenum)
				R_DrawSkyChain(s);
			else if (i == mirrortexturenum && r_mirroralpha.value != 1.0f)
			{
				R_MirrorChain(s);
				continue;
			}
			else
			{
				if ((s->flags & SURF_DRAWTURB) && r_wateralpha.value != 1.0f)
					continue;  // draw translucent water later
				for (; s; s = s->texturechain)
					R_RenderBrushPoly(s, 1.0f);
			}

			t->texturechain = NULL;
		}
	}
	else
	{
		GL_DisableMultitexture();
		if (skychain)
		{
			R_DrawSkyChain(skychain);
			skychain = NULL;
		}
	}
}

void R_DrawBrushModel(entity_t *e)
{
	msurface_t *psurf;
	float dot;
	mplane_t *pplane;
	model_t *clmodel;
	qboolean rotated;

	currententity = e;
	currenttexture = -1;

	clmodel = e->model;

	vec3_t mins, maxs;
	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;
		for (int i = 0; i < 3; i++)
		{
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd(e->origin, clmodel->mins, mins);
		VectorAdd(e->origin, clmodel->maxs, maxs);
	}

	if (R_CullBox(mins, maxs))
		return;

	memset(lightmap_polys, 0, sizeof(lightmap_polys));

    vec3_t modelOrigin;
	VectorSubtract(r_refdef.vieworg, e->origin, modelOrigin);
	if (rotated)
	{
		vec3_t temp;
		vec3_t forward, right, up;
		VectorCopy(modelOrigin, temp);
		AngleVectors(e->angles, forward, right, up);
		modelOrigin[0] = DotProduct(temp, forward);
		modelOrigin[1] = -DotProduct(temp, right);
		modelOrigin[2] = DotProduct(temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

	// calculate dynamic lighting for bmodel if it's not an
	// instanced model
	if (clmodel->firstmodelsurface != 0 && !gl_flashblend.value)
	{
		for (int k = 0; k < MAX_DLIGHTS; k++)
		{
			if ((cl_dlights[k].die < cl.time) ||
			    (!cl_dlights[k].radius))
				continue;

			R_MarkLights(&cl_dlights[k], 1 << k, clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}

	oglwPushMatrix();
	e->angles[0] = -e->angles[0]; // stupid quake bug
	R_RotateForEntity(e);
	e->angles[0] = -e->angles[0]; // stupid quake bug

	//
	// draw texture
	//
	for (int i = 0; i < clmodel->nummodelsurfaces; i++, psurf++)
	{
		// find which side of the node we are on
		pplane = psurf->plane;

		dot = DotProduct(modelOrigin, pplane->normal) - pplane->dist;

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			if (gl_texsort.value)
				R_RenderBrushPoly(psurf, 1.0f);
			else
				R_DrawSequentialPoly(psurf);
		}
	}

	R_BlendLightmaps();

	oglwPopMatrix();
}

/*
   =============================================================

        WORLD MODEL

   =============================================================
 */

static void R_RecursiveWorldNode(mnode_t *node, vec3_t modelOrigin)
{
	int c, side;
	mplane_t *plane;
	msurface_t *surf, **mark;
	mleaf_t *pleaf;

	if (node->contents == CONTENTS_SOLID)
		return;                                // solid

	if (node->visframe != r_visframecount)
		return;

	if (R_CullBox(node->minmaxs, node->minmaxs + 3))
		return;

	// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		pleaf = (mleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

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
			R_StoreEfrags(&pleaf->efrags);

		return;
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	plane = node->plane;

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

	if (dot >= 0)
		side = 0;
	else
		side = 1;

	// recurse down the children, front side first
	R_RecursiveWorldNode(node->children[side], modelOrigin);

	// draw stuff
	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;
		if (dot < 0 - BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;
        for (; c; c--, surf++)
        {
            if (surf->visframe != r_framecount)
                continue;

            // don't backface underwater surfaces, because they warp
            if (!(surf->flags & SURF_UNDERWATER) && ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)))
                continue;                                                                                  // wrong side

            // if sorting by texture, just store it out
            if (gl_texsort.value)
            {
                if (!mirror || surf->texinfo->texture != cl.worldmodel->textures[mirrortexturenum])
                {
                    surf->texturechain = surf->texinfo->texture->texturechain;
                    surf->texinfo->texture->texturechain = surf;
                }
            }
            else if (surf->flags & SURF_DRAWSKY)
            {
                surf->texturechain = skychain;
                skychain = surf;
            }
            else if (surf->flags & SURF_DRAWTURB)
            {
                surf->texturechain = waterchain;
                waterchain = surf;
            }
            else
                R_DrawSequentialPoly(surf);
        }
	}

	// recurse down the back side
	R_RecursiveWorldNode(node->children[!side], modelOrigin);
}

void R_DrawWorld()
{
	entity_t ent;

	memset(&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;

	currententity = &ent;
	currenttexture = -1;

	memset(lightmap_polys, 0, sizeof(lightmap_polys));

	R_RecursiveWorldNode(cl.worldmodel->nodes, r_refdef.vieworg);

	DrawTextureChains();

	R_BlendLightmaps();
}

void R_MarkLeaves()
{
	byte *vis;
	mnode_t *node;
	int i;
	byte solid[4096];

	if (r_oldviewleaf == r_viewleaf && !r_novis.value)
		return;

	if (mirror)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis.value)
	{
		vis = solid;
		memset(solid, 0xff, (cl.worldmodel->numleafs + 7) >> 3);
	}
	else
		vis = Mod_LeafPVS(r_viewleaf, cl.worldmodel);

	for (i = 0; i < cl.worldmodel->numleafs; i++)
	{
		if (vis[i >> 3] & (1 << (i & 7)))
		{
			node = (mnode_t *)&cl.worldmodel->leafs[i + 1];
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

/*
   =============================================================================

   LIGHTMAP ALLOCATION

   =============================================================================
 */

// returns a texture number and the position inside it
static int AllocBlock(int w, int h, int *x, int *y)
{
	int i, j;
	int best, best2;
	int texnum;

	for (texnum = 0; texnum < MAX_LIGHTMAPS; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i = 0; i < BLOCK_WIDTH - w; i++)
		{
			best2 = 0;

			for (j = 0; j < w; j++)
			{
				if (allocated[texnum][i + j] >= best)
					break;
				if (allocated[texnum][i + j] > best2)
					best2 = allocated[texnum][i + j];
			}
			if (j == w) // this is a valid spot
			{
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i = 0; i < w; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error("AllocBlock: full");
	return -1;
}

static mvertex_t *r_pcurrentvertbase;
static model_t *currentmodel;

static int nColinElim;

static void BuildSurfaceDisplayList(msurface_t *fa)
{
	int i, lindex, lnumverts;
	medge_t *pedges, *r_pedge;
	float *vec;
	float s, t;
	glpoly_t *poly;

	// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;

	//
	// draw texture
	//
	poly = Hunk_Alloc(sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i = 0; i < lnumverts; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy(vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		//
		// lightmap texture coordinates
		//
		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= BLOCK_WIDTH * 16; //fa->texinfo->texture->width;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= BLOCK_HEIGHT * 16; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	//
	// remove co-linear points - Ed
	//
	if (!gl_keeptjunctions.value && !(fa->flags & SURF_UNDERWATER))
	{
		for (i = 0; i < lnumverts; ++i)
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
				int j;
				for (j = i + 1; j < lnumverts; ++j)
				{
					int k;
					for (k = 0; k < VERTEXSIZE; ++k)
						poly->verts[j - 1][k] = poly->verts[j][k];
				}
				--lnumverts;
				++nColinElim;
				// retry next vertex next time, which is now current vertex
				--i;
			}
		}
	}
	poly->numverts = lnumverts;
}

static void GL_CreateSurfaceLightmap(msurface_t *surf)
{
	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	int smax = (surf->extents[0] >> 4) + 1;
	int tmax = (surf->extents[1] >> 4) + 1;

	surf->lightmaptexturenum = AllocBlock(smax, tmax, &surf->light_s, &surf->light_t);
	byte *base = lightmaps + surf->lightmaptexturenum * 4 * BLOCK_WIDTH * BLOCK_HEIGHT;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * 4;
	R_BuildLightMap(surf, base, BLOCK_WIDTH * 4);
}

/*
   Builds the lightmap texture
   with all the surfaces from all brush models
 */
void GL_BuildLightmaps()
{
	memset(allocated, 0, sizeof(allocated));

	r_framecount = 1; // no dlightcache

	if (!lightmap_textures)
	{
		lightmap_textures = texture_extension_number;
		texture_extension_number += MAX_LIGHTMAPS;
	}

	for (int j = 1; j < MAX_MODELS; j++)
	{
		model_t *m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (int i = 0; i < m->numsurfaces; i++)
		{
			GL_CreateSurfaceLightmap(m->surfaces + i);
			if (m->surfaces[i].flags & SURF_DRAWTURB)
				continue;
			if (m->surfaces[i].flags & SURF_DRAWSKY)
				continue;
			BuildSurfaceDisplayList(m->surfaces + i);
		}
	}

	if (!gl_texsort.value)
		GL_SelectTexture(GL_TEXTURE1);

	//
	// upload all lightmaps that were filled
	//
	for (int i = 0; i < MAX_LIGHTMAPS; i++)
	{
		if (!allocated[i][0])
			break;                // no more used
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = BLOCK_WIDTH;
		lightmap_rectchange[i].t = BLOCK_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;
		GL_Bind(lightmap_textures + i);
		#if defined(EGLW_GLES1)
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		#endif
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, lightmaps + i * BLOCK_WIDTH * BLOCK_HEIGHT * 4);
		#if defined(EGLW_GLES1)
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
		#else
		glGenerateMipmap(GL_TEXTURE_2D);
		#endif
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	if (!gl_texsort.value)
		GL_SelectTexture(GL_TEXTURE0);
}

static void BoundPoly(int numverts, float *verts, vec3_t mins, vec3_t maxs)
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

static void SubdividePolygon(int numverts, float *verts)
{
	if (numverts > 60)
		Sys_Error("numverts = %i", numverts);

	vec3_t mins, maxs;
	BoundPoly(numverts, verts, mins, maxs);

	float subdivideSize = gl_subdivide_size.value;
	for (int i = 0; i < 3; i++)
	{
		float m = (mins[i] + maxs[i]) * 0.5f;
		m = subdivideSize * floorf(m / subdivideSize + 0.5f);
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

		SubdividePolygon(f, front[0]);
		SubdividePolygon(b, back[0]);
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
void GL_SubdivideSurface(msurface_t *fa)
{
	vec3_t verts[64];
	warpface = fa;

	//
	// convert edges back to a normal polygon
	//
	int numverts = 0;
	for (int i = 0; i < fa->numedges; i++)
	{
		int lindex = loadmodel->surfedges[fa->firstedge + i];

		float *vec;
		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy(vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon(numverts, verts[0]);
}

/*
   A sky texture is 256*128, with the right side being a masked overlay
 */
void R_InitSky(texture_t *mt)
{
	// make an average value for the back to avoid
	// a fringe on the top level
	byte *src = (byte *)mt + mt->offsets[0];
	unsigned *trans = malloc(128 * 128 * sizeof(unsigned));
	int r = 0, g = 0, b = 0;
	for (int i = 0; i < 128; i++)
		for (int j = 0; j < 128; j++)
		{
			int p = src[i * 256 + j + 128];
			unsigned *rgba = &d_8to24table[p];
			trans[(i * 128) + j] = *rgba;
			r += ((byte *)rgba)[0];
			g += ((byte *)rgba)[1];
			b += ((byte *)rgba)[2];
		}

	unsigned transpix;
	((byte *)&transpix)[0] = r / (128 * 128);
	((byte *)&transpix)[1] = g / (128 * 128);
	((byte *)&transpix)[2] = b / (128 * 128);
	((byte *)&transpix)[3] = 0;

	if (!solidskytexture)
		solidskytexture = texture_extension_number++;
	GL_Bind(solidskytexture);
	#if defined(EGLW_GLES1)
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	#endif
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	#if defined(EGLW_GLES1)
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	#else
	glGenerateMipmap(GL_TEXTURE_2D);
	#endif
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	for (int i = 0; i < 128; i++)
		for (int j = 0; j < 128; j++)
		{
			int p = src[i * 256 + j];
			if (p == 0)
				trans[(i * 128) + j] = transpix;
			else
				trans[(i * 128) + j] = d_8to24table[p];
		}

	if (!alphaskytexture)
		alphaskytexture = texture_extension_number++;
	GL_Bind(alphaskytexture);
	#if defined(EGLW_GLES1)
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	#endif
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	#if defined(EGLW_GLES1)
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	#else
	glGenerateMipmap(GL_TEXTURE_2D);
	#endif
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	free(trans);
}
