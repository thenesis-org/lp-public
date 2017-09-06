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
#include "Rendering/gl_model.h"
#include "Rendering/glquake.h"
#include "Rendering/render.h"
#include "Sound/sound.h"

#include "OpenGLES/OpenGLWrapper.h"

#include <string.h>

int R_LightPoint(vec3_t p);
void R_DrawBrushModel(entity_t *e);
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);
void R_AnimateLight();
void V_CalcBlend();
void R_DrawWorld();
void R_RenderDlights();
void R_DrawWaterSurfaces();
void R_RenderBrushPoly(msurface_t *fa, float alpha);
void GL_BuildLightmaps();

void R_InitParticleTexture();
void R_InitParticles();
void R_ClearParticles();
void R_DrawParticles();

void R_Envmap_f();

int texture_extension_number;

entity_t r_worldentity;

entity_t *currententity;

int r_visframecount; // bumped when going to a new PVS
int r_framecount; // used for dlight push checking

mplane_t frustum[4];

int c_brush_polys, c_alias_polys;

qboolean envmap; // true during envmap command capture

qboolean gl_mtexable = 0;

int currenttexture = -1; // to avoid unnecessary texture sets

int cnttextures[2] = { -1, -1 }; // cached

int particletexture; // little dot for particles
int playertextures; // up to 16 color translated skins

int mirrortexturenum; // quake texturenum, not gltexturenum
qboolean mirror;
mplane_t *mirror_plane;

//
// view origin
//
vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t r_origin;

float r_world_matrix[16];
float r_base_world_matrix[16];

//
// screen size info
//
refdef_t r_refdef;

mleaf_t *r_viewleaf, *r_oldviewleaf;

texture_t *r_notexture_mip;

int d_lightstylevalue[256]; // 8.8 fraction of base light value

void R_MarkLeaves();

cvar_t vid_fullscreen = { "vid_fullscreen", "0" };

cvar_t r_norefresh = { "r_norefresh", "0" };
cvar_t r_drawentities = { "r_drawentities", "1" };
cvar_t r_drawviewmodel = { "r_drawviewmodel", "1" };
cvar_t r_novis = { "r_novis", "0" };

cvar_t gl_clear = { "gl_clear", "0" };
cvar_t gl_ztrick = { "gl_ztrick", "0" };

cvar_t gl_cull = { "gl_cull", "1" };

// Lightmaps and lighting.
cvar_t r_fullbright = { "r_fullbright", "0" };
cvar_t r_lightmap = { "r_lightmap", "0" };
cvar_t r_dynamic = { "r_dynamic", "1" };
cvar_t gl_flashblend = { "gl_flashblend", "0" };

// Other world and brush model rendering.
cvar_t r_mirroralpha = { "r_mirroralpha", "1" };
cvar_t r_wateralpha = { "r_wateralpha", "1" };
cvar_t gl_keeptjunctions = { "gl_keeptjunctions", "0" };
cvar_t gl_reporttjunctions = { "gl_reporttjunctions", "0" };
cvar_t gl_texsort = { "gl_texsort", "1" };

// Model.
cvar_t r_shadows = { "r_shadows", "0" };
cvar_t gl_smoothmodels = { "gl_smoothmodels", "1" };
cvar_t gl_affinemodels = { "gl_affinemodels", "0" };
cvar_t gl_doubleeyes = { "gl_doubleeys", "1" };

// Player model.
cvar_t gl_playermip = { "gl_playermip", "0" };
cvar_t gl_nocolors = { "gl_nocolors", "0" };

// Full screen.
cvar_t gl_polyblend = { "gl_polyblend", "1" };

cvar_t gl_particle_min_size = { "gl_particle_min_size", "2", true, false };
cvar_t gl_particle_max_size = { "gl_particle_max_size", "40", true, false };
cvar_t gl_particle_size = { "gl_particle_size", "40", true, false };
cvar_t gl_particle_att_a = { "gl_particle_att_a", "0.01", true, false };
cvar_t gl_particle_att_b = { "gl_particle_att_b", "0.0", true, false };
cvar_t gl_particle_att_c = { "gl_particle_att_c", "0.01", true, false };

float gldepthmin = 0.0f;
float gldepthmax = 0.0f;

const GLubyte *gl_vendor;
const GLubyte *gl_renderer;
const GLubyte *gl_version;
const GLubyte *gl_extensions;

void R_Init()
{
	Cmd_AddCommand("timerefresh", R_TimeRefresh_f);
	Cmd_AddCommand("envmap", R_Envmap_f);
	Cmd_AddCommand("pointfile", R_ReadPointFile_f);

	Cvar_RegisterVariable(&vid_fullscreen);

	Cvar_RegisterVariable(&r_norefresh);
	Cvar_RegisterVariable(&r_lightmap);
	Cvar_RegisterVariable(&r_fullbright);
	Cvar_RegisterVariable(&gl_texsort);
	Cvar_RegisterVariable(&r_drawentities);
	Cvar_RegisterVariable(&r_drawviewmodel);
	Cvar_RegisterVariable(&r_shadows);
	Cvar_RegisterVariable(&r_mirroralpha);
	Cvar_RegisterVariable(&r_wateralpha);
	Cvar_RegisterVariable(&r_dynamic);
	Cvar_RegisterVariable(&r_novis);

	Cvar_RegisterVariable(&gl_clear);
	Cvar_RegisterVariable(&gl_ztrick);

	if (gl_mtexable)
		Cvar_SetValue("gl_texsort", 0.0f);

	Cvar_RegisterVariable(&gl_cull);
	Cvar_RegisterVariable(&gl_smoothmodels);
	Cvar_RegisterVariable(&gl_affinemodels);
	Cvar_RegisterVariable(&gl_polyblend);
	Cvar_RegisterVariable(&gl_flashblend);
	Cvar_RegisterVariable(&gl_playermip);
	Cvar_RegisterVariable(&gl_nocolors);

	Cvar_RegisterVariable(&gl_keeptjunctions);
	Cvar_RegisterVariable(&gl_reporttjunctions);

	Cvar_RegisterVariable(&gl_doubleeyes);

	Cvar_RegisterVariable(&gl_particle_min_size);
	Cvar_RegisterVariable(&gl_particle_max_size);
	Cvar_RegisterVariable(&gl_particle_size);
	Cvar_RegisterVariable(&gl_particle_att_a);
	Cvar_RegisterVariable(&gl_particle_att_b);
	Cvar_RegisterVariable(&gl_particle_att_c);

	R_InitParticles();
	R_InitParticleTexture();

	#ifdef GLTEST
	Test_Init();
	#endif

	playertextures = texture_extension_number;
	texture_extension_number += 16;
}

void R_InitTextures()
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

void R_NewMap()
{
	int i;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;                    // normal light value

	memset(&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = cl.worldmodel;

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for (i = 0; i < cl.worldmodel->numleafs; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles();

	GL_BuildLightmaps();

	// identify sky texture
	skytexturenum = -1;
	mirrortexturenum = -1;
	for (i = 0; i < cl.worldmodel->numtextures; i++)
	{
		if (!cl.worldmodel->textures[i])
			continue;
		if (!Q_strncmp(cl.worldmodel->textures[i]->name, "sky", 3))
			skytexturenum = i;
		if (!Q_strncmp(cl.worldmodel->textures[i]->name, "window02_1", 10))
			mirrortexturenum = i;
		cl.worldmodel->textures[i]->texturechain = NULL;
	}
}

// Returns true if the box is completely outside the frustom
qboolean R_CullBox(vec3_t mins, vec3_t maxs)
{
	int i;
	for (i = 0; i < 4; i++)
		if (BoxOnPlaneSide(mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}

void R_RotateForEntity(entity_t *e)
{
	oglwTranslate(e->origin[0], e->origin[1], e->origin[2]);
	oglwRotate(e->angles[1], 0, 0, 1);
	oglwRotate(-e->angles[0], 0, 1, 0);
	oglwRotate(e->angles[2], 1, 0, 0);
}

/*
   =============================================================

   SPRITE MODELS

   =============================================================
 */

mspriteframe_t* R_GetSpriteFrame(entity_t *e)
{
	msprite_t *psprite;
	mspritegroup_t *pspritegroup;
	mspriteframe_t *pspriteframe;
	int i, numframes, frame;
	float *pintervals, fullinterval, targettime, time;

	psprite = e->model->cache.data;
	frame = e->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_Printf("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes - 1];

		time = cl.time + e->syncbase;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i = 0; i < (numframes - 1); i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}

void R_DrawSpriteModel(entity_t *e)
{
	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	mspriteframe_t *frame = R_GetSpriteFrame(e);
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
		up = vup;
		right = vright;
	}

	GL_DisableMultitexture();

	GL_Bind(frame->gl_texturenum);

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

/*
   =============================================================

   ALIAS MODELS

   =============================================================
 */

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
static const float r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "Rendering/anorm_dots.h"
;

static void GL_DrawAliasFrame(entity_t *e, aliashdr_t *paliashdr, int pose0, int pose1, float poseBlend, float light)
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

extern vec3_t lightspot;

static void GL_DrawAliasShadow(entity_t *e, aliashdr_t *paliashdr, int pose0, int pose1, float poseBlend)
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
			p[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			p[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			p[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

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
}

static void R_DrawAliasModel(entity_t *e)
{
	model_t *clmodel = e->model;
	vec3_t mins, maxs;
	VectorAdd(e->origin, clmodel->mins, mins);
	VectorAdd(e->origin, clmodel->maxs, maxs);

	if (R_CullBox(mins, maxs))
		return;

    float light;
	if (!strcmp(clmodel->name, "progs/flame2.mdl") || !strcmp(clmodel->name, "progs/flame.mdl"))
		light = 256; // HACK HACK HACK -- no fullbright colors, so make torches full light
    else
    {
        light = R_LightPoint(e->origin);

        // allways give the gun some light
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
	c_alias_polys += paliashdr->numtris;

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

	GL_DisableMultitexture();

	oglwPushMatrix();
	R_RotateForEntity(e);

	if (!strcmp(clmodel->name, "progs/eyes.mdl") && gl_doubleeyes.value)
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
        int texture = paliashdr->gl_texturenum[e->skinnum][anim];
        // we can't dynamically colormap textures, so they are cached
        // seperately for the players.  Heads are just uncolored.
        if (e->colormap != vid.colormap && !gl_nocolors.value)
        {
            int i = e - cl_entities;
            if (i >= 1 && i <= cl.maxclients /* && !strcmp (e->model->name, "progs/player.mdl") */)
                texture = playertextures - 1 + i;
        }
        GL_Bind(texture);
    }

	if (gl_smoothmodels.value)
		oglwEnableSmoothShading(true);
	oglwSetTextureBlending(0, GL_MODULATE);

    #if defined(EGLW_GLES1)
	if (gl_affinemodels.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    #endif

	GL_DrawAliasFrame(e, paliashdr, pose, pose, 0.0f, light);

	oglwSetTextureBlending(0, GL_REPLACE);

    oglwEnableSmoothShading(false);
    #if defined(EGLW_GLES1)
	if (gl_affinemodels.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    #endif

	oglwPopMatrix();

	if (r_shadows.value)
	{
		oglwPushMatrix();
		R_RotateForEntity(e);
		oglwEnableTexturing(0, false);
        oglwEnableBlending(true);
		GL_DrawAliasShadow(e, paliashdr, pose, pose, 0.0f);
		oglwEnableTexturing(0, true);
        oglwEnableBlending(false);
		oglwPopMatrix();
	}
}

static void R_DrawEntitiesOnList()
{
	if (!r_drawentities.value)
		return;

	// draw sprites seperately, because of alpha blending
	for (int i = 0; i < cl_numvisedicts; i++)
	{
        entity_t *e = cl_visedicts[i];
		currententity = e;
		switch (e->model->type)
		{
		case mod_alias:
			R_DrawAliasModel(e);
			break;

		case mod_brush:
			R_DrawBrushModel(e);
			break;

		default:
			break;
		}
	}

	for (int i = 0; i < cl_numvisedicts; i++)
	{
        entity_t *e = cl_visedicts[i];
		currententity = e;
		switch (e->model->type)
		{
		case mod_sprite:
			R_DrawSpriteModel(e);
			break;
		}
	}
}

static void R_DrawViewModel()
{
	if (!r_drawviewmodel.value)
		return;

	if (chase_active.value)
		return;

	if (envmap)
		return;

	if (!r_drawentities.value)
		return;

	if (cl.items & IT_INVISIBILITY)
		return;

	if (cl.stats[STAT_HEALTH] <= 0)
		return;

    entity_t *e = &cl.viewent;
	currententity = e;
	if (!e->model)
		return;

	int j = R_LightPoint(e->origin);
	if (j < 24)
		j = 24; // allways give some light on gun
	int ambientlight = j;
	int shadelight = j;

	// add dynamic lights
	for (int lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
        dlight_t *dl = &cl_dlights[lnum];
		if (!dl->radius || dl->die < cl.time)
			continue;

        vec3_t dist;
		VectorSubtract(e->origin, dl->origin, dist);
		float add = dl->radius - Length(dist);
		if (add > 0)
			ambientlight += add;
	}

	float ambient[4], diffuse[4];
	ambient[0] = ambient[1] = ambient[2] = ambient[3] = (float)ambientlight / 128;
	diffuse[0] = diffuse[1] = diffuse[2] = diffuse[3] = (float)shadelight / 128;

	// hack the depth range to prevent view model from poking into walls
	oglwSetDepthRange(gldepthmin, gldepthmin + 0.3f * (gldepthmax - gldepthmin));
	R_DrawAliasModel(e);
	oglwSetDepthRange(gldepthmin, gldepthmax);
}

void R_PolyBlend()
{
	if (!gl_polyblend.value)
		return;

	if (!v_blend[3])
		return;

	GL_DisableMultitexture();

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

int SignbitsForPlane(mplane_t *out)
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

void R_SetFrustum()
{
	int i;

	if (r_refdef.fov_x == 90)
	{
		// front side is visible

		VectorAdd(vpn, vright, frustum[0].normal);
		VectorSubtract(vpn, vright, frustum[1].normal);

		VectorAdd(vpn, vup, frustum[2].normal);
		VectorSubtract(vpn, vup, frustum[3].normal);
	}
	else
	{
		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector(frustum[0].normal, vup, vpn, -(90 - r_refdef.fov_x / 2));
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector(frustum[1].normal, vup, vpn, 90 - r_refdef.fov_x / 2);
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector(frustum[2].normal, vright, vpn, 90 - r_refdef.fov_y / 2);
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector(frustum[3].normal, vright, vpn, -(90 - r_refdef.fov_y / 2));
	}

	for (i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane(&frustum[i]);
	}
}

void R_SetupFrame()
{
	// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
		Cvar_Set("r_fullbright", "0");

	R_AnimateLight();

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy(r_refdef.vieworg, r_origin);

	AngleVectors(r_refdef.viewangles, vpn, vright, vup);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf(r_origin, cl.worldmodel);

	V_SetContentsColor(r_viewleaf->contents);
	V_CalcBlend();

	c_brush_polys = 0;
	c_alias_polys = 0;
}

void MYgluPerspective(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar)
{
	GLfloat xmin, xmax, ymin, ymax;

	ymax = zNear * tan(fovy * Q_PI / 360.0f);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	oglwFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

void R_SetupGL()
{
	extern int glwidth, glheight;

	// set up viewpoint
	oglwMatrixMode(GL_PROJECTION);
	oglwLoadIdentity();
	int x = r_refdef.vrect.x * glwidth / vid.width;
	int x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth / vid.width;
	int y = (vid.height - r_refdef.vrect.y) * glheight / vid.height;
	int y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight / vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < glheight)
		y++;

	int w = x2 - x;
	int h = y - y2;

	if (envmap)
	{
		x = y2 = 0;
		w = h = 256;
	}

	oglwSetViewport(glx + x, gly + y2, w, h);
	float screenaspect = (float)r_refdef.vrect.width / r_refdef.vrect.height;
	//	yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/Q_PI;
	MYgluPerspective(r_refdef.fov_y, screenaspect, 4, 4096);

	if (mirror)
	{
		if (mirror_plane->normal[2])
			oglwScale(1, -1, 1);
		else
			oglwScale(-1, 1, 1);
		glCullFace(GL_BACK);
	}
	else
		glCullFace(GL_FRONT);

	oglwMatrixMode(GL_MODELVIEW);
	oglwLoadIdentity();

	oglwRotate(-90, 1, 0, 0); // put Z going up
	oglwRotate(90, 0, 0, 1); // put Z going up
	oglwRotate(-r_refdef.viewangles[2], 1, 0, 0);
	oglwRotate(-r_refdef.viewangles[0], 0, 1, 0);
	oglwRotate(-r_refdef.viewangles[1], 0, 0, 1);
	oglwTranslate(-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);

	oglwGetMatrix(GL_MODELVIEW, r_world_matrix);

	//
	// set drawing parms
	//
	if (gl_cull.value)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

    oglwEnableBlending(false);
    oglwEnableAlphaTest(false);
	oglwEnableDepthTest(true);
    oglwEnableDepthWrite(true);

    oglwSetCurrentTextureUnitForced(0);
    oglwEnableTexturing(0, true);
	oglwSetTextureBlending(0, GL_REPLACE);
}

/*
   r_refdef must be set before the first call
 */
void R_RenderScene()
{
	R_SetupFrame();

	R_SetFrustum();

	R_SetupGL();

	R_MarkLeaves(); // done here so we know if we're in water

	R_DrawWorld(); // adds static entities to the list

	S_ExtraUpdate(); // don't let sound get messed up if going slow

	R_DrawEntitiesOnList();

	GL_DisableMultitexture();

	R_RenderDlights();

	R_DrawParticles();

	#ifdef GLTEST
	Test_Draw();
	#endif
}

void R_Clear()
{
    GLuint flags = 0;
    GLenum depthFunc;

	if (r_mirroralpha.value != 1.0f)
	{
		if (gl_clear.value)
			flags = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
		else
			flags = GL_DEPTH_BUFFER_BIT;
		gldepthmin = 0;
		gldepthmax = 0.5f;
		depthFunc = GL_LEQUAL;
	}
	else if (gl_ztrick.value)
	{
		static int trickframe;

		if (gl_clear.value)
			flags = GL_COLOR_BUFFER_BIT;

		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999f;
			depthFunc = GL_LEQUAL;
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5f;
			depthFunc = GL_GEQUAL;
		}
	}
	else
	{
		if (gl_clear.value)
			flags = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
		else
			flags = GL_DEPTH_BUFFER_BIT;
		gldepthmin = 0;
		gldepthmax = 1;
		depthFunc = GL_LEQUAL;
	}

    if (flags != 0)
        oglwClear(flags);
    glDepthFunc(depthFunc);
	oglwSetDepthRange(gldepthmin, gldepthmax);
}

void R_Mirror()
{
	float d;
	msurface_t *s;
	entity_t *ent;

	if (!mirror)
		return;

	memcpy(r_base_world_matrix, r_world_matrix, sizeof(r_base_world_matrix));

	d = DotProduct(r_refdef.vieworg, mirror_plane->normal) - mirror_plane->dist;
	VectorMA(r_refdef.vieworg, -2 * d, mirror_plane->normal, r_refdef.vieworg);

	d = DotProduct(vpn, mirror_plane->normal);
	VectorMA(vpn, -2 * d, mirror_plane->normal, vpn);

	r_refdef.viewangles[0] = -asinf(vpn[2]) / Q_PI * 180;
	r_refdef.viewangles[1] = atan2f(vpn[1], vpn[0]) / Q_PI * 180;
	r_refdef.viewangles[2] = -r_refdef.viewangles[2];

	ent = &cl_entities[cl.viewentity];
	if (cl_numvisedicts < MAX_VISEDICTS)
	{
		cl_visedicts[cl_numvisedicts] = ent;
		cl_numvisedicts++;
	}

	gldepthmin = 0.5f;
	gldepthmax = 1;
	oglwSetDepthRange(gldepthmin, gldepthmax);
	glDepthFunc(GL_LEQUAL);

	R_RenderScene();
	R_DrawWaterSurfaces();

	gldepthmin = 0;
	gldepthmax = 0.5f;
	oglwSetDepthRange(gldepthmin, gldepthmax);
	glDepthFunc(GL_LEQUAL);

	// blend on top
    oglwEnableBlending(true);
	oglwMatrixMode(GL_PROJECTION);
	if (mirror_plane->normal[2])
		oglwScale(1, -1, 1);
	else
		oglwScale(-1, 1, 1);
	glCullFace(GL_FRONT);
	oglwMatrixMode(GL_MODELVIEW);

	oglwLoadMatrix(r_base_world_matrix);

	s = cl.worldmodel->textures[mirrortexturenum]->texturechain;
	for (; s; s = s->texturechain)
		R_RenderBrushPoly(s, r_mirroralpha.value);
	cl.worldmodel->textures[mirrortexturenum]->texturechain = NULL;
    oglwEnableBlending(false);
}

void R_RenderView()
{
	if (r_norefresh.value)
		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error("R_RenderView: NULL worldmodel");

	mirror = false;

	R_Clear();

	// render normal view
	R_RenderScene();
	R_DrawViewModel();
	R_DrawWaterSurfaces();

	// render mirror view
	R_Mirror();

	R_PolyBlend();
}
