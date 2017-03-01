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
 * Surface generation and drawing
 *
 * =======================================================================
 */

#include "local.h"

#include <assert.h>

int c_visible_lightmaps;
int c_visible_textures;
static vec3_t modelorg; /* relative to viewpoint */

static msurface_t *r_alpha_surfaces[2];
static image_t * usedTextureList; // List of textures used by world and current entity.

gllightmapstate_t gl_lms;

void LM_InitBlock();
void LM_UploadBlock(qboolean dynamic);
qboolean LM_AllocBlock(int w, int h, int *x, int *y);

void R_SetCacheState(msurface_t *surf);
void R_BuildLightMap(msurface_t *surf, byte *dest, int stride);

/*
 * Returns the proper texture for a given time and base texture
 */
static image_t* R_TextureAnimation(mtexinfo_t *tex)
{
	if (tex->next)
	{
		int c = currententity->frame % tex->numframes;
		while (c)
		{
			tex = tex->next;
			c--;
		}
	}
	return tex->image;
}

static void R_DrawPolys(msurface_t *s, float intensity, float alpha)
{
	float scroll = 0.0f;
	if (s->texinfo->flags & SURF_FLOWING)
	{
		float t = r_newrefdef.time * (1.0f / 40.0f);
		scroll = -64 * (t - (int)t);
		if (scroll == 0.0f)
		{
			scroll = -64.0f;
		}
	}

	glpoly_t *p = s->polys;
	int n = p->numverts;
	OpenGLWrapperVertex *vtx = oglwAllocateTriangleFan(n);
	float *v = p->verts[0];
	for (int i = 0; i < n; i++, v += VERTEXSIZE)
	{
		vtx = AddVertex3D_CT1(vtx, v[0], v[1], v[2], intensity, intensity, intensity, alpha, (v[3] + scroll), v[4]);
	}
}

static void R_DrawLightmappedPolys(msurface_t *surf, float intensity, float alpha)
{
	float scroll = 0.0f;
	if (surf->texinfo->flags & SURF_FLOWING)
	{
		float t = r_newrefdef.time * (1.0f / 40.0f);
		scroll = -64 * (t - (int)t);
		if (scroll == 0.0f)
		{
			scroll = -64.0f;
		}
	}

	for (glpoly_t *p = surf->polys; p; p = p->chain)
	{
		int nv = p->numverts;
		OpenGLWrapperVertex *vtx = oglwAllocateTriangleFan(nv);
		float *v = p->verts[0];
		for (int i = 0; i < nv; i++, v += VERTEXSIZE)
		{
			vtx = AddVertex3D_CT2(vtx, v[0], v[1], v[2], intensity, intensity, intensity, alpha, (v[3] + scroll), v[4], v[5], v[6]);
		}
	}
}

static void R_DrawTranslucentPolys(msurface_t *surf, float intensity, float alpha)
{
	if (surf->texinfo->flags & SURF_TRANS33)
	{
		alpha *= 0.33f;
	}
	else
	if (surf->texinfo->flags & SURF_TRANS66)
	{
		alpha *= 0.66f;
	}

	if (surf->flags & SURF_DRAWTURB)
	{
		R_DrawWaterPolys(surf, intensity, alpha);
	}
	else
	{
		R_DrawPolys(surf, intensity, alpha);
	}
}

static void R_SetupLightmap(msurface_t *surf, bool immediate)
{
	qboolean is_dynamic = false;

	// check for lightmap modification
	int map;
	for (map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++)
	{
		if (r_newrefdef.lightstyles[surf->styles[map]].white != surf->cached_light[map])
		{
			goto dynamic;
		}
	}

	// dynamic this frame or dynamic previously
	if (surf->dlightframe == r_framecount)
	{
dynamic:
		if (gl_dynamic->value)
		{
			if (!(surf->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)))
			{
				is_dynamic = true;
			}
		}
	}

	unsigned lmtex;
	if (is_dynamic)
	{
		int smax = (surf->extents[0] >> 4) + 1;
		int tmax = (surf->extents[1] >> 4) + 1;

		bool buildLightmap = false;
		if ((surf->styles[map] >= 32 || surf->styles[map] == 0) && (surf->dlightframe != r_framecount))
		{
			buildLightmap = true;
			R_SetCacheState(surf);

			lmtex = surf->lightmaptexturenum;
		}
		else
		{
			lmtex = 0;
		}

		byte *base = gl_lms.lightmap_buffer;
		if (immediate || buildLightmap)
		{
			R_BuildLightMap(surf, (void *)base, smax * 4);
		}
		if (immediate)
		{
			oglwSetCurrentTextureUnitForced(1);
			oglwBindTextureForced(1, gl_state.lightmap_textures + lmtex);
			glTexSubImage2D(GL_TEXTURE_2D, 0, surf->light_s, surf->light_t, smax, tmax, GL_LIGHTMAP_FORMAT, GL_UNSIGNED_BYTE, base);
			// oglwSetCurrentTextureUnitForced(0);
		}
	}
	else
	{
		lmtex = surf->lightmaptexturenum;
		if (immediate)
		{
			oglwBindTexture(1, gl_state.lightmap_textures + lmtex);
		}
	}
	if (!immediate)
	{
		surf->lightmapchain = gl_lms.lightmap_surfaces[lmtex];
		gl_lms.lightmap_surfaces[lmtex] = surf;
	}
}

static void R_DrawBegin(qboolean multitexturing)
{
	usedTextureList = NULL;
	memset(gl_lms.lightmap_surfaces, 0, sizeof(gl_lms.lightmap_surfaces));

	oglwSetTextureBlending(0, GL_MODULATE);

	if (multitexturing && !gl_fullbright->value)
	{
		oglwEnableTexturing(1, GL_TRUE);
		if (gl_lightmap->value)
		{
			oglwSetTextureBlending(1, GL_REPLACE);
		}
		else
		{
			oglwSetTextureBlending(1, GL_MODULATE);
		}
	}
}

static void R_DrawEnd()
{
	oglwSetTextureBlending(0, GL_REPLACE);

	oglwEnableTexturing(1, GL_FALSE);
	oglwSetTextureBlending(1, GL_REPLACE);
}

//----------------------------------------
// Surface rendering without multitexturing.
//----------------------------------------
static void R_ChainSurface(msurface_t *surf)
{
	image_t *image = R_TextureAnimation(surf->texinfo);
	if (!image->used)
	{
		image->used = 1;
		image->image_chain_node = usedTextureList;
		usedTextureList = image;
	}
	surf->texturechain = image->texturechain;
	image->texturechain = surf;
}

static void R_DrawTextureChains(float alpha, int chain)
{
	oglwSetTextureBlending(0, GL_MODULATE);

	if (chain == 0)
		c_visible_textures = 0;
	for (image_t *image = usedTextureList; image != NULL; )
	{
		msurface_t *s = image->texturechain;
		if (s)
		{
			if (chain == 0)
				c_visible_textures++;

			oglwBindTexture(0, image->texnum);
			oglwBegin(GL_TRIANGLES);
			for (; s; s = s->texturechain)
			{
				if (!(s->flags & SURF_DRAWTURB))
				{
					c_brush_polys++;
					R_DrawPolys(s, 1.0f, alpha);
				}
			}
			oglwEnd();
		}
		image_t *imageNext = image->image_chain_node;
		image = imageNext;
	}

	// Water polys.
	float intensity = gl_state.inverse_intensity; // the textures are prescaled up for a better lighting range, so scale it back down
	for (image_t *image = usedTextureList; image != NULL; )
	{
		msurface_t *s = image->texturechain;
		if (s)
		{
			oglwBindTexture(0, image->texnum);
			oglwBegin(GL_TRIANGLES);
			for (; s; s = s->texturechain)
			{
				if (s->flags & SURF_DRAWTURB)
				{
					c_brush_polys++;
					R_DrawWaterPolys(s, intensity, alpha);
				}
			}
			oglwEnd();
		}
		image_t *imageNext = image->image_chain_node;
		image->image_chain_node = NULL;
		image->texturechain = NULL;
		image->used = 0;
		image = imageNext;
	}

	usedTextureList = NULL;

	oglwSetTextureBlending(0, GL_REPLACE);
}

static void R_DrawStaticLightmaps(float alpha)
{
	for (int i = 1; i < MAX_LIGHTMAPS; i++)
	{
		if (gl_lms.lightmap_surfaces[i])
		{
			if (currentmodel == r_worldmodel)
			{
				c_visible_lightmaps++;
			}

			oglwBindTexture(0, gl_state.lightmap_textures + i);
			oglwBegin(GL_TRIANGLES);
			for (msurface_t *surf = gl_lms.lightmap_surfaces[i]; surf != 0; surf = surf->lightmapchain)
			{
				glpoly_t *p = surf->polys;
				if (p)
				{
					for (; p != 0; p = p->chain)
					{
						float *v = p->verts[0];
						if (v == NULL)
						{
							break;
						}
						OpenGLWrapperVertex *vtx = oglwAllocateTriangleFan(p->numverts);
						for (int j = 0; j < p->numverts; j++, v += VERTEXSIZE)
						{
							vtx = AddVertex3D_CT1(vtx, v[0], v[1], v[2], 1.0f, 1.0f, 1.0f, alpha, v[5], v[6]);
						}
					}
				}
			}
			oglwEnd();
		}
	}
}

static void R_DrawDynamicLightmaps(float alpha)
{
	msurface_t *batchFirstSurf = gl_lms.lightmap_surfaces[0];
	if (batchFirstSurf == NULL)
		return;

	oglwSetCurrentTextureUnitForced(0);
	oglwBindTextureForced(0, gl_state.lightmap_textures + 0);

	if (currentmodel == r_worldmodel)
	{
		c_visible_lightmaps++;
	}

	while (batchFirstSurf != NULL)
	{
		// Clear the current lightmap.
		LM_InitBlock();

		// Add as many blocks as possible in the current lightmap.
		msurface_t *surf;
		for (surf = batchFirstSurf; surf != 0; surf = surf->lightmapchain)
		{
			// Find a place in the current lightmap.
			int smax = (surf->extents[0] >> 4) + 1;
			int tmax = (surf->extents[1] >> 4) + 1;
			if (!LM_AllocBlock(smax, tmax, &surf->dlight_s, &surf->dlight_t))
			{
				// The block does not fit in the current lightmap. Draw all surfaces already done and try again with an empty lightmap.
				break;
			}

			// Build the light map for this surface.
			byte *base = gl_lms.lightmap_buffer + (surf->dlight_t * BLOCK_WIDTH + surf->dlight_s) * LIGHTMAP_BYTES;
			R_BuildLightMap(surf, base, BLOCK_WIDTH * LIGHTMAP_BYTES);
		}

		if (batchFirstSurf == surf)
		{
			// There is not enough room in the lightmap for even a single block.
			break;
		}

		// Load the current lightmap in texture memory.
		LM_UploadBlock(true);

		// Draw the surfaces for this lightmap.
		oglwBegin(GL_TRIANGLES);
		for (msurface_t *drawsurf = batchFirstSurf; drawsurf != surf; drawsurf = drawsurf->lightmapchain)
		{
			glpoly_t *p = drawsurf->polys;
			if (p)
			{
				float soffset = (drawsurf->light_s - drawsurf->dlight_s) * (1.0f / 128.0f);
				float toffset = (drawsurf->light_t - drawsurf->dlight_t) * (1.0f / 128.0f);
				for (; p != 0; p = p->chain)
				{
					float *v = p->verts[0];
					if (v == NULL)
					{
						break;
					}
					int n = p->numverts;
					OpenGLWrapperVertex *vtx = oglwAllocateTriangleFan(n);
					for (int j = 0; j < n; j++, v += VERTEXSIZE)
					{
						vtx = AddVertex3D_CT1(vtx, v[0], v[1], v[2], 1.0f, 1.0f, 1.0f, alpha, v[5] - soffset, v[6] - toffset);
					}
				}
			}
		}
		oglwEnd();

		// Try again with the last surface that did not fit.
		batchFirstSurf = surf;
	}
}

/*
 * This routine takes all the given light mapped surfaces
 * in the world and blends them into the framebuffer.
 */
static void R_DrawLightmaps(float alpha)
{
	/* don't bother if we're set to fullbright */
	if (gl_fullbright->value)
	{
		return;
	}

	if (!r_worldmodel->lightdata)
	{
		return;
	}

	/* set the appropriate blending mode unless
	   we're only looking at the lightmaps. */
	if (!gl_lightmap->value)
	{
		oglwEnableBlending(true);
		if (gl_saturatelighting->value)
			oglwSetBlendingFunction(GL_ONE, GL_ONE);
		else
			oglwSetBlendingFunction(GL_ZERO, GL_SRC_COLOR);
		oglwEnableDepthWrite(false);
	}

	if (currentmodel == r_worldmodel)
	{
		c_visible_lightmaps = 0;
	}

	R_DrawStaticLightmaps(alpha); // render static lightmaps first
	if (gl_dynamic->value)
		R_DrawDynamicLightmaps(alpha); // render dynamic lightmaps

	oglwEnableBlending(false);
	oglwSetBlendingFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	oglwEnableDepthWrite(true);
}

static void R_DrawTriangleOutlines()
{
	if (!gl_showtris->value)
	{
		return;
	}

	oglwEnableTexturing(0, GL_FALSE);
	oglwEnableDepthTest(false);

	oglwBegin(GL_LINES);
	for (int i = 0; i < MAX_LIGHTMAPS; i++)
	{
		for (msurface_t *surf = gl_lms.lightmap_surfaces[i]; surf != 0; surf = surf->lightmapchain)
		{
			for (glpoly_t *p = surf->polys; p; p = p->chain)
			{
				for (int j = 2; j < p->numverts; j++)
				{
					OpenGLWrapperVertex *vtx = oglwAllocateLineLoop(3);
					float *vp;
					vp = p->verts[0];
					vtx = AddVertex3D(vtx, vp[0], vp[1], vp[2]);
					vp = p->verts[j - 1];
					vtx = AddVertex3D(vtx, vp[0], vp[1], vp[2]);
					vp = p->verts[j];
					vtx = AddVertex3D(vtx, vp[0], vp[1], vp[2]);
				}
			}
		}
	}
	oglwEnd();

	oglwEnableDepthTest(true);
	oglwEnableTexturing(0, GL_TRUE);
}

//----------------------------------------
// Alpha surfaces.
//----------------------------------------
static void R_ChainAlphaSurface(msurface_t *surf, int chain)
{
	surf->texturechain = r_alpha_surfaces[chain];
	r_alpha_surfaces[chain] = surf;
	image_t *image = R_TextureAnimation(surf->texinfo);
	surf->current_image = image;
}

/*
 * Draw water surfaces and windows.
 * The BSP tree is walked front to back, so unwinding the chain
 * of alpha_surfaces will draw back to front, giving proper ordering.
 */
void R_DrawAlphaSurfaces(float alpha, int chain)
{
	oglwEnableBlending(true);
	oglwEnableDepthWrite(false);

	oglwSetTextureBlending(0, GL_MODULATE);

	GLuint currentTexture = 0;
	float intensity = gl_state.inverse_intensity;
	for (msurface_t *surf = r_alpha_surfaces[chain]; surf; surf = surf->texturechain)
	{
		c_brush_polys++;

		GLuint texture = surf->current_image->texnum;
		if (currentTexture != texture)
		{
			currentTexture = texture;
			oglwEnd(); // We can do this even if there was no previous oglwBegin().
			oglwBindTexture(0, texture);
			oglwBegin(GL_TRIANGLES);
		}
		R_DrawTranslucentPolys(surf, intensity, alpha);
	}
	oglwEnd();

	oglwSetTextureBlending(0, GL_REPLACE);

	oglwEnableBlending(false);
	oglwEnableDepthWrite(true);

	r_alpha_surfaces[chain] = NULL;
}

//----------------------------------------
// Surface rendering.
//----------------------------------------
static void R_RenderLightmappedPolys(msurface_t *surf, qboolean immediate, float alpha)
{
	R_SetupLightmap(surf, immediate);

	if (immediate)
	{
		c_brush_polys++;

		image_t *image = R_TextureAnimation(surf->texinfo);
		oglwBindTexture(0, image->texnum);
		oglwBegin(GL_TRIANGLES);
		R_DrawLightmappedPolys(surf, 1.0f, alpha);
		oglwEnd();
	}
	else
	{
		R_ChainSurface(surf);
	}
}

static void R_RenderTranslucentPolys(msurface_t *surf, qboolean immediate, float alpha, int chain)
{
	if (immediate)
	{
		c_brush_polys++;

		oglwEnableBlending(true);
		oglwEnableDepthWrite(false);

		oglwEnableTexturing(1, GL_FALSE);

		image_t *image = R_TextureAnimation(surf->texinfo);
		oglwBindTexture(0, image->texnum);
		oglwBegin(GL_TRIANGLES);
		R_DrawTranslucentPolys(surf, gl_state.inverse_intensity, alpha);
		oglwEnd();

		oglwEnableTexturing(1, GL_TRUE);

		oglwEnableBlending(false);
		oglwEnableDepthWrite(true);
	}
	else
	{
		R_ChainAlphaSurface(surf, chain);
	}
}

static void R_RenderWaterPolys(msurface_t *surf, qboolean immediate, float alpha)
{
	if (immediate)
	{
		c_brush_polys++;

		oglwEnableTexturing(1, GL_FALSE);

		image_t *image = R_TextureAnimation(surf->texinfo);
		oglwBindTexture(0, image->texnum);
		oglwBegin(GL_TRIANGLES);
		R_DrawWaterPolys(surf, gl_state.inverse_intensity, alpha);
		oglwEnd();

		oglwEnableTexturing(1, GL_TRUE);
	}
	else
	{
		R_ChainSurface(surf);
	}
}

static void R_RenderPolys(msurface_t *surf, qboolean immediate, float alpha, int chain)
{
	if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
	{
		R_RenderTranslucentPolys(surf, false, alpha, chain);
	}
	else
	if (!(surf->flags & SURF_DRAWTURB))
	{
		R_RenderLightmappedPolys(surf, immediate, alpha);
	}
	else
	{
		R_RenderWaterPolys(surf, immediate, alpha);
	}
}

//----------------------------------------
// Brush model.
//----------------------------------------
static void R_DrawInlineBModel(float alpha)
{
	/* calculate dynamic lighting for bmodel */
	if (!gl_flashblend->value)
	{
		dlight_t *lt = r_newrefdef.dlights;
		for (int k = 0; k < r_newrefdef.num_dlights; k++, lt++)
		{
			R_MarkLights(lt, 1 << k, currentmodel->nodes + currentmodel->firstnode);
		}
	}

	qboolean multitexturing = gl_multitexturing->value != 0;

	/* draw texture */
	msurface_t *psurf = &currentmodel->surfaces[currentmodel->firstmodelsurface];
	for (int i = 0; i < currentmodel->nummodelsurfaces; i++, psurf++)
	{
		/* find which side of the node we are on */
		cplane_t *pplane = psurf->plane;
		float dot = DotProduct(modelorg, pplane->normal) - pplane->dist;

		/* draw the polygon */
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
		    (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			R_RenderPolys(psurf, multitexturing, alpha, 1);
		}
	}
}

void R_DrawBrushModel(entity_t *e)
{
	if (currentmodel->nummodelsurfaces == 0)
	{
		return;
	}

	currententity = e;

	qboolean rotated;
	vec3_t mins, maxs;
	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;
		for (int i = 0; i < 3; i++)
		{
			mins[i] = e->origin[i] - currentmodel->radius;
			maxs[i] = e->origin[i] + currentmodel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd(e->origin, currentmodel->mins, mins);
		VectorAdd(e->origin, currentmodel->maxs, maxs);
	}

	if (R_CullBox(mins, maxs))
	{
		return;
	}

	if (gl_zfix->value)
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
	}

	VectorSubtract(r_newrefdef.vieworg, e->origin, modelorg);

	if (rotated)
	{
		vec3_t temp;
		vec3_t forward, right, up;

		VectorCopy(modelorg, temp);
		AngleVectors(e->angles, forward, right, up);
		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}

	oglwPushMatrix();
	e->angles[0] = -e->angles[0];
	e->angles[2] = -e->angles[2];
	R_RotateForEntity(e);
	e->angles[0] = -e->angles[0];
	e->angles[2] = -e->angles[2];

	float alpha = 1.0f;
	if (currententity->flags & RF_TRANSLUCENT)
	{
		alpha = currententity->alpha;
	}

	R_DrawBegin(gl_multitexturing->value != 0);
	R_DrawInlineBModel(alpha);
	R_DrawEnd();
	R_DrawTextureChains(alpha, 1);
	R_DrawLightmaps(alpha);
	R_DrawAlphaSurfaces(alpha, 1);

	oglwPopMatrix();

	if (gl_zfix->value)
	{
		glDisable(GL_POLYGON_OFFSET_FILL);
	}
}

//----------------------------------------
// World.
//----------------------------------------
/*
 * Mark the leaves and nodes that are
 * in the PVS for the current cluster
 */
void R_MarkLeaves()
{
	byte *vis;
	byte fatvis[MAX_MAP_LEAFS / 8];
	mnode_t *node;
	int i, c;
	mleaf_t *leaf;
	int cluster;

	if ((r_oldviewcluster == r_viewcluster) &&
	    (r_oldviewcluster2 == r_viewcluster2) &&
	    !gl_novis->value &&
	    (r_viewcluster != -1))
	{
		return;
	}

	/* development aid to let you run around
	   and see exactly where the pvs ends */
	if (gl_lockpvs->value)
	{
		return;
	}

	r_visframecount++;
	r_oldviewcluster = r_viewcluster;
	r_oldviewcluster2 = r_viewcluster2;

	if (gl_novis->value || (r_viewcluster == -1) || !r_worldmodel->vis)
	{
		/* mark everything */
		for (i = 0; i < r_worldmodel->numleafs; i++)
		{
			r_worldmodel->leafs[i].visframe = r_visframecount;
		}

		for (i = 0; i < r_worldmodel->numnodes; i++)
		{
			r_worldmodel->nodes[i].visframe = r_visframecount;
		}

		return;
	}

	vis = Mod_ClusterPVS(r_viewcluster, r_worldmodel);

	/* may have to combine two clusters because of solid water boundaries */
	if (r_viewcluster2 != r_viewcluster)
	{
		memcpy(fatvis, vis, (r_worldmodel->numleafs + 7) / 8);
		vis = Mod_ClusterPVS(r_viewcluster2, r_worldmodel);
		c = (r_worldmodel->numleafs + 31) / 32;

		for (i = 0; i < c; i++)
		{
			((int *)fatvis)[i] |= ((int *)vis)[i];
		}

		vis = fatvis;
	}

	for (i = 0, leaf = r_worldmodel->leafs;
	     i < r_worldmodel->numleafs;
	     i++, leaf++)
	{
		cluster = leaf->cluster;

		if (cluster == -1)
		{
			continue;
		}

		if (vis[cluster >> 3] & (1 << (cluster & 7)))
		{
			node = (mnode_t *)leaf;

			do
			{
				if (node->visframe == r_visframecount)
				{
					break;
				}

				node->visframe = r_visframecount;
				node = node->parent;
			}
			while (node);
		}
	}
}

static void R_RecursiveWorldNode(mnode_t *node);

void R_DrawWorld()
{
	if (!gl_drawworld->value)
	{
		return;
	}

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		return;
	}

	currentmodel = r_worldmodel;

	VectorCopy(r_newrefdef.vieworg, modelorg);

	/* auto cycle the world frame for texture animation */
	entity_t ent;
	memset(&ent, 0, sizeof(ent));
	ent.frame = (int)(r_newrefdef.time * 2);
	currententity = &ent;

	R_ClearSkyBox();

	R_DrawBegin(gl_multitexturing->value != 0);
	R_RecursiveWorldNode(r_worldmodel->nodes);
	R_DrawEnd();
	R_DrawTextureChains(1.0f, 0);
	R_DrawLightmaps(1.0f);

	R_DrawSkyBox();
	R_DrawTriangleOutlines();

	currententity = NULL;
}

static void R_RecursiveWorldNode(mnode_t *node)
{
	if (node->contents == CONTENTS_SOLID)
	{
		return; /* solid */
	}

	if (node->visframe != r_visframecount)
	{
		return;
	}

	if (R_CullBox(node->minmaxs, node->minmaxs + 3))
	{
		return;
	}

	/* if a leaf node, draw stuff */
	if (node->contents != -1)
	{
		mleaf_t *pleaf = (mleaf_t *)node;

		/* check for door connected areas */
		if (r_newrefdef.areabits)
		{
			if (!(r_newrefdef.areabits[pleaf->area >> 3] & (1 << (pleaf->area & 7))))
			{
				return; /* not visible */
			}
		}

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

		return;
	}

	/* node is just a decision point, so go down the apropriate
	   sides find which side of the node we are on */
	cplane_t *plane = node->plane;
	float dot;
	switch (plane->type)
	{
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct(modelorg, plane->normal) - plane->dist;
		break;
	}
	int side, sidebit;
	if (dot >= 0)
	{
		side = 0;
		sidebit = 0;
	}
	else
	{
		side = 1;
		sidebit = SURF_PLANEBACK;
	}

	/* recurse down the children, front side first */
	R_RecursiveWorldNode(node->children[side]);

	/* draw stuff */
	qboolean multitexturing = gl_multitexturing->value != 0;

	{
		int c;
		msurface_t *surf;
		for (c = node->numsurfaces, surf = r_worldmodel->surfaces + node->firstsurface; c; c--, surf++)
		{
			if (surf->visframe != r_framecount)
			{
				continue;
			}

			if ((surf->flags & SURF_PLANEBACK) != sidebit)
			{
				continue; /* wrong side */
			}

			if (surf->texinfo->flags & SURF_SKY)
			{
				/* just adds to visible sky bounds */
				R_AddSkySurface(surf);
			}
			else
			{
				R_RenderPolys(surf, multitexturing, 1.0f, 0);
			}
		}
	}

	/* recurse down the back side */
	R_RecursiveWorldNode(node->children[!side]);
}
