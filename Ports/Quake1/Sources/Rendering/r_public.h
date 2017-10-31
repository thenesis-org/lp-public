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
// Public interface to rendering functions

#ifndef r_public_h
#define r_public_h

#include "Common/common.h"
#include "Common/mathlib.h"
#include "Rendering/r_video.h"
void V_CalcBlend();

#define BACKFACE_EPSILON 0.01f

#define TOP_RANGE 16 // soldier uniform colors
#define BOTTOM_RANGE 96

typedef struct efrag_s
{
	struct mleaf_s *leaf;
	struct efrag_s *leafnext;
	struct entity_s *entity;
	struct efrag_s *entnext;
} efrag_t;

typedef struct
{
	vec3_t origin;
	vec3_t angles;
	int modelindex;
	int frame;
	int colormap;
	int skin;
	int effects;
} entity_state_t;

typedef struct entity_s
{
	qboolean forcelink; // model changed

	int update_type;

	entity_state_t baseline; // to fill in defaults in updates

	double msgtime; // time of last update
	vec3_t msg_origins[2]; // last two updates (0 is newest)
	vec3_t origin;
	vec3_t msg_angles[2]; // last two updates (0 is newest)
	vec3_t angles;
	struct model_s *model; // NULL = no model
	struct efrag_s *efrag; // linked list of efrags
	int frame;
	float syncbase; // for client-side animations
	byte *colormap;
	int effects; // light, particals, etc
	int skinnum; // for Alias models
	int visframe; // last frame this entity was
	//  found in an active leaf

	int dlightframe; // dynamic lighting
	int dlightbits;

	// FIXME: could turn these into a union
	int trivial_accept;
	struct mnode_s *topnode; // for bmodels, first world node
	//  that splits bmodel, or NULL if
	//  not split
} entity_t;

typedef struct
{
	vrect_t vrect; // subwindow in video for refresh
	// FIXME: not need vrect next field here?
	vrect_t aliasvrect; // scaled Alias version
	int vrectright, vrectbottom; // right & bottom screen coords
	int aliasvrectright, aliasvrectbottom; // scaled Alias versions
	float vrectrightedge; // rightmost right edge we care about,
	//  for use in edge list
	float fvrectx, fvrecty; // for floating-point compares
	float fvrectx_adj, fvrecty_adj; // left and top edges, for clamping
	int vrect_x_adj_shift20; // (vrect.x + 0.5 - epsilon) << 20
	int vrectright_adj_shift20; // (vrectright + 0.5 - epsilon) << 20
	float fvrectright_adj, fvrectbottom_adj;
	// right and bottom edges, for clamping
	float fvrectright; // rightmost edge, for Alias clamping
	float fvrectbottom; // bottommost edge, for Alias clamping
	float horizontalFieldOfView; // at Z = 1.0, this many X is visible
	// 2.0 = 90 degrees
	float xOrigin; // should probably allways be 0.5
	float yOrigin; // between be around 0.3 to 0.5

	vec3_t vieworg;
	vec3_t viewangles;

	float fov_x, fov_y;

	int ambientlight;
} refdef_t;

extern refdef_t r_refdef;
extern vec3_t r_viewOrigin, r_viewRight, r_viewUp, r_viewZ;

void R_Init();
void R_initTextures();

void R_Texture_clearAll();

void R_beginNewMap();

void R_setupFrame();
void R_renderView(); // must set r_refdef first

//--------------------------------------------------------------------------------
// Entities.
//--------------------------------------------------------------------------------
void R_Efrag_add(entity_t *ent);
void R_Efrag_remove(entity_t *ent);

//--------------------------------------------------------------------------------
// Particles.
//--------------------------------------------------------------------------------
void R_EntityParticles(entity_t *ent);
void R_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count);
void R_ParseParticleEffect();
void R_RocketTrail(vec3_t start, vec3_t end, int type);
void R_BlobExplosion(vec3_t org);
void R_ParticleExplosion(vec3_t org);
void R_ParticleExplosion2(vec3_t org, int colorStart, int colorLength);
void R_LavaSplash(vec3_t org);
void R_TeleportSplash(vec3_t org);

#endif
