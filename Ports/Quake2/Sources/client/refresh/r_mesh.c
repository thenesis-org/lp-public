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
 * Mesh handling
 *
 * =======================================================================
 */

#include "local.h"

#define NUMVERTEXNORMALS 162
#define SHADEDOT_QUANT 16

float r_avertexnormals[NUMVERTEXNORMALS][3] =
{
	#include "constants/anorms.h"
};

/* precalculated dot products for quantized angles */
float r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "constants/anormtab.h"
;

typedef float vec4_t[4];
static vec4_t s_lerped[MAX_VERTS];
vec3_t shadevector;
float shadelight[3];
float *shadedots = r_avertexnormal_dots[0];
extern vec3_t lightspot;
extern qboolean graphics_has_stencil;

void R_LerpVerts(int nverts, dtrivertx_t *v, dtrivertx_t *ov, dtrivertx_t *verts, float *lerp, float move[3], float frontv[3], float backv[3])
{
	if (currententity->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM))
	{
		for (int i = 0; i < nverts; i++, v++, ov++, lerp += 4)
		{
			float *normal = r_avertexnormals[verts[i].lightnormalindex];
			lerp[0] = move[0] + ov->v[0] * backv[0] + v->v[0] * frontv[0] + normal[0] * POWERSUIT_SCALE;
			lerp[1] = move[1] + ov->v[1] * backv[1] + v->v[1] * frontv[1] + normal[1] * POWERSUIT_SCALE;
			lerp[2] = move[2] + ov->v[2] * backv[2] + v->v[2] * frontv[2] + normal[2] * POWERSUIT_SCALE;
		}
	}
	else
	{
		for (int i = 0; i < nverts; i++, v++, ov++, lerp += 4)
		{
			lerp[0] = move[0] + ov->v[0] * backv[0] + v->v[0] * frontv[0];
			lerp[1] = move[1] + ov->v[1] * backv[1] + v->v[1] * frontv[1];
			lerp[2] = move[2] + ov->v[2] * backv[2] + v->v[2] * frontv[2];
		}
	}
}

/*
 * Interpolates between two frames and origins
 */
void R_DrawAliasFrameLerp(dmdl_t *paliashdr, float backlerp)
{
	float alpha = 1.0f;
	if (currententity->flags & RF_TRANSLUCENT)
	{
		alpha = currententity->alpha;
	}

	if (currententity->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM))
	{
		oglwEnableTexturing(0, GL_FALSE);
	}

	daliasframe_t *frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + currententity->frame * paliashdr->framesize);
	daliasframe_t *oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + currententity->oldframe * paliashdr->framesize);

	/* move should be the delta back to the previous frame * backlerp */
	vec3_t move, delta, vectors[3];
	VectorSubtract(currententity->oldorigin, currententity->origin, delta);
	AngleVectors(currententity->angles, vectors[0], vectors[1], vectors[2]);
	move[0] = DotProduct(delta, vectors[0]); /* forward */
	move[1] = -DotProduct(delta, vectors[1]); /* left */
	move[2] = DotProduct(delta, vectors[2]); /* up */
	VectorAdd(move, oldframe->translate, move);

	float frontlerp = 1.0f - backlerp;

	for (int i = 0; i < 3; i++)
	{
		move[i] = backlerp * move[i] + frontlerp * frame->translate[i];
	}

	vec3_t frontv, backv;
	for (int i = 0; i < 3; i++)
	{
		frontv[i] = frontlerp * frame->scale[i];
		backv[i] = backlerp * oldframe->scale[i];
	}

	float *lerp = s_lerped[0];
	dtrivertx_t *verts = frame->verts, *v = verts, *ov = oldframe->verts;
	R_LerpVerts(paliashdr->num_xyz, v, ov, verts, lerp, move, frontv, backv);

	int *order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);

	oglwBegin(GL_TRIANGLES);
	while (1)
	{
		/* get the vertex count and primitive type */
		int count = *order++;

		if (!count)
		{
			break; /* done */
		}

		OpenGLWrapperVertex *vtx;
		if (count < 0)
		{
			count = -count;
			vtx = oglwAllocateTriangleFan(count);
		}
		else
		{
			vtx = oglwAllocateTriangleStrip(count);
		}

		if (currententity->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE))
		{
			do
			{
				int index_xyz = order[2];
				float *p = s_lerped[index_xyz];
				vtx = AddVertex3D_C(vtx, p[0], p[1], p[2], shadelight[0], shadelight[1], shadelight[2], alpha);
				order += 3;
			}
			while (--count);
		}
		else
		{
			do
			{
				int index_xyz = order[2];
				float *p = s_lerped[index_xyz];
				float l = shadedots[verts[index_xyz].lightnormalindex];
				float *tc = (float *)order;
				/* texture coordinates come from the draw list */
				/* normals and vertexes come from the frame list */
				vtx = AddVertex3D_CT1(vtx, p[0], p[1], p[2], l * shadelight[0], l * shadelight[1], l * shadelight[2], alpha, tc[0], tc[1]);
				order += 3;
			}
			while (--count);
		}
	}
	oglwEnd();

	if (currententity->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM))
	{
		oglwEnableTexturing(0, GL_TRUE);
	}
}

static void R_DrawAliasShadow(dmdl_t *paliashdr, int posenum)
{
	/* stencilbuffer shadows */
	if (graphics_has_stencil && gl_stencilshadow->value)
	{
		oglwEnableStencilTest(true);
	}

	oglwBegin(GL_TRIANGLES);

	float lheight = currententity->origin[2] - lightspot[2];
	int *order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);
	float height = -lheight + 0.1f;
	while (1)
	{
		/* get the vertex count and primitive type */
		int count = *order++;

		if (!count)
		{
			break; /* done */
		}

		OpenGLWrapperVertex *vtx;
		if (count < 0)
		{
			count = -count;
			vtx = oglwAllocateTriangleFan(count);
		}
		else
		{
			vtx = oglwAllocateTriangleStrip(count);
		}

		do
		{
			float *p = s_lerped[order[2]];
			float t = p[2] + lheight;
			vtx = AddVertex3D_C(vtx, p[0] - shadevector[0] * t, p[1] - shadevector[1] * t, height, 0.0f, 0.0f, 0.0f, 0.5f);
			order += 3;
		}
		while (--count);
	}
	oglwEnd();

	/* stencilbuffer shadows */
	if (graphics_has_stencil && gl_stencilshadow->value)
	{
		oglwEnableStencilTest(false);
	}
}

static qboolean R_CullAliasModel(vec3_t bbox[8], entity_t *e)
{
	int i;
	vec3_t mins, maxs;
	dmdl_t *paliashdr;
	vec3_t vectors[3];
	vec3_t thismins, oldmins, thismaxs, oldmaxs;
	daliasframe_t *pframe, *poldframe;
	vec3_t angles;

	paliashdr = (dmdl_t *)currentmodel->extradata;

	if ((e->frame >= paliashdr->num_frames) || (e->frame < 0))
	{
		VID_Printf(PRINT_DEVELOPER, "R_CullAliasModel %s: no such frame %d\n",
			currentmodel->name, e->frame);
		e->frame = 0;
	}

	if ((e->oldframe >= paliashdr->num_frames) || (e->oldframe < 0))
	{
		VID_Printf(PRINT_DEVELOPER, "R_CullAliasModel %s: no such oldframe %d\n",
			currentmodel->name, e->oldframe);
		e->oldframe = 0;
	}

	pframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames +
	                           e->frame * paliashdr->framesize);

	poldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames +
	                              e->oldframe * paliashdr->framesize);

	/* compute axially aligned mins and maxs */
	if (pframe == poldframe)
	{
		for (i = 0; i < 3; i++)
		{
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i] * 255;
		}
	}
	else
	{
		for (i = 0; i < 3; i++)
		{
			thismins[i] = pframe->translate[i];
			thismaxs[i] = thismins[i] + pframe->scale[i] * 255;

			oldmins[i] = poldframe->translate[i];
			oldmaxs[i] = oldmins[i] + poldframe->scale[i] * 255;

			if (thismins[i] < oldmins[i])
			{
				mins[i] = thismins[i];
			}
			else
			{
				mins[i] = oldmins[i];
			}

			if (thismaxs[i] > oldmaxs[i])
			{
				maxs[i] = thismaxs[i];
			}
			else
			{
				maxs[i] = oldmaxs[i];
			}
		}
	}

	/* compute a full bounding box */
	for (i = 0; i < 8; i++)
	{
		vec3_t tmp;

		if (i & 1)
		{
			tmp[0] = mins[0];
		}
		else
		{
			tmp[0] = maxs[0];
		}

		if (i & 2)
		{
			tmp[1] = mins[1];
		}
		else
		{
			tmp[1] = maxs[1];
		}

		if (i & 4)
		{
			tmp[2] = mins[2];
		}
		else
		{
			tmp[2] = maxs[2];
		}

		VectorCopy(tmp, bbox[i]);
	}

	/* rotate the bounding box */
	VectorCopy(e->angles, angles);
	angles[YAW] = -angles[YAW];
	AngleVectors(angles, vectors[0], vectors[1], vectors[2]);

	for (i = 0; i < 8; i++)
	{
		vec3_t tmp;

		VectorCopy(bbox[i], tmp);

		bbox[i][0] = DotProduct(vectors[0], tmp);
		bbox[i][1] = -DotProduct(vectors[1], tmp);
		bbox[i][2] = DotProduct(vectors[2], tmp);

		VectorAdd(e->origin, bbox[i], bbox[i]);
	}

	int p, f, aggregatemask = ~0;

	for (p = 0; p < 8; p++)
	{
		int mask = 0;

		for (f = 0; f < 4; f++)
		{
			float dp = DotProduct(frustum[f].normal, bbox[p]);

			if ((dp - frustum[f].dist) < 0)
			{
				mask |= (1 << f);
			}
		}

		aggregatemask &= mask;
	}

	if (aggregatemask)
	{
		return true;
	}

	return false;
}

void R_DrawAliasModel(entity_t *e)
{
	int i;
	dmdl_t *paliashdr;
	float an;
	vec3_t bbox[8];
	image_t *skin;

	if (!(e->flags & RF_WEAPONMODEL))
	{
		if (R_CullAliasModel(bbox, e))
		{
			return;
		}
	}

	if (e->flags & RF_WEAPONMODEL)
	{
		if (gl_lefthand->value == 2)
		{
			return;
		}
	}

	paliashdr = (dmdl_t *)currentmodel->extradata;

	/* get lighting information */
	if (currententity->flags &
	    (RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED |
	     RF_SHELL_BLUE | RF_SHELL_DOUBLE))
	{
		VectorClear(shadelight);

		if (currententity->flags & RF_SHELL_HALF_DAM)
		{
			shadelight[0] = 0.56;
			shadelight[1] = 0.59;
			shadelight[2] = 0.45;
		}

		if (currententity->flags & RF_SHELL_DOUBLE)
		{
			shadelight[0] = 0.9;
			shadelight[1] = 0.7;
		}

		if (currententity->flags & RF_SHELL_RED)
		{
			shadelight[0] = 1.0f;
		}

		if (currententity->flags & RF_SHELL_GREEN)
		{
			shadelight[1] = 1.0f;
		}

		if (currententity->flags & RF_SHELL_BLUE)
		{
			shadelight[2] = 1.0f;
		}
	}
	else
	if (currententity->flags & RF_FULLBRIGHT)
	{
		for (i = 0; i < 3; i++)
		{
			shadelight[i] = 1.0f;
		}
	}
	else
	{
		R_LightPoint(currententity->origin, shadelight);

		/* player lighting hack for communication back to server */
		if (currententity->flags & RF_WEAPONMODEL)
		{
			/* pick the greatest component, which should be
			   the same as the mono value returned by software */
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
	}

	if (currententity->flags & RF_MINLIGHT)
	{
		for (i = 0; i < 3; i++)
		{
			if (shadelight[i] > 0.1f)
			{
				break;
			}
		}

		if (i == 3)
		{
			shadelight[0] = 0.1f;
			shadelight[1] = 0.1f;
			shadelight[2] = 0.1f;
		}
	}

	if (currententity->flags & RF_GLOW)
	{
		/* bonus items will pulse with time */
		float scale;
		float min;

		scale = 0.1f * sinf(r_newrefdef.time * 7);

		for (i = 0; i < 3; i++)
		{
			min = shadelight[i] * 0.8f;
			shadelight[i] += scale;

			if (shadelight[i] < min)
			{
				shadelight[i] = min;
			}
		}
	}

	/* ir goggles color override */
	if (r_newrefdef.rdflags & RDF_IRGOGGLES && currententity->flags &
	    RF_IR_VISIBLE)
	{
		shadelight[0] = 1.0f;
		shadelight[1] = 0.0f;
		shadelight[2] = 0.0f;
	}

	shadedots = r_avertexnormal_dots[((int)(currententity->angles[1] *
	                                        (SHADEDOT_QUANT / 360.0f))) & (SHADEDOT_QUANT - 1)];

	an = currententity->angles[1] / 180 * Q_PI;
	shadevector[0] = cosf(-an);
	shadevector[1] = sinf(-an);
	shadevector[2] = 1;
	VectorNormalize(shadevector);

	/* locate the proper data */
	c_alias_polys += paliashdr->num_tris;

	/* draw all the triangles */
	if (currententity->flags & RF_DEPTHHACK)
	{
		/* hack the depth range to prevent view model from poking into walls */
		glDepthRangef(gldepthmin, gldepthmin + 0.3f * (gldepthmax - gldepthmin));
	}

	if ((currententity->flags & RF_WEAPONMODEL) && (gl_lefthand->value == 1.0F))
	{
		extern void R_MYgluPerspective(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar);

		oglwMatrixMode(GL_PROJECTION);
		oglwPushMatrix();
		oglwLoadIdentity();
		oglwScale(-1, 1, 1);
		R_MYgluPerspective(r_newrefdef.fov_y,
			(float)r_newrefdef.width / r_newrefdef.height,
			4, 4096);
		oglwMatrixMode(GL_MODELVIEW);

		glCullFace(GL_BACK);
	}

	oglwPushMatrix();
	e->angles[PITCH] = -e->angles[PITCH];
	R_RotateForEntity(e);
	e->angles[PITCH] = -e->angles[PITCH];

	/* select skin */
	if (currententity->skin)
	{
		skin = currententity->skin; /* custom player skin */
	}
	else
	{
		if (currententity->skinnum >= MAX_MD2SKINS)
		{
			skin = currentmodel->skins[0];
		}
		else
		{
			skin = currentmodel->skins[currententity->skinnum];

			if (!skin)
			{
				skin = currentmodel->skins[0];
			}
		}
	}

	if (!skin)
	{
		skin = r_notexture; /* fallback... */
	}

	oglwBindTexture(0, skin->texnum);
	oglwSetTextureBlending(0, GL_MODULATE);

	/* draw it */

	if ((currententity->frame >= paliashdr->num_frames) ||
	    (currententity->frame < 0))
	{
		VID_Printf(PRINT_DEVELOPER, "R_DrawAliasModel %s: no such frame %d\n",
			currentmodel->name, currententity->frame);
		currententity->frame = 0;
		currententity->oldframe = 0;
	}

	if ((currententity->oldframe >= paliashdr->num_frames) ||
	    (currententity->oldframe < 0))
	{
		VID_Printf(PRINT_DEVELOPER, "R_DrawAliasModel %s: no such oldframe %d\n",
			currentmodel->name, currententity->oldframe);
		currententity->frame = 0;
		currententity->oldframe = 0;
	}

	if (!gl_lerpmodels->value)
	{
		currententity->backlerp = 0;
	}

	oglwEnableSmoothShading(true);

	R_DrawAliasFrameLerp(paliashdr, currententity->backlerp);

	oglwEnableSmoothShading(false);

	oglwPopMatrix();

	if ((currententity->flags & RF_WEAPONMODEL) && (gl_lefthand->value == 1.0F))
	{
		oglwMatrixMode(GL_PROJECTION);
		oglwPopMatrix();
		oglwMatrixMode(GL_MODELVIEW);
		glCullFace(GL_FRONT);
	}

	if (currententity->flags & RF_DEPTHHACK)
	{
		glDepthRangef(gldepthmin, gldepthmax);
	}

	if (gl_shadows->value &&
	    !(currententity->flags & (RF_TRANSLUCENT | RF_WEAPONMODEL | RF_NOSHADOW)))
	{
		oglwPushMatrix();

		/* don't rotate shadows on ungodly axes */
		oglwTranslate(e->origin[0], e->origin[1], e->origin[2]);
		oglwRotate(e->angles[1], 0, 0, 1);

		oglwEnableTexturing(0, GL_FALSE);
		oglwEnableBlending(true);
		// oglwEnableDepthWrite(false); // Maybe better with depth written.
		R_DrawAliasShadow(paliashdr, currententity->frame);
		oglwEnableTexturing(0, GL_TRUE);
		oglwEnableBlending(false);
		// oglwEnableDepthWrite(true);
		oglwPopMatrix();
	}

	oglwSetTextureBlending(0, GL_REPLACE);
}
