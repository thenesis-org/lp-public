#include "client/refresh/r_private.h"

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

static vec4_t s_lerped[MAX_VERTS];

void R_AliasModel_lerp(entity_t *entity, int nverts, dtrivertx_t *v, dtrivertx_t *ov, dtrivertx_t *verts, float *lerp, float move[3], float frontv[3], float backv[3])
{
	if (entity->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM))
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
void R_AliasModel_drawLerp(entity_t *entity, dmdl_t *paliashdr, float backlerp, float *shadelight, float *shadedots)
{
	float alpha = 1.0f;
	if (entity->flags & RF_TRANSLUCENT)
		alpha = entity->alpha;
    if (alpha < 1.0f)
    {
		oglwEnableBlending(true);
		oglwEnableDepthWrite(false);
    }
    
	if (entity->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM))
		oglwEnableTexturing(0, GL_FALSE);

	daliasframe_t *frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + entity->frame * paliashdr->framesize);
	daliasframe_t *oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + entity->oldframe * paliashdr->framesize);

	/* move should be the delta back to the previous frame * backlerp */
	vec3_t move, delta, vectors[3];
	VectorSubtract(entity->oldorigin, entity->origin, delta);
	AngleVectors(entity->angles, vectors[0], vectors[1], vectors[2]);
	move[0] = DotProduct(delta, vectors[0]); /* forward */
	move[1] = -DotProduct(delta, vectors[1]); /* left */
	move[2] = DotProduct(delta, vectors[2]); /* up */
	VectorAdd(move, oldframe->translate, move);

	float frontlerp = 1.0f - backlerp;

	for (int i = 0; i < 3; i++)
		move[i] = backlerp * move[i] + frontlerp * frame->translate[i];

	vec3_t frontv, backv;
	for (int i = 0; i < 3; i++)
	{
		frontv[i] = frontlerp * frame->scale[i];
		backv[i] = backlerp * oldframe->scale[i];
	}

	float *lerp = s_lerped[0];
	dtrivertx_t *verts = frame->verts, *v = verts, *ov = oldframe->verts;
	R_AliasModel_lerp(entity, paliashdr->num_xyz, v, ov, verts, lerp, move, frontv, backv);

	int *order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);

	oglwBegin(GL_TRIANGLES);
	while (1)
	{
		/* get the vertex count and primitive type */
		int count = *order++;
		if (!count)
			break; /* done */

		OglwVertex *vtx;
		if (count < 0)
		{
			count = -count;
			vtx = oglwAllocateTriangleFan(count);
		}
		else
			vtx = oglwAllocateTriangleStrip(count);
        if (vtx == NULL)
        {
            order += 3 * count;
            continue;
        }
        
		if (entity->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE))
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

	if (entity->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM))
		oglwEnableTexturing(0, GL_TRUE);

    if (alpha < 1.0f)
    {
		oglwEnableBlending(false);
		oglwEnableDepthWrite(true);
    }
}

static void R_AliasModel_drawShadow(entity_t *entity, dmdl_t *paliashdr, vec3_t shadevector, vec3_t lightSpot)
{
	/* stencilbuffer shadows */
	if (r_stencilAvailable && gl_stencilshadow->value)
		oglwEnableStencilTest(true);

	oglwBegin(GL_TRIANGLES);

	float lheight = entity->origin[2] - lightSpot[2];
	int *order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);
	float height = -lheight + 0.1f;
	while (1)
	{
		/* get the vertex count and primitive type */
		int count = *order++;
		if (!count)
			break; /* done */

		OglwVertex *vtx;
		if (count < 0)
		{
			count = -count;
			vtx = oglwAllocateTriangleFan(count);
		}
		else
			vtx = oglwAllocateTriangleStrip(count);
        if (vtx == NULL)
        {
            order += 3 * count;
            continue;
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

	if (r_stencilAvailable && gl_stencilshadow->value)
		oglwEnableStencilTest(false);
}

static qboolean R_AliasModel_cull(entity_t *entity, vec3_t bbox[8])
{
    model_t *model = entity->model;
	dmdl_t *paliashdr = (dmdl_t *)model->extradata;

	if ((entity->frame >= paliashdr->num_frames) || (entity->frame < 0))
	{
		R_printf(PRINT_DEVELOPER, "R_AliasModel_cull %s: no such frame %d\n", model->name, entity->frame);
		entity->frame = 0;
	}

	if ((entity->oldframe >= paliashdr->num_frames) || (entity->oldframe < 0))
	{
		R_printf(PRINT_DEVELOPER, "R_AliasModel_cull %s: no such oldframe %d\n", model->name, entity->oldframe);
		entity->oldframe = 0;
	}

	daliasframe_t *pframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + entity->frame * paliashdr->framesize);
	daliasframe_t *poldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + entity->oldframe * paliashdr->framesize);

	/* compute axially aligned mins and maxs */
	vec3_t mins, maxs;
	if (pframe == poldframe)
	{
		for (int i = 0; i < 3; i++)
		{
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i] * 255;
		}
	}
	else
	{
		for (int i = 0; i < 3; i++)
		{
			float thismins = pframe->translate[i];
			float thismaxs = thismins + pframe->scale[i] * 255;

			float oldmins = poldframe->translate[i];
			float oldmaxs = oldmins + poldframe->scale[i] * 255;

			if (thismins < oldmins)
				mins[i] = thismins;
			else
				mins[i] = oldmins;

			if (thismaxs > oldmaxs)
				maxs[i] = thismaxs;
			else
				maxs[i] = oldmaxs;
		}
	}

	/* compute a full bounding box */
	for (int i = 0; i < 8; i++)
	{
		vec3_t tmp;

		if (i & 1)
			tmp[0] = mins[0];
		else
			tmp[0] = maxs[0];

		if (i & 2)
			tmp[1] = mins[1];
		else
			tmp[1] = maxs[1];

		if (i & 4)
			tmp[2] = mins[2];
		else
			tmp[2] = maxs[2];

		VectorCopy(tmp, bbox[i]);
	}

	/* rotate the bounding box */
	vec3_t angles;
	VectorCopy(entity->angles, angles);
	angles[YAW] = -angles[YAW];
	vec3_t vectors[3];
	AngleVectors(angles, vectors[0], vectors[1], vectors[2]);

	for (int i = 0; i < 8; i++)
	{
		vec3_t tmp;

		VectorCopy(bbox[i], tmp);

		bbox[i][0] = DotProduct(vectors[0], tmp);
		bbox[i][1] = -DotProduct(vectors[1], tmp);
		bbox[i][2] = DotProduct(vectors[2], tmp);

		VectorAdd(entity->origin, bbox[i], bbox[i]);
	}

	int aggregatemask = ~0;
	for (int p = 0; p < 8; p++)
	{
		int mask = 0;
		for (int f = 0; f < 4; f++)
		{
			float dp = DotProduct(frustum[f].normal, bbox[p]);
			if ((dp - frustum[f].dist) < 0)
				mask |= (1 << f);
		}
		aggregatemask &= mask;
	}
	if (aggregatemask)
		return true;

	return false;
}

static void R_AliasModel_light(entity_t *entity, vec3_t shadelight, vec3_t lightSpot)
{
	VectorClear(lightSpot);
	if (entity->flags & (RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE))
	{
		VectorClear(shadelight);

		if (entity->flags & RF_SHELL_HALF_DAM)
		{
			shadelight[0] = 0.56;
			shadelight[1] = 0.59;
			shadelight[2] = 0.45;
		}

		if (entity->flags & RF_SHELL_DOUBLE)
		{
			shadelight[0] = 0.9;
			shadelight[1] = 0.7;
		}

		if (entity->flags & RF_SHELL_RED)
			shadelight[0] = 1.0f;

		if (entity->flags & RF_SHELL_GREEN)
			shadelight[1] = 1.0f;

		if (entity->flags & RF_SHELL_BLUE)
			shadelight[2] = 1.0f;
	}
	else if (entity->flags & RF_FULLBRIGHT)
	{
		for (int i = 0; i < 3; i++)
			shadelight[i] = 1.0f;
	}
	else
	{
        cplane_t *lightPlane;
		R_Lighmap_lightPoint(entity, entity->origin, shadelight, lightSpot, &lightPlane);

		/* player lighting hack for communication back to server */
		if (entity->flags & RF_WEAPONMODEL)
		{
			/* pick the greatest component, which should be
			   the same as the mono value returned by software */
			if (shadelight[0] > shadelight[1])
			{
				if (shadelight[0] > shadelight[2])
					gl_lightlevel->value = 150 * shadelight[0];
				else
					gl_lightlevel->value = 150 * shadelight[2];
			}
			else
			{
				if (shadelight[1] > shadelight[2])
					gl_lightlevel->value = 150 * shadelight[1];
				else
					gl_lightlevel->value = 150 * shadelight[2];
			}
		}
	}

	if (entity->flags & RF_MINLIGHT)
	{
        int i;
		for (i = 0; i < 3; i++)
		{
			if (shadelight[i] > 0.1f)
				break;
		}
		if (i == 3)
		{
			shadelight[0] = 0.1f;
			shadelight[1] = 0.1f;
			shadelight[2] = 0.1f;
		}
	}

	if (entity->flags & RF_GLOW)
	{
		/* bonus items will pulse with time */
		float scale = 0.1f * sinf(r_newrefdef.time * 7);
		for (int i = 0; i < 3; i++)
		{
			float min = shadelight[i] * 0.8f;
			shadelight[i] += scale;
			if (shadelight[i] < min)
				shadelight[i] = min;
		}
	}

	/* ir goggles color override */
	if (r_newrefdef.rdflags & RDF_IRGOGGLES && entity->flags & RF_IR_VISIBLE)
	{
		shadelight[0] = 1.0f;
		shadelight[1] = 0.0f;
		shadelight[2] = 0.0f;
	}
}

void R_AliasModel_draw(entity_t *entity)
{
	if (!(entity->flags & RF_WEAPONMODEL))
	{
        vec3_t bbox[8];
		if (R_AliasModel_cull(entity, bbox))
			return;
	}

	if (entity->flags & RF_WEAPONMODEL)
	{
		if (gl_lefthand->value == 2)
			return;
	}

    model_t *model = entity->model;
	dmdl_t *paliashdr = (dmdl_t *)model->extradata;
 
	/* locate the proper data */
	c_alias_polys += paliashdr->num_tris;

	/* draw all the triangles */
	if (entity->flags & RF_DEPTHHACK)
	{
		/* hack the depth range to prevent view model from poking into walls */
		oglwSetDepthRange(gldepthmin, gldepthmin + 0.3f * (gldepthmax - gldepthmin));
	}

	if ((entity->flags & RF_WEAPONMODEL) && (gl_lefthand->value == 1.0F))
	{
		oglwMatrixMode(GL_PROJECTION);
		oglwPushMatrix();
		oglwLoadIdentity();
		oglwScale(-1, 1, 1);
		R_View_setupProjection(r_newrefdef.fov_y, (float)r_newrefdef.width / r_newrefdef.height, 4, 4096);
		oglwMatrixMode(GL_MODELVIEW);

		glCullFace(GL_BACK);
	}

	oglwPushMatrix();
	entity->angles[PITCH] = -entity->angles[PITCH];
	R_Entity_rotate(entity);
	entity->angles[PITCH] = -entity->angles[PITCH];

	/* select skin */
	image_t *skin;
	if (entity->skin)
		skin = entity->skin; /* custom player skin */
	else
	{
        model_t *model = entity->model;
		if (entity->skinnum >= MAX_MD2SKINS)
			skin = model->skins[0];
		else
		{
			skin = model->skins[entity->skinnum];
			if (!skin)
				skin = model->skins[0];
		}
	}

	if (!skin)
		skin = r_notexture; /* fallback... */

	oglwBindTexture(0, skin->texnum);
	oglwSetTextureBlending(0, GL_MODULATE);
	oglwEnableSmoothShading(true);

	if ((entity->frame >= paliashdr->num_frames) || (entity->frame < 0))
	{
		R_printf(PRINT_DEVELOPER, "R_AliasModel_draw %s: no such frame %d\n", entity->model->name, entity->frame);
		entity->frame = 0;
		entity->oldframe = 0;
	}

	if ((entity->oldframe >= paliashdr->num_frames) || (entity->oldframe < 0))
	{
		R_printf(PRINT_DEVELOPER, "R_AliasModel_draw %s: no such oldframe %d\n", entity->model->name, entity->oldframe);
		entity->frame = 0;
		entity->oldframe = 0;
	}

	if (!gl_lerpmodels->value)
	{
		entity->backlerp = 0;
	}

    vec3_t shadelight;
    vec3_t lightSpot;
    R_AliasModel_light(entity, shadelight, lightSpot);
	float *shadedots = r_avertexnormal_dots[((int)(entity->angles[1] * (SHADEDOT_QUANT / 360.0f))) & (SHADEDOT_QUANT - 1)];
	R_AliasModel_drawLerp(entity, paliashdr, entity->backlerp, shadelight, shadedots);

	oglwEnableSmoothShading(false);

	oglwPopMatrix();

	if ((entity->flags & RF_WEAPONMODEL) && (gl_lefthand->value == 1.0F))
	{
		oglwMatrixMode(GL_PROJECTION);
		oglwPopMatrix();
		oglwMatrixMode(GL_MODELVIEW);
		glCullFace(GL_FRONT);
	}

	if (entity->flags & RF_DEPTHHACK)
	{
		oglwSetDepthRange(gldepthmin, gldepthmax);
	}

	if (gl_shadows->value && !(entity->flags & (RF_TRANSLUCENT | RF_WEAPONMODEL | RF_NOSHADOW)))
	{
		oglwPushMatrix();

		/* don't rotate shadows on ungodly axes */
		oglwTranslate(entity->origin[0], entity->origin[1], entity->origin[2]);
		oglwRotate(entity->angles[1], 0, 0, 1);

		oglwEnableTexturing(0, GL_FALSE);
		oglwEnableBlending(true);
		// oglwEnableDepthWrite(false); // Maybe better with depth written.

        vec3_t shadevector;
        {
            float an = entity->angles[1] / 180 * Q_PI;
            shadevector[0] = cosf(-an);
            shadevector[1] = sinf(-an);
            shadevector[2] = 1;
            VectorNormalize(shadevector);
        }
		R_AliasModel_drawShadow(entity, paliashdr, shadevector, lightSpot);

		oglwEnableTexturing(0, GL_TRUE);
		oglwEnableBlending(false);
		// oglwEnableDepthWrite(true);
		oglwPopMatrix();
	}

	oglwSetTextureBlending(0, GL_REPLACE);
}
