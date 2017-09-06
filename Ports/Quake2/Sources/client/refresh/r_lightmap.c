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
 * Lightmap handling
 *
 * =======================================================================
 */

#include "local.h"

extern gllightmapstate_t gl_lightmapState;

void R_SetCacheState(msurface_t *surf);
void R_BuildLightMap(msurface_t *surf, byte *dest, int stride);

static lightstyle_t lightstyles[MAX_LIGHTSTYLES];

void LM_InitBlock()
{
	memset(gl_lightmapState.allocated, 0, sizeof(gl_lightmapState.allocated));
}

void LM_UploadBlock(qboolean dynamic)
{
	oglwSetCurrentTextureUnitForced(0);

	if (dynamic)
	{
        // Dynamic textures are used in a round robin way, even inter frames. Otherwise, if a texture already in use for rendering is updated, it causes huge slowdowns with tile / deferred rendering platforms.
        int texture = gl_lightmapState.dynamicLightmapCurrent;
        gl_lightmapState.dynamicLightmapCurrent = (texture + 1) % LIGHTMAP_DYNAMIC_MAX_NB;
        oglwBindTextureForced(0, gl_state.lightmap_textures + LIGHTMAP_STATIC_MAX_NB + texture);

		int height = 0;
		for (int i = 0; i < LIGHTMAP_WIDTH; i++)
		{
            int allocated = gl_lightmapState.allocated[i];
			if (height < allocated)
				height = allocated;
		}
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, LIGHTMAP_WIDTH, height, GL_RGBA, GL_UNSIGNED_BYTE, gl_lightmapState.lightmap_buffer);
	}
	else
	{
        int texture = gl_lightmapState.staticLightmapNb;
		if (texture >= LIGHTMAP_STATIC_MAX_NB)
		{
			VID_Error(ERR_DROP, "LM_UploadBlock() - LIGHTMAP_STATIC_MAX_NB exceeded\n");
            return;
		}
        else
            gl_lightmapState.staticLightmapNb = texture + 1;
        oglwBindTextureForced(0, gl_state.lightmap_textures + texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, gl_lightmapState.lightmap_buffer);
	}
}

/*
 * returns a texture number and the position inside it
 */
qboolean LM_AllocBlock(int w, int h, short *x, short *y)
{
	int best = LIGHTMAP_HEIGHT;

	for (int i = 0; i < LIGHTMAP_WIDTH - w; i++)
	{
		int best2 = 0;
		int j;

		for (j = 0; j < w; j++)
		{
            int current = gl_lightmapState.allocated[i + j];
			if (current >= best)
				break;
			if (current > best2)
				best2 = current;
		}

		if (j == w)
		{
			/* this is a valid spot */
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > LIGHTMAP_HEIGHT)
		return false;

	for (int i = 0; i < w; i++)
		gl_lightmapState.allocated[*x + i] = best + h;

	return true;
}

void LM_BuildPolygonFromSurface(msurface_t *fa)
{
	int i, lindex, lnumverts;
	medge_t *pedges, *r_pedge;
	float *vec;
	float s, t;
	glpoly_t *poly;
	vec3_t total;

	/* reconstruct the polygon */
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;

	VectorClear(total);

	/* draw texture */
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
			vec = currentmodel->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = currentmodel->vertexes[r_pedge->v[1]].position;
		}

		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->image->width;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->image->height;

		VectorAdd(total, vec, total);
		VectorCopy(vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		/* lightmap texture coordinates */
		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= LIGHTMAP_WIDTH * 16; /* fa->texinfo->texture->width; */

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= LIGHTMAP_HEIGHT * 16; /* fa->texinfo->texture->height; */

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	poly->numverts = lnumverts;
}

void LM_CreateSurfaceLightmap(msurface_t *surf)
{
	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	int smax = (surf->extents[0] >> 4) + 1;
	int tmax = (surf->extents[1] >> 4) + 1;

	if (!LM_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
	{
		LM_UploadBlock(false);
		LM_InitBlock();
		if (!LM_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
			VID_Error(ERR_FATAL, "Consecutive calls to LM_AllocBlock(%d,%d) failed\n", smax, tmax);
	}

	surf->lightmaptexturenum = gl_lightmapState.staticLightmapNb;

	R_SetCacheState(surf);
	byte *base = gl_lightmapState.lightmap_buffer + (surf->light_t * LIGHTMAP_WIDTH + surf->light_s) * 4;
	R_BuildLightMap(surf, base, LIGHTMAP_WIDTH * 4);
}

void LM_BeginBuildingLightmaps(model_t *m)
{
	memset(gl_lightmapState.allocated, 0, sizeof(gl_lightmapState.allocated));

	r_framecount = 1; /* no dlightcache */

	/* setup the base lightstyles so the lightmaps
	   won't have to be regenerated the first time
	   they're seen */
	for (int i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		lightstyles[i].rgb[0] = 1;
		lightstyles[i].rgb[1] = 1;
		lightstyles[i].rgb[2] = 1;
		lightstyles[i].white = 3;
	}

	r_newrefdef.lightstyles = lightstyles;

	if (!gl_state.lightmap_textures)
		gl_state.lightmap_textures = TEXNUM_LIGHTMAPS;

	gl_lightmapState.staticLightmapNb = 0;
	gl_lightmapState.dynamicLightmapCurrent = 0;

	// Initialize the dynamic lightmap texture.
	oglwSetCurrentTextureUnitForced(0);
    for (int i = 0; i < LIGHTMAP_DYNAMIC_MAX_NB; i++)
    {
        oglwBindTextureForced(0, gl_state.lightmap_textures + LIGHTMAP_STATIC_MAX_NB + i);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }
}

void LM_EndBuildingLightmaps()
{
	LM_UploadBlock(false);
}
