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
 * Lightmaps and dynamic lighting
 *
 * =======================================================================
 */

#include "local.h"

#define DLIGHT_CUTOFF 64

int r_dlightframecount;
vec3_t pointcolor;
cplane_t *lightplane; /* used as shadow plane */
vec3_t lightspot;
static float s_blocklights[34 * 34 * 3];

static void R_RenderDlight(dlight_t *light)
{
	OpenGLWrapperVertex *vtx = oglwAllocateTriangleFan(1 + 16 + 1);

	vec3_t v;

	float rad = light->intensity * 0.35f;
	for (int i = 0; i < 3; i++)
	{
		v[i] = light->origin[i] - vpn[i] * rad;
	}
	vtx = AddVertex3D_C(vtx, v[0], v[1], v[2], light->color[0] * 0.2f, light->color[1] * 0.2f, light->color[2] * 0.2f, 1.0f);

	for (int i = 16; i >= 0; i--)
	{
		float a = i / 16.0f * Q_PI * 2;
		for (int j = 0; j < 3; j++)
		{
			v[j] = light->origin[j] + vright[j] * cosf(a) * rad
			        + vup[j] * sinf(a) * rad;
		}
		vtx = AddVertex3D_C(vtx, v[0], v[1], v[2], 0.0f, 0.0f, 0.0f, 1.0f);
	}
}

void R_RenderDlights()
{
	if (!gl_flashblend->value)
	{
		return;
	}

	/* because the count hasn't advanced yet for this frame */
	r_dlightframecount = r_framecount + 1;

	oglwEnableBlending(true);
	oglwSetBlendingFunction(GL_ONE, GL_ONE);
	oglwEnableDepthWrite(false);

	oglwEnableSmoothShading(true);
	oglwEnableTexturing(0, GL_FALSE);

	oglwBegin(GL_TRIANGLES);
	dlight_t *l = r_newrefdef.dlights;
	for (int i = 0; i < r_newrefdef.num_dlights; i++, l++)
	{
		R_RenderDlight(l);
	}
	oglwEnd();

	oglwEnableSmoothShading(false);
	oglwEnableTexturing(0, GL_TRUE);

	oglwEnableBlending(false);
	oglwSetBlendingFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	oglwEnableDepthWrite(true);
}

void R_MarkLights(dlight_t *light, int bit, mnode_t *node)
{
	if (node->contents != -1)
	{
		return;
	}

	cplane_t *splitplane = node->plane;
	float dist = DotProduct(light->origin, splitplane->normal) - splitplane->dist;

	if (dist > light->intensity - DLIGHT_CUTOFF)
	{
		R_MarkLights(light, bit, node->children[0]);
		return;
	}

	if (dist < -light->intensity + DLIGHT_CUTOFF)
	{
		R_MarkLights(light, bit, node->children[1]);
		return;
	}

	/* mark the polygons */
	msurface_t *surf = r_worldmodel->surfaces + node->firstsurface;

	for (int i = 0; i < node->numsurfaces; i++, surf++)
	{
		dist = DotProduct(light->origin, surf->plane->normal) - surf->plane->dist;

		int sidebit;
		if (dist >= 0)
		{
			sidebit = 0;
		}
		else
		{
			sidebit = SURF_PLANEBACK;
		}

		if ((surf->flags & SURF_PLANEBACK) != sidebit)
		{
			continue;
		}

		if (surf->dlightframe != r_dlightframecount)
		{
			surf->dlightbits = 0;
			surf->dlightframe = r_dlightframecount;
		}

		surf->dlightbits |= bit;
	}

	R_MarkLights(light, bit, node->children[0]);
	R_MarkLights(light, bit, node->children[1]);
}

void R_PushDlights(void)
{
	if (gl_flashblend->value)
	{
		return;
	}

	/* because the count hasn't advanced yet for this frame */
	r_dlightframecount = r_framecount + 1;

	dlight_t *l = r_newrefdef.dlights;

	for (int i = 0; i < r_newrefdef.num_dlights; i++, l++)
	{
		R_MarkLights(l, 1 << i, r_worldmodel->nodes);
	}
}

int R_RecursiveLightPoint(mnode_t *node, vec3_t start, vec3_t end)
{
	if (node->contents != -1)
	{
		return -1; /* didn't hit anything */
	}

	/* calculate mid point */
	cplane_t *plane = node->plane;
	float front = DotProduct(start, plane->normal) - plane->dist;
	float back = DotProduct(end, plane->normal) - plane->dist;
	int side = front < 0;

	if ((back < 0) == side)
	{
		return R_RecursiveLightPoint(node->children[side], start, end);
	}

	float frac = front / (front - back);
	vec3_t mid;
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

	/* go down front side */
	int r = R_RecursiveLightPoint(node->children[side], start, mid);

	if (r >= 0)
	{
		return r; /* hit something */
	}

	if ((back < 0) == side)
	{
		return -1; /* didn't hit anuthing */
	}

	/* check for impact on this node */
	VectorCopy(mid, lightspot);
	lightplane = plane;

	msurface_t *surf = r_worldmodel->surfaces + node->firstsurface;

	for (int i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
		{
			continue; /* no lightmaps */
		}

		mtexinfo_t *tex = surf->texinfo;

		int s = DotProduct(mid, tex->vecs[0]) + tex->vecs[0][3];
		int t = DotProduct(mid, tex->vecs[1]) + tex->vecs[1][3];

		if ((s < surf->texturemins[0]) ||
		    (t < surf->texturemins[1]))
		{
			continue;
		}

		int ds = s - surf->texturemins[0];
		int dt = t - surf->texturemins[1];

		if ((ds > surf->extents[0]) || (dt > surf->extents[1]))
		{
			continue;
		}

		if (!surf->samples)
		{
			return 0;
		}

		ds >>= 4;
		dt >>= 4;

		VectorCopy(vec3_origin, pointcolor);

		byte *lightmap = surf->samples;
		if (lightmap)
		{
			lightmap += 3 * (dt * ((surf->extents[0] >> 4) + 1) + ds);

			float modulate = gl_modulate->value * (1.0f / 255);
			for (int maps = 0; maps < MAXLIGHTMAPS; maps++)
			{
				int style = surf->styles[maps];
				if (style == 255)
					break;
				float *styleRgb = r_newrefdef.lightstyles[style].rgb;
				for (i = 0; i < 3; i++)
				{
					pointcolor[i] += lightmap[i] * modulate * styleRgb[i];
				}
				lightmap += 3 * ((surf->extents[0] >> 4) + 1) *
				        ((surf->extents[1] >> 4) + 1);
			}
		}

		return 1;
	}

	/* go down back side */
	return R_RecursiveLightPoint(node->children[!side], mid, end);
}

void R_LightPoint(vec3_t p, vec3_t color)
{
	vec3_t end;
	float r;
	int lnum;
	dlight_t *dl;
	vec3_t dist;
	float add;

	if (!r_worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 1.0f;
		return;
	}

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	r = R_RecursiveLightPoint(r_worldmodel->nodes, p, end);

	if (r == -1)
	{
		VectorCopy(vec3_origin, color);
	}
	else
	{
		VectorCopy(pointcolor, color);
	}

	/* add dynamic lights */
	dl = r_newrefdef.dlights;

	for (lnum = 0; lnum < r_newrefdef.num_dlights; lnum++, dl++)
	{
		VectorSubtract(currententity->origin,
			dl->origin, dist);
		add = dl->intensity - VectorLength(dist);
		add *= (1.0f / 256);

		if (add > 0)
		{
			VectorMA(color, add, dl->color, color);
		}
	}

	VectorScale(color, gl_modulate->value, color);
}

void R_AddDynamicLights(msurface_t *surf)
{
	int smax = (surf->extents[0] >> 4) + 1;
	int tmax = (surf->extents[1] >> 4) + 1;
	mtexinfo_t *tex = surf->texinfo;

	for (int lnum = 0; lnum < r_newrefdef.num_dlights; lnum++)
	{
		if (!(surf->dlightbits & (1 << lnum)))
		{
			continue; /* not lit by this light */
		}

		dlight_t *dl = &r_newrefdef.dlights[lnum];
		float fdist = DotProduct(dl->origin, surf->plane->normal) - surf->plane->dist;
		float frad = dl->intensity - fabsf(fdist);

		/* rad is now the highest intensity on the plane */
		float fminlight = DLIGHT_CUTOFF;
		if (frad < fminlight)
		{
			continue;
		}

		fminlight = frad - fminlight;

		vec3_t impact;
		for (int i = 0; i < 3; i++)
		{
			impact[i] = dl->origin[i] - surf->plane->normal[i] * fdist;
		}

		vec3_t local;
		local[0] = DotProduct(impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0];
		local[1] = DotProduct(impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1];

		float *pfBL = s_blocklights;

		float fsacc, ftacc;
		int s, t;
		for (t = 0, ftacc = 0.0f; t < tmax; t++, ftacc += 16)
		{
			int td = local[1] - ftacc;
			if (td < 0)
			{
				td = -td;
			}

			for (s = 0, fsacc = 0.0f; s < smax; s++, fsacc += 16, pfBL += 3)
			{
				int sd = Q_ftol(local[0] - fsacc);
				if (sd < 0)
				{
					sd = -sd;
				}
				if (sd > td)
				{
					fdist = sd + (td >> 1);
				}
				else
				{
					fdist = td + (sd >> 1);
				}
				if (fdist < fminlight)
				{
					pfBL[0] += (frad - fdist) * dl->color[0];
					pfBL[1] += (frad - fdist) * dl->color[1];
					pfBL[2] += (frad - fdist) * dl->color[2];
				}
			}
		}
	}
}

void R_SetCacheState(msurface_t *surf)
{
	for (int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
	{
		surf->cached_light[maps] = r_newrefdef.lightstyles[surf->styles[maps]].white;
	}
}

/*
 * Combine and scale multiple lightmaps into the floating format in blocklights
 */
void R_BuildLightMap(msurface_t *surf, byte *dest, int stride)
{
	if (surf->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP))
	{
		VID_Error(ERR_DROP, "R_BuildLightMap called for non-lit surface");
	}

	int smax = (surf->extents[0] >> 4) + 1;
	int tmax = (surf->extents[1] >> 4) + 1;
	int size = smax * tmax;
	if (size > (int)(sizeof(s_blocklights) >> 4))
	{
		VID_Error(ERR_DROP, "Bad s_blocklights size");
	}

	/* set to full bright if no light data */
	if (!surf->samples)
	{
		for (int i = 0; i < size * 3; i++)
		{
			s_blocklights[i] = 255;
		}
		goto store;
	}

	{
		/* count the # of maps */
		int nummaps;
		for (nummaps = 0; nummaps < MAXLIGHTMAPS && surf->styles[nummaps] != 255;
		     nummaps++)
		{
		}

		byte *lightmap = surf->samples;
		float scale[4];

		/* add all the lightmaps */
		if (nummaps == 1)
		{
			for (int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
			{
				float *bl = s_blocklights;

				for (int i = 0; i < 3; i++)
				{
					scale[i] = gl_modulate->value *
					        r_newrefdef.lightstyles[surf->styles[maps]].rgb[i];
				}

				if ((scale[0] == 1.0F) &&
				    (scale[1] == 1.0F) &&
				    (scale[2] == 1.0F))
				{
					for (int i = 0; i < size; i++, bl += 3)
					{
						bl[0] = lightmap[i * 3 + 0];
						bl[1] = lightmap[i * 3 + 1];
						bl[2] = lightmap[i * 3 + 2];
					}
				}
				else
				{
					for (int i = 0; i < size; i++, bl += 3)
					{
						bl[0] = lightmap[i * 3 + 0] * scale[0];
						bl[1] = lightmap[i * 3 + 1] * scale[1];
						bl[2] = lightmap[i * 3 + 2] * scale[2];
					}
				}

				lightmap += size * 3; /* skip to next lightmap */
			}
		}
		else
		{
			memset(s_blocklights, 0, sizeof(s_blocklights[0]) * size * 3);
			for (int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
			{
				for (int i = 0; i < 3; i++)
				{
					scale[i] = gl_modulate->value *
					        r_newrefdef.lightstyles[surf->styles[maps]].rgb[i];
				}

				float *bl = s_blocklights;
				if ((scale[0] == 1.0F) &&
				    (scale[1] == 1.0F) &&
				    (scale[2] == 1.0F))
				{
					for (int i = 0; i < size; i++, bl += 3)
					{
						bl[0] += lightmap[i * 3 + 0];
						bl[1] += lightmap[i * 3 + 1];
						bl[2] += lightmap[i * 3 + 2];
					}
				}
				else
				{
					for (int i = 0; i < size; i++, bl += 3)
					{
						bl[0] += lightmap[i * 3 + 0] * scale[0];
						bl[1] += lightmap[i * 3 + 1] * scale[1];
						bl[2] += lightmap[i * 3 + 2] * scale[2];
					}
				}

				lightmap += size * 3; /* skip to next lightmap */
			}
		}

		/* add all the dynamic lights */
		if (surf->dlightframe == r_framecount)
		{
			R_AddDynamicLights(surf);
		}
	}

store:

	stride -= (smax << 2);
	float *bl = s_blocklights;

	for (int i = 0; i < tmax; i++, dest += stride)
	{
		for (int j = 0; j < smax; j++)
		{
			int r = Q_ftol(bl[0]);
			int g = Q_ftol(bl[1]);
			int b = Q_ftol(bl[2]);

			/* catch negative lights */
			if (r < 0)
				r = 0;
			if (g < 0)
				g = 0;
			if (b < 0)
				b = 0;

			/* determine the brightest of the three color components */
			int max;
			if (r > g)
				max = r;
			else
				max = g;
			if (b > max)
				max = b;

			/* alpha is ONLY used for the mono lightmap case. For this
			   reason we set it to the brightest of the color components
			   so that things don't get too dim. */
			int a = max;

			/* rescale all the color components if the
			   intensity of the greatest channel exceeds
			   1.0f */
			if (max > 255)
			{
				float t = 255.0F / max;
				r = r * t;
				g = g * t;
				b = b * t;
				a = a * t;
			}

			dest[0] = r;
			dest[1] = g;
			dest[2] = b;
			dest[3] = a;

			bl += 3;
			dest += 4;
		}
	}
}
