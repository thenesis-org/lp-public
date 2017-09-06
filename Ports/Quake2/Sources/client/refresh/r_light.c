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

static vec3_t pointcolor;
static cplane_t *lightplane; /* used as shadow plane */
vec3_t lightspot;
static float s_blocklights[34 * 34 * 3];

static void R_RenderDlight(dlight_t *light)
{
	OglwVertex *vtx = oglwAllocateTriangleFan(1 + 16 + 1);

	vec3_t v;

	float rad = light->intensity * 0.35f;
	for (int i = 0; i < 3; i++)
		v[i] = light->origin[i] - vpn[i] * rad;
	vtx = AddVertex3D_C(vtx, v[0], v[1], v[2], light->color[0] * 0.2f, light->color[1] * 0.2f, light->color[2] * 0.2f, 1.0f);

	for (int i = 16; i >= 0; i--)
	{
		float a = i / 16.0f * Q_PI * 2, cosa = cosf(a), sina = sinf(a);
		for (int j = 0; j < 3; j++)
			v[j] = light->origin[j] + vright[j] * cosa * rad + vup[j] * sina * rad;
		vtx = AddVertex3D_C(vtx, v[0], v[1], v[2], 0.0f, 0.0f, 0.0f, 1.0f);
	}
}

void R_RenderDlights()
{
	if (!gl_flashblend->value)
		return;

	oglwEnableBlending(true);
	oglwSetBlendingFunction(GL_ONE, GL_ONE);
	oglwEnableDepthWrite(false);

	oglwEnableSmoothShading(true);
	oglwEnableTexturing(0, GL_FALSE);

	oglwBegin(GL_TRIANGLES);
	dlight_t *l = r_newrefdef.dlights;
	for (int i = 0; i < r_newrefdef.num_dlights; i++, l++)
		R_RenderDlight(l);
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
		return;

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
			sidebit = 0;
		else
			sidebit = SURF_PLANEBACK;

		if ((surf->flags & SURF_PLANEBACK) != sidebit)
			continue;

        int dlightframecount = r_framecount + 1; // because the count hasn't advanced yet for this frame
		if (surf->dlightframe != dlightframecount)
		{
			surf->dlightbits = 0;
			surf->dlightframe = dlightframecount;
		}

		surf->dlightbits |= bit;
	}

	R_MarkLights(light, bit, node->children[0]);
	R_MarkLights(light, bit, node->children[1]);
}

void R_PushDlights()
{
	if (gl_flashblend->value)
		return;

	dlight_t *l = r_newrefdef.dlights;
	for (int i = 0; i < r_newrefdef.num_dlights; i++, l++)
		R_MarkLights(l, 1 << i, r_worldmodel->nodes);
}

int R_RecursiveLightPoint(mnode_t *node, vec3_t start, vec3_t end)
{
	if (node->contents != -1)
		return -1; /* didn't hit anything */

	/* calculate mid point */
	cplane_t *plane = node->plane;
	float front = DotProduct(start, plane->normal) - plane->dist;
	float back = DotProduct(end, plane->normal) - plane->dist;
	int side = front < 0;

	if ((back < 0) == side)
		return R_RecursiveLightPoint(node->children[side], start, end);

	float frac = front / (front - back);
	vec3_t mid;
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

	/* go down front side */
	int r = R_RecursiveLightPoint(node->children[side], start, mid);
	if (r >= 0)
		return r; /* hit something */

	if ((back < 0) == side)
		return -1; /* didn't hit anuthing */

	/* check for impact on this node */
	VectorCopy(mid, lightspot);
	lightplane = plane;

	msurface_t *surf = r_worldmodel->surfaces + node->firstsurface;

	for (int i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
			continue; /* no lightmaps */

		mtexinfo_t *tex = surf->texinfo;

		int s = DotProduct(mid, tex->vecs[0]) + tex->vecs[0][3];
		int t = DotProduct(mid, tex->vecs[1]) + tex->vecs[1][3];
		if (s < surf->texturemins[0] || t < surf->texturemins[1])
			continue;

		int ds = s - surf->texturemins[0];
		int dt = t - surf->texturemins[1];
		if (ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		if (!surf->samples)
			return 0;

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
					pointcolor[i] += lightmap[i] * modulate * styleRgb[i];
				lightmap += 3 * ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1);
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
		VectorCopy(vec3_origin, color);
	else
		VectorCopy(pointcolor, color);

	/* add dynamic lights */
	dl = r_newrefdef.dlights;

	for (lnum = 0; lnum < r_newrefdef.num_dlights; lnum++, dl++)
	{
		VectorSubtract(currententity->origin, dl->origin, dist);
		add = dl->intensity - VectorLength(dist);
		add *= (1.0f / 256);
		if (add > 0)
			VectorMA(color, add, dl->color, color);
	}

	VectorScale(color, gl_modulate->value, color);
}

static void R_AddDynamicLights(msurface_t *surf)
{
	int smax = (surf->extents[0] >> 4) + 1;
	int tmax = (surf->extents[1] >> 4) + 1;
	mtexinfo_t *tex = surf->texinfo;

	for (int lnum = 0; lnum < r_newrefdef.num_dlights; lnum++)
	{
		if (!(surf->dlightbits & (1 << lnum)))
			continue; /* not lit by this light */

		dlight_t *dl = &r_newrefdef.dlights[lnum];
        // Distance of the light from the plane.
		float fdist = DotProduct(dl->origin, surf->plane->normal) - surf->plane->dist;
        // Radius of the light on the plane.
		float frad = dl->intensity - fabsf(fdist);
		// Cut off distance.
		float fminlight = frad - DLIGHT_CUTOFF;
		if (fminlight < 0.0f)
			continue;

        // Position of the projection of the center of the light in 3D.
		vec3_t impact;
		for (int i = 0; i < 3; i++)
			impact[i] = dl->origin[i] - surf->plane->normal[i] * fdist;

        // Position of the projection of the center of the light in 2D.
		float localX = DotProduct(impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0];
		float localY = DotProduct(impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1];
        
        float colorR = dl->color[0], colorG = dl->color[1], colorB = dl->color[2];
		float *pfBL = s_blocklights;
		for (int t = 0; t < tmax; t++)
		{
			float td = localY - (t << 4);
			for (int s = 0; s < smax; s++, pfBL += 3)
			{
				float sd = localX - (s << 4);
                float distance = sqrtf(td * td + sd * sd); // TODO: Could use sqrt approximation here (or rsqrt(x)*x).
				if (distance < fminlight)
				{
                    float attenuation = frad - distance;
                    float lr = pfBL[0] + attenuation * colorR;
                    float lg = pfBL[1] + attenuation * colorG;
                    float lb = pfBL[2] + attenuation * colorB;
					pfBL[0] = lr;
					pfBL[1] = lg;
					pfBL[2] = lb;
				}
			}
		}
	}
}

void R_SetCacheState(msurface_t *surf)
{
	for (int maps = 0; maps < MAXLIGHTMAPS; maps++)
	{
        int style = surf->styles[maps];
        if (style == 255)
            break;
		surf->cached_light[maps] = r_newrefdef.lightstyles[style].white;
	}
}

/*
 * Combine and scale multiple lightmaps into the floating format in blocklights
 */
void R_BuildLightMap(msurface_t *surf, byte *dest, int stride)
{
	if (surf->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP))
		VID_Error(ERR_DROP, "R_BuildLightMap called for non-lit surface");

	int smax = (surf->extents[0] >> 4) + 1;
	int tmax = (surf->extents[1] >> 4) + 1;
	int size = smax * tmax;
	if (size > (int)(sizeof(s_blocklights) >> 4))
		VID_Error(ERR_DROP, "Bad s_blocklights size");

	/* set to full bright if no light data */
	if (!surf->samples)
	{
        float *bl = s_blocklights;
		for (int i = 0; i < size * 3; i++)
			bl[i] = 255;
	}
    else
	{
		// Count the number of maps.
		int nummaps;
		for (nummaps = 0; nummaps < MAXLIGHTMAPS && surf->styles[nummaps] != 255; nummaps++)
            ;

		byte *lightmap = surf->samples;

		/* add all the lightmaps */
		if (nummaps == 1)
		{
            float scale[3];
            for (int i = 0; i < 3; i++)
            {
                scale[i] = gl_modulate->value * r_newrefdef.lightstyles[surf->styles[0]].rgb[i];
            }

            float *bl = s_blocklights;
            if (scale[0] == 1.0f && scale[1] == 1.0f && scale[2] == 1.0f)
            {
                for (int i = 0; i < size; i++, lightmap += 3, bl += 3)
                {
                    float lr = lightmap[0];
                    float lg = lightmap[1];
                    float lb = lightmap[2];
                    bl[0] = lr;
                    bl[1] = lg;
                    bl[2] = lb;
                }
            }
            else
            {
                for (int i = 0; i < size; i++, lightmap += 3, bl += 3)
                {
                    float lr = lightmap[0] * scale[0];
                    float lg = lightmap[1] * scale[1];
                    float lb = lightmap[2] * scale[2];
                    bl[0] = lr;
                    bl[1] = lg;
                    bl[2] = lb;
                }
            }
		}
		else
		{
			memset(s_blocklights, 0, sizeof(s_blocklights[0]) * size * 3);
			for (int maps = 0; maps < nummaps; maps++)
			{
                float scale[3];
				for (int i = 0; i < 3; i++)
				{
					scale[i] = gl_modulate->value * r_newrefdef.lightstyles[surf->styles[maps]].rgb[i];
				}

				float *bl = s_blocklights;
				if (scale[0] == 1.0f && scale[1] == 1.0f && scale[2] == 1.0f)
				{
					for (int i = 0; i < size; i++, lightmap += 3, bl += 3)
					{
                        float lr = bl[0] + lightmap[0];
                        float lg = bl[1] + lightmap[1];
                        float lb = bl[2] + lightmap[2];
						bl[0] = lr;
						bl[1] = lg;
						bl[2] = lb;
					}
				}
				else
				{
					for (int i = 0; i < size; i++, lightmap += 3, bl += 3)
					{
                        float lr = bl[0] + lightmap[0] * scale[0];
                        float lg = bl[1] + lightmap[1] * scale[1];
                        float lb = bl[2] + lightmap[2] * scale[2];
						bl[0] = lr;
						bl[1] = lg;
						bl[2] = lb;
					}
				}
			}
		}

		/* add all the dynamic lights */
		if (surf->dlightframe == r_framecount)
			R_AddDynamicLights(surf);
	}

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
			int max = r;
			if (max < g)
				max = g;
			if (max < b)
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
