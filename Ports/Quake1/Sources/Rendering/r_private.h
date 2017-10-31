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

#ifndef r_private_h
#define r_private_h

#include "Common/common.h"
#include "Common/cvar.h"
#include "Rendering/r_model.h"

#include "OpenGLES/OpenGLWrapper.h"

//--------------------------------------------------------------------------------
// Cvars.
//--------------------------------------------------------------------------------
extern cvar_t r_fullscreen;

extern cvar_t r_disabled;
extern cvar_t r_draw_world;
extern cvar_t r_draw_entities;
extern cvar_t r_draw_viewmodel;
extern cvar_t r_novis;

extern cvar_t r_clear;
extern cvar_t r_ztrick;
extern cvar_t r_zfix;

extern cvar_t r_cull;

extern cvar_t r_lightmap_disabled;
extern cvar_t r_lightmap_only;
extern cvar_t r_lightmap_dynamic;
extern cvar_t r_lightmap_backface_lighting;
extern cvar_t r_lightmap_upload_full;
extern cvar_t r_lightmap_upload_delayed;
extern cvar_t r_lightmap_mipmap;
extern cvar_t r_lightflash;

extern cvar_t r_tjunctions_keep;
extern cvar_t r_tjunctions_report;
extern cvar_t r_sky_subdivision;
extern cvar_t r_water_subdivision;
extern cvar_t r_texture_sort;

extern cvar_t r_meshmodel_shadow;
extern cvar_t r_meshmodel_shadow_stencil;
extern cvar_t r_meshmodel_smooth_shading;
extern cvar_t r_meshmodel_affine_filtering;
extern cvar_t r_meshmodel_double_eyes;

extern cvar_t r_player_downsampling;
extern cvar_t r_player_nocolors;

extern cvar_t r_fullscreenfx;

extern cvar_t r_texture_maxsize;

//--------------------------------------------------------------------------------
// Graphic context.
//--------------------------------------------------------------------------------
extern const GLubyte *gl_vendor;
extern const GLubyte *gl_renderer;
extern const GLubyte *gl_version;
extern const GLubyte *gl_extensions;

extern bool r_stencilAvailable;

//--------------------------------------------------------------------------------
// Global rendering.
//--------------------------------------------------------------------------------
void R_beginRendering(int *x, int *y, int *width, int *height);
void R_endRendering();

//--------------------------------------------------------------------------------
// Textures.
//--------------------------------------------------------------------------------
extern texture_t *r_notexture_mip;

bool R_Texture_load(void *data, int width, int height, bool mipmap, bool alpha, int downsampling, const unsigned *palette);
unsigned int R_Texture_create(char *identifier, int width, int height, void *data, bool mipmap, bool alpha, bool downsampling, bool paletted);
void R_TranslatePlayerSkin(int playernum);

//--------------------------------------------------------------------------------
// Viewport.
//--------------------------------------------------------------------------------
extern int r_viewportX, r_viewportY, r_viewportWidth, r_viewportHeight;

//--------------------------------------------------------------------------------
// 2D.
//--------------------------------------------------------------------------------
void R_setup2D();

//--------------------------------------------------------------------------------
// 3D view.
//--------------------------------------------------------------------------------
extern float r_world_matrix[16];
extern vec3_t modelorg;

//--------------------------------------------------------------------------------
// 3D statistics.
//--------------------------------------------------------------------------------
extern int r_spriteCount, r_aliasModelCount, r_brushModelCount;

extern int r_surfaceTextureCount;

extern int r_surfaceTotalCount, r_surfaceBaseCount, r_surfaceLightmappedCount, r_surfaceMultitexturedCount;
extern int r_surfaceUnderwaterTotalCount, r_surfaceUnderWaterBaseCount, r_surfaceUnderWaterLightmappedCount, r_surfaceUnderWaterMultitexturedCount;
extern int r_surfaceWaterCount;
extern int r_surfaceSkyCount;
extern int r_surfaceGlobalCount;
extern int r_surfaceBaseTotalCount;
extern int r_surfaceLightmappedTotalCount;
extern int r_surfaceMultitexturedTotalCount;

extern int r_surfaceTotalPolyCount, r_surfaceBasePolyCount, r_surfaceLightmappedPolyCount, r_surfaceMultitexturedPolyCount;
extern int r_surfaceUnderWaterTotalPolyCount, r_surfaceUnderWaterBasePolyCount, r_surfaceUnderWaterLightmappedPolyCount, r_surfaceUnderWaterMultitexturedPolyCount;
extern int r_surfaceWaterPolyCount;
extern int r_surfaceSkyPolyCount;
extern int r_surfaceGlobalPolyCount;
extern int r_surfaceBaseTotalPolyCount;
extern int r_surfaceLightmappedTotalPolyCount;
extern int r_surfaceMultitexturedTotalPolyCount;

extern int r_aliasModelPolyCount;

//--------------------------------------------------------------------------------
// Surfaces.
//--------------------------------------------------------------------------------
void R_Surface_subdivide(model_t *model, msurface_t *surface, float subdivisionSize);

// called whenever r_refdef or vid change
void R_Sky_init(struct texture_s *mt); // called at level load

//--------------------------------------------------------------------------------
// Particles.
//--------------------------------------------------------------------------------
extern cvar_t r_particles_min_size;
extern cvar_t r_particles_max_size;
extern cvar_t r_particles_size;
extern cvar_t r_particles_att_a;
extern cvar_t r_particles_att_b;
extern cvar_t r_particles_att_c;

void R_Particles_initialize();
void R_Particles_clear();
void R_Particles_draw();

#endif
