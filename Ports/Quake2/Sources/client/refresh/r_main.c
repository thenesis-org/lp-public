#include "client/client.h"
#include "client/keyboard.h"
#include "client/refresh/r_private.h"

glconfig_t gl_config;
glstate_t gl_state;
bool r_stencilAvailable = false;
bool r_msaaAvailable = false;

viddef_t viddef; /* global video state; used by other modules */

image_t *r_notexture; /* use for bad textures */
image_t *r_particletexture; // little dot for particles.

float gldepthmin, gldepthmax;
cplane_t frustum[4];

/* view origin */
vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t r_origin;

float r_world_matrix[16];
float r_base_world_matrix[16];

/* screen size info */
refdef_t r_newrefdef;

int r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;
unsigned r_rawpalette[256];

static int r_visframecount; /* bumped when going to a new PVS */
static int r_framecount; /* used for dlight push checking */

int c_brush_polys, c_alias_polys;

float v_blend[4]; /* final blending color */

static entity_t r_worldEntity;
model_t *r_worldmodel;

cvar_t *r_window_width;
cvar_t *r_window_height;
cvar_t *r_window_x;
cvar_t *r_window_y;
cvar_t *r_fullscreen;
cvar_t *r_fullscreen_width;
cvar_t *r_fullscreen_height;
cvar_t *r_fullscreen_bitsPerPixel;
cvar_t *r_fullscreen_frequency;
cvar_t *r_msaa_samples;
cvar_t *r_gamma;

cvar_t *r_norefresh;
cvar_t *r_discardframebuffer;
cvar_t *gl_clear;
cvar_t *gl_ztrick;
cvar_t *gl_zfix;
cvar_t *gl_swapinterval;

cvar_t *r_texture_retexturing;
cvar_t *r_texture_alphaformat;
cvar_t *r_texture_solidformat;
cvar_t *r_texture_rounddown;
cvar_t *r_texture_scaledown;
cvar_t *r_texture_filter;
cvar_t *r_texture_anisotropy;
cvar_t *r_texture_anisotropy_available;

cvar_t *gl_stereo;
cvar_t *gl_stereo_separation;
cvar_t *gl_stereo_convergence;
cvar_t *gl_stereo_anaglyph_colors;

cvar_t *gl_farsee;
cvar_t *gl_drawentities;
cvar_t *gl_drawworld;
cvar_t *gl_speeds;

cvar_t *gl_novis;
cvar_t *gl_lockpvs;
cvar_t *gl_nocull;
cvar_t *gl_cull;

cvar_t *gl_lerpmodels;
cvar_t *gl_lefthand;
cvar_t *gl_lightlevel;
cvar_t *gl_shadows;
cvar_t *gl_stencilshadow;

cvar_t *gl_particle_min_size;
cvar_t *gl_particle_max_size;
cvar_t *gl_particle_size;
cvar_t *gl_particle_att_a;
cvar_t *gl_particle_att_b;
cvar_t *gl_particle_att_c;
#if defined(EGLW_GLES1)
cvar_t *gl_particle_point;
cvar_t *gl_particle_sprite;
#endif

cvar_t *r_nobind;
cvar_t *r_multitexturing;
cvar_t *r_lightmap_mipmap;
cvar_t *r_lightmap_dynamic;
cvar_t *r_lightmap_backface_lighting;
cvar_t *r_lightmap_disabled;
cvar_t *r_lightmap_only;
cvar_t *r_lightmap_saturate;
cvar_t *gl_modulate;
cvar_t *r_lightmap_outline;
cvar_t *r_subdivision;

cvar_t *r_fullscreenflash;
cvar_t *r_lightflash;

cvar_t *gl_hudscale; /* named for consistency with R1Q2 */
cvar_t *gl_consolescale;
cvar_t *gl_menuscale;

//********************************************************************************
// Global.
//********************************************************************************
//--------------------------------------------------------------------------------
// EGL and OpenGL ES error.
//--------------------------------------------------------------------------------
void Gles_checkEglError()
{
	EGLint error = eglGetError();
	if (error && error != EGL_SUCCESS)
		R_printf(PRINT_ALL, "EGL Error: 0x%04x\n", (int)error);
}

void Gles_checkGlesError()
{
	GLenum error = glGetError();
	if (error != GL_NO_ERROR)
		R_printf(PRINT_ALL, "GLES Error: 0x%04x\n", (int)error);
}

//--------------------------------------------------------------------------------
// Tools.
//--------------------------------------------------------------------------------
// Returns true if the box is completely outside the frustom
bool R_CullBox(vec3_t mins, vec3_t maxs)
{
	if (gl_nocull->value)
		return false;

	for (int i = 0; i < 4; i++)
	{
		if (BOX_ON_PLANE_SIDE(mins, maxs, &frustum[i]) == 2)
			return true;
	}

	return false;
}

void R_Entity_rotate(entity_t *e)
{
	oglwTranslate(e->origin[0], e->origin[1], e->origin[2]);
	oglwRotate(e->angles[1], 0, 0, 1);
	oglwRotate(-e->angles[0], 0, 1, 0);
	oglwRotate(-e->angles[2], 1, 0, 0);
}

static void R_SetDefaultState()
{
	glCullFace(GL_FRONT);
	glDisable(GL_CULL_FACE);

	oglwEnableSmoothShading(false);

	oglwEnableTexturing(0, true);

	R_TextureMode(r_texture_filter->string);
	R_TextureAlphaMode(r_texture_alphaformat->string);
	R_TextureSolidMode(r_texture_solidformat->string);

	oglwSetTextureBlending(0, GL_REPLACE);

	#if defined(EGLW_GLES1)
	{
		float attenuations[3];

		attenuations[0] = gl_particle_att_a->value;
		attenuations[1] = gl_particle_att_b->value;
		attenuations[2] = gl_particle_att_c->value;

		/* GL_POINT_SMOOTH is not implemented by some OpenGL
		   drivers, especially the crappy Mesa3D backends like
		   i915.so. That the points are squares and not circles
		   is not a problem by Quake II! */
		glEnable(GL_POINT_SMOOTH);
		glPointParameterf(GL_POINT_SIZE_MIN, gl_particle_min_size->value);
		glPointParameterf(GL_POINT_SIZE_MAX, gl_particle_max_size->value);
		glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION, attenuations);
	}
	#endif

	#if defined(EGLW_GLES1)
	if (r_msaa_samples->value)
		glEnable(GL_MULTISAMPLE);
	else
		glDisable(GL_MULTISAMPLE);
	#endif

	oglwEnableBlending(false);
	oglwSetBlendingFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	oglwEnableAlphaTest(false);
	oglwEnableDepthTest(false);
	oglwEnableStencilTest(false);

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	oglwEnableDepthWrite(true);
	glStencilMask(0xffffffff);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepthf(1.0f);
	glClearStencil(0);
	oglwClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void R_SetPalette(const unsigned char *palette)
{
	byte *rp = (byte *)r_rawpalette;
	if (palette)
	{
		for (int i = 0; i < 256; i++)
		{
			rp[i * 4 + 0] = palette[i * 3 + 0];
			rp[i * 4 + 1] = palette[i * 3 + 1];
			rp[i * 4 + 2] = palette[i * 3 + 2];
			rp[i * 4 + 3] = 0xff;
		}
	}
	else
	{
		for (int i = 0; i < 256; i++)
		{
			unsigned char *pc = d_8to24table[i];
			rp[i * 4 + 0] = pc[0];
			rp[i * 4 + 1] = pc[1];
			rp[i * 4 + 2] = pc[2];
			rp[i * 4 + 3] = 0xff;
		}
	}
}

//********************************************************************************
// Brush model.
//********************************************************************************
int c_visible_lightmaps;
int c_visible_textures;
static vec3_t modelorg; /* relative to viewpoint */

static msurface_t *r_alpha_surfaces[2];
static image_t * usedTextureList; // List of textures used by world and current entity.

gllightmapstate_t gl_lightmapState;

//--------------------------------------------------------------------------------
// Dynamic lighting.
//--------------------------------------------------------------------------------
#define DLIGHT_CUTOFF 64

static void R_DynamicLighting_drawLight(dlight_t *light)
{
	OglwVertex *vtx = oglwAllocateTriangleFan(1 + 16 + 1);
    if (!vtx)
        return;

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

void R_DynamicLighting_draw()
{
	if (!r_lightflash->value)
		return;

	oglwEnableBlending(true);
	oglwSetBlendingFunction(GL_ONE, GL_ONE);
	oglwEnableDepthWrite(false);

	oglwEnableSmoothShading(true);
	oglwEnableTexturing(0, GL_FALSE);

	oglwBegin(GL_TRIANGLES);
	dlight_t *l = r_newrefdef.dlights;
	for (int i = 0; i < r_newrefdef.num_dlights; i++, l++)
		R_DynamicLighting_drawLight(l);
	oglwEnd();

	oglwEnableSmoothShading(false);
	oglwEnableTexturing(0, GL_TRUE);

	oglwEnableBlending(false);
	oglwSetBlendingFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	oglwEnableDepthWrite(true);
}

static void R_DynamicLighting_markLights(dlight_t *light, int bit, mnode_t *node)
{
	if (node->contents != -1)
		return;

    {
        cplane_t *splitplane = node->plane;
        float dist = DotProduct(light->origin, splitplane->normal) - splitplane->dist;
        if (dist > light->intensity - DLIGHT_CUTOFF)
        {
            R_DynamicLighting_markLights(light, bit, node->children[0]);
            return;
        }
        if (dist < -light->intensity + DLIGHT_CUTOFF)
        {
            R_DynamicLighting_markLights(light, bit, node->children[1]);
            return;
        }
    }

	msurface_t *surface = r_worldmodel->surfaces + node->firstsurface;
	for (int i = 0; i < node->numsurfaces; i++, surface++)
	{
        if (!r_lightmap_backface_lighting->value)
        {
            // This avoids to light backfacing surfaces but this still does not handle occlusion.
            // As a result this does not always look better because one backfacing surface is not lit whereas a nearby one is, and no shadow are cast.
            // With backfacing surfaces lit, it fakes global illumination, which looks nicer.
            // But it may be faster with this enabled because less surfaces are lit.
            cplane_t *surfacePlane = surface->plane;
            float dist = DotProduct(light->origin, surfacePlane->normal) - surfacePlane->dist;
            int sidebit;
            if (dist >= 0)
                sidebit = 0;
            else
                sidebit = SURF_PLANEBACK;
            if ((surface->flags & SURF_PLANEBACK) != sidebit)
                continue;
        }

        int dlightframecount = r_framecount + 1; // because the count hasn't advanced yet for this frame
		if (surface->dlightframe != dlightframecount)
		{
			surface->dlightbits = 0;
			surface->dlightframe = dlightframecount;
		}

		surface->dlightbits |= bit;
	}

	R_DynamicLighting_markLights(light, bit, node->children[0]);
	R_DynamicLighting_markLights(light, bit, node->children[1]);
}

void R_DynamicLighting_push()
{
    if (!r_lightmap_dynamic->value)
        return;
	dlight_t *l = r_newrefdef.dlights;
	for (int i = 0; i < r_newrefdef.num_dlights; i++, l++)
		R_DynamicLighting_markLights(l, 1 << i, r_worldmodel->nodes);
}

//--------------------------------------------------------------------------------
// Lightmap.
//--------------------------------------------------------------------------------
static float r_lightmap_block[34 * 34 * 3];
static lightstyle_t r_lightmap_styles[MAX_LIGHTSTYLES];

static int R_Lighmap_lightPointR(mnode_t *node, vec3_t start, vec3_t end, vec3_t color, vec3_t lightSpot, cplane_t **lightPlane)
{
	if (node->contents != -1)
		return -1; /* didn't hit anything */

	/* calculate mid point */
	cplane_t *plane = node->plane;
	float front = DotProduct(start, plane->normal) - plane->dist;
	float back = DotProduct(end, plane->normal) - plane->dist;
	int side = front < 0;

	if ((back < 0) == side)
		return R_Lighmap_lightPointR(node->children[side], start, end, color, lightSpot, lightPlane);

	float frac = front / (front - back);
	vec3_t mid;
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

	/* go down front side */
	int r = R_Lighmap_lightPointR(node->children[side], start, mid, color, lightSpot, lightPlane);
	if (r >= 0)
		return r; /* hit something */

	if ((back < 0) == side)
		return -1; /* didn't hit anuthing */

	/* check for impact on this node */
	VectorClear(color);
	VectorCopy(mid, lightSpot);
	*lightPlane = plane;

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

		byte *lightmap = surf->samples;
		if (!lightmap)
			return 0;

        ds >>= 4;
        dt >>= 4;
        lightmap += 3 * (dt * ((surf->extents[0] >> 4) + 1) + ds);
        float modulate = gl_modulate->value * (1.0f / 255);
        for (int maps = 0; maps < MAXLIGHTMAPS; maps++)
        {
            int style = surf->styles[maps];
            if (style == 255)
                break;
            float *styleRgb = r_newrefdef.lightstyles[style].rgb;
            for (i = 0; i < 3; i++)
                color[i] += lightmap[i] * modulate * styleRgb[i];
            lightmap += 3 * ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1);
        }
		return 1;
	}

	/* go down back side */
	return R_Lighmap_lightPointR(node->children[!side], mid, end, color, lightSpot, lightPlane);
}

void R_Lighmap_lightPoint(entity_t *e, vec3_t p, vec3_t color, vec3_t lightSpot, cplane_t **lightPlane)
{
	if (!r_worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 1.0f;
        VectorClear(lightSpot);
        *lightPlane = NULL;
		return;
	}

	vec3_t end;
	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	int r = R_Lighmap_lightPointR(r_worldmodel->nodes, p, end, color, lightSpot, lightPlane);
    if (r < 0)
    {
        // Nothing was hit.
        VectorClear(color);
        VectorClear(lightSpot);
        *lightPlane = NULL;
    }

	// add dynamic lights
    int lightNb = r_newrefdef.num_dlights;
	dlight_t *dl = r_newrefdef.dlights;
	for (int lightIndex = 0; lightIndex < lightNb; lightIndex++, dl++)
	{
        vec3_t dist;
		VectorSubtract(e->origin, dl->origin, dist);
		float add = dl->intensity - VectorLength(dist);
		add *= (1.0f / 256);
		if (add > 0)
			VectorMA(color, add, dl->color, color);
	}

	VectorScale(color, gl_modulate->value, color);
}

static void R_Lighmap_addDynamicLights(msurface_t *surf)
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
		float *pfBL = r_lightmap_block;
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

static void R_Lightmap_setCacheState(msurface_t *surf)
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
static void R_Lightmap_build(msurface_t *surf, byte *dest, int stride)
{
	if (surf->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP))
		R_error(ERR_DROP, "R_Lightmap_build called for non-lit surface");

	int smax = (surf->extents[0] >> 4) + 1;
	int tmax = (surf->extents[1] >> 4) + 1;
	int size = smax * tmax;
	if (size > (int)(sizeof(r_lightmap_block) >> 4))
		R_error(ERR_DROP, "Bad r_lightmap_block size");

	/* set to full bright if no light data */
	if (!surf->samples)
	{
        float *bl = r_lightmap_block;
		for (int i = 0; i < size * 3; i++)
			bl[i] = 255;
	}
    else
	{
		// Count the number of maps.
		int nummaps;
		for (nummaps = 0; nummaps < MAXLIGHTMAPS; nummaps++)
        {
            if (surf->styles[nummaps] == 255)
                break;
        }

		byte *lightmap = surf->samples;

		/* add all the lightmaps */
		if (nummaps == 1)
		{
            float modulate = gl_modulate->value;
            float scale[3];
            for (int i = 0; i < 3; i++)
                scale[i] = modulate * r_newrefdef.lightstyles[surf->styles[0]].rgb[i];

            float *bl = r_lightmap_block;
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
			memset(r_lightmap_block, 0, sizeof(r_lightmap_block[0]) * size * 3);
			for (int maps = 0; maps < nummaps; maps++)
			{
                float scale[3];
				for (int i = 0; i < 3; i++)
					scale[i] = gl_modulate->value * r_newrefdef.lightstyles[surf->styles[maps]].rgb[i];

				float *bl = r_lightmap_block;
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
			R_Lighmap_addDynamicLights(surf);
	}

	stride -= (smax << 2);
	float *bl = r_lightmap_block;
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

static void R_Lightmap_initializeBlock()
{
	memset(gl_lightmapState.allocated, 0, sizeof(gl_lightmapState.allocated));
}

void R_Lightmap_upload(qboolean dynamic)
{
	oglwSetCurrentTextureUnitForced(0);

	if (dynamic)
	{
        // Dynamic textures are used in a round robin way, even inter frames. Otherwise, if a texture already in use for rendering is updated, it causes huge slowdowns with tile / deferred rendering platforms.
        int texture = gl_lightmapState.dynamicLightmapCurrent;
        gl_lightmapState.dynamicLightmapCurrent = (texture + 1) % LIGHTMAP_DYNAMIC_MAX_NB;
		int height = 0;
		for (int i = 0; i < LIGHTMAP_WIDTH; i++)
		{
            int allocated = gl_lightmapState.allocated[i];
			if (height < allocated)
				height = allocated;
		}
        oglwSetCurrentTextureUnitForced(0);
        oglwBindTextureForced(0, gl_state.lightmap_textures + LIGHTMAP_STATIC_MAX_NB + texture);
        R_Texture_upload(gl_lightmapState.lightmap_buffer, 0, 0, LIGHTMAP_WIDTH, height, false, false, false);
	}
	else
	{
        int texture = gl_lightmapState.staticLightmapNb;
		if (texture >= LIGHTMAP_STATIC_MAX_NB)
		{
			R_error(ERR_DROP, "R_Lightmap_upload() - LIGHTMAP_STATIC_MAX_NB exceeded\n");
            return;
		}
        else
            gl_lightmapState.staticLightmapNb = texture + 1;
        bool mipmapFlag = r_lightmap_mipmap->value != 0;
        oglwSetCurrentTextureUnitForced(0);
        oglwBindTextureForced(0, gl_state.lightmap_textures + texture);
        R_Texture_upload(gl_lightmapState.lightmap_buffer, 0, 0, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, true, false, mipmapFlag);
	}
}

/*
 * returns a texture number and the position inside it
 */
static bool R_Lightmap_allocateBlock(int w, int h, short *x, short *y)
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

void R_Lightmap_buildPolygonFromSurface(model_t *model, msurface_t *fa)
{
	/* reconstruct the polygon */
	medge_t *pedges = model->edges;
	int lnumverts = fa->numedges;

	vec3_t total;
	VectorClear(total);

	/* draw texture */
	glpoly_t *poly = Hunk_Alloc(sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (int i = 0; i < lnumverts; i++)
	{
        float *vec;
		int lindex = model->surfedges[fa->firstedge + i];
		if (lindex > 0)
		{
			medge_t *r_pedge = &pedges[lindex];
			vec = model->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			medge_t *r_pedge = &pedges[-lindex];
			vec = model->vertexes[r_pedge->v[1]].position;
		}

		float s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->image->width;

		float t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
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

void R_Lightmap_createSurface(msurface_t *surf)
{
	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	int smax = (surf->extents[0] >> 4) + 1;
	int tmax = (surf->extents[1] >> 4) + 1;

	if (!R_Lightmap_allocateBlock(smax, tmax, &surf->light_s, &surf->light_t))
	{
		R_Lightmap_upload(false);
		R_Lightmap_initializeBlock();
		if (!R_Lightmap_allocateBlock(smax, tmax, &surf->light_s, &surf->light_t))
			R_error(ERR_FATAL, "Consecutive calls to R_Lightmap_allocateBlock(%d,%d) failed\n", smax, tmax);
	}

	surf->lightmaptexturenum = gl_lightmapState.staticLightmapNb;

	R_Lightmap_setCacheState(surf);
	byte *base = gl_lightmapState.lightmap_buffer + (surf->light_t * LIGHTMAP_WIDTH + surf->light_s) * 4;
	R_Lightmap_build(surf, base, LIGHTMAP_WIDTH * 4);
}

void R_Lightmap_beginBuilding(model_t *m)
{
	memset(gl_lightmapState.allocated, 0, sizeof(gl_lightmapState.allocated));

	r_framecount = 1; /* no dlightcache */

	/* setup the base lightstyles so the lightmaps
	   won't have to be regenerated the first time
	   they're seen */
	for (int i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		r_lightmap_styles[i].rgb[0] = 1;
		r_lightmap_styles[i].rgb[1] = 1;
		r_lightmap_styles[i].rgb[2] = 1;
		r_lightmap_styles[i].white = 3;
	}

	r_newrefdef.lightstyles = r_lightmap_styles;

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

void R_Lightmap_endBuilding()
{
	R_Lightmap_upload(false);
}

static void R_Lightmap_setup(msurface_t *surf, bool immediate)
{
    if ((surf->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)))
        return; // These surfaces do not have lightmap.

	bool updateNeeded = false;

	// check for lightmap modification
	int map;
	for (map = 0; map < MAXLIGHTMAPS; map++)
	{
        int style = surf->styles[map];
        if (style == 255)
            break;
		if (r_newrefdef.lightstyles[style].white != surf->cached_light[map])
        {
			updateNeeded = true;
            break;
        }
	}

	// dynamic this frame or dynamic previously
	if (surf->dlightframe == r_framecount)
		updateNeeded = true;

	unsigned lightmapIndex;
	if (updateNeeded)
	{
		int smax = (surf->extents[0] >> 4) + 1;
		int tmax = (surf->extents[1] >> 4) + 1;

		bool buildLightmap = false;
		if ((surf->styles[map] >= 32 || surf->styles[map] == 0) && (surf->dlightframe != r_framecount))
		{
			buildLightmap = true;
			R_Lightmap_setCacheState(surf);
			lightmapIndex = surf->lightmaptexturenum;
		}
		else
			lightmapIndex = LIGHTMAP_STATIC_MAX_NB + gl_lightmapState.dynamicLightmapCurrent;

		byte *base = gl_lightmapState.lightmap_buffer;
		if (immediate || buildLightmap)
			R_Lightmap_build(surf, (void *)base, smax * 4);
		if (immediate)
		{
			oglwSetCurrentTextureUnitForced(1);
			oglwBindTextureForced(1, gl_state.lightmap_textures + lightmapIndex);
            R_Texture_upload(base, surf->light_s, surf->light_t, smax, tmax, false, false, false);
			oglwSetCurrentTextureUnit(0);
		}
	}
	else
	{
		lightmapIndex = surf->lightmaptexturenum;
		if (immediate)
			oglwBindTexture(1, gl_state.lightmap_textures + lightmapIndex);
	}
	if (!immediate)
	{
        if (lightmapIndex < LIGHTMAP_STATIC_MAX_NB)
        {
            msurface_t *lightmapSurfaces = gl_lightmapState.lightmapSurfaces[lightmapIndex];
            if (lightmapSurfaces == NULL)
            {
                int staticLightmapNbInFrame = gl_lightmapState.staticLightmapNbInFrame;
                gl_lightmapState.staticLightmapSurfacesInFrame[staticLightmapNbInFrame] = lightmapIndex;
                gl_lightmapState.staticLightmapNbInFrame = staticLightmapNbInFrame + 1;
            }
            surf->lightmapchain = lightmapSurfaces;
            gl_lightmapState.lightmapSurfaces[lightmapIndex] = surf;
        }
        else
        {
            msurface_t *lightmapSurfaces = gl_lightmapState.lightmapSurfaces[LIGHTMAP_STATIC_MAX_NB];
            surf->lightmapchain = lightmapSurfaces;
            gl_lightmapState.lightmapSurfaces[LIGHTMAP_STATIC_MAX_NB] = surf;
        }
	}
}

static void R_Lightmap_drawChainStatic(model_t *model, float alpha)
{
    int staticLightmapNbInFrame = gl_lightmapState.staticLightmapNbInFrame;
	if (model == r_worldmodel)
			c_visible_lightmaps += staticLightmapNbInFrame;
	for (int i = 0; i < staticLightmapNbInFrame; i++)
	{
        int textureIndex = gl_lightmapState.staticLightmapSurfacesInFrame[i];
        msurface_t *surf = gl_lightmapState.lightmapSurfaces[textureIndex];

		oglwBindTexture(0, gl_state.lightmap_textures + textureIndex);
		oglwBegin(GL_TRIANGLES);
        do
        {
            glpoly_t *p = surf->polys;
            if (p)
            {
                for (; p != 0; p = p->chain)
                {
                    float *v = p->verts[0];
                    if (v == NULL)
                        break;
                    OglwVertex *vtx = oglwAllocateTriangleFan(p->numverts);
                    if (vtx == NULL)
                        continue;
                    for (int j = 0; j < p->numverts; j++, v += VERTEXSIZE)
                        vtx = AddVertex3D_CT1(vtx, v[0], v[1], v[2], 1.0f, 1.0f, 1.0f, alpha, v[5], v[6]);
                }
            }
            surf = surf->lightmapchain;
        } while (surf);
        oglwEnd();
	}
}

static void R_Lightmap_drawChainDynamic(model_t *model, float alpha)
{
    msurface_t *batchFirstSurf = gl_lightmapState.lightmapSurfaces[LIGHTMAP_STATIC_MAX_NB];
    while (batchFirstSurf != NULL)
    {
        if (model == r_worldmodel)
            c_visible_lightmaps++;

        if (gl_lightmapState.dynamicLightmapNbInFrame == LIGHTMAP_DYNAMIC_MAX_NB)
            R_printf(PRINT_ALL, "Too many dynamic textures used in the same frame, performance may drop on some targets.\n");
        gl_lightmapState.dynamicLightmapNbInFrame++;

        // Clear the current lightmap.
        R_Lightmap_initializeBlock();

        // Add as many blocks as possible in the current lightmap.
        msurface_t *surf;
        for (surf = batchFirstSurf; surf != 0; surf = surf->lightmapchain)
        {
            // Find a place in the current lightmap.
            int smax = (surf->extents[0] >> 4) + 1;
            int tmax = (surf->extents[1] >> 4) + 1;
            if (!R_Lightmap_allocateBlock(smax, tmax, &surf->dlight_s, &surf->dlight_t))
            {
                // The block does not fit in the current lightmap. Draw all surfaces already done and try again with an empty lightmap.
                break;
            }

            // Build the light map for this surface.
            byte *base = gl_lightmapState.lightmap_buffer + (surf->dlight_t * LIGHTMAP_WIDTH + surf->dlight_s) * 4;
            R_Lightmap_build(surf, base, LIGHTMAP_WIDTH * 4);
        }

        if (batchFirstSurf == surf)
        {
            // There is not enough room in the lightmap for even a single block.
            break;
        }

        int texture = gl_lightmapState.dynamicLightmapCurrent; // Must be before R_Lightmap_upload().

        // Load the current lightmap in texture memory.
        R_Lightmap_upload(true);

        // Draw the surfaces for this lightmap.
        oglwBindTextureForced(0, gl_state.lightmap_textures + LIGHTMAP_STATIC_MAX_NB + texture);
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
                        break;
                    int n = p->numverts;
                    OglwVertex *vtx = oglwAllocateTriangleFan(n);
                    if (vtx == NULL)
                        continue;
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

// This routine takes all the given light mapped surfaces in the world and blends them into the framebuffer.
static void R_Lightmap_drawChain(model_t *model, float alpha)
{
	/* don't bother if we're set to fullbright */
	if (r_lightmap_disabled->value)
		return;
	if (!r_worldmodel->lightdata)
		return;

	/* set the appropriate blending mode unless
	   we're only looking at the lightmaps. */
	if (!r_lightmap_only->value)
	{
		oglwEnableBlending(true);
		if (r_lightmap_saturate->value)
			oglwSetBlendingFunction(GL_ONE, GL_ONE);
		else
			oglwSetBlendingFunction(GL_ZERO, GL_SRC_COLOR);
		oglwEnableDepthWrite(false);
	}

	if (model == r_worldmodel)
		c_visible_lightmaps = 0;

	R_Lightmap_drawChainStatic(model, alpha); // render static lightmaps first
	if (r_lightmap_dynamic->value)
		R_Lightmap_drawChainDynamic(model, alpha); // render dynamic lightmaps

	oglwEnableBlending(false);
	oglwSetBlendingFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	oglwEnableDepthWrite(true);
}

static void R_Lightmap_drawOutlines()
{
	if (!r_lightmap_outline->value)
		return;

	oglwEnableTexturing(0, GL_FALSE);
	oglwEnableDepthTest(false);

	oglwBegin(GL_LINES);
	for (int i = 0; i < LIGHTMAP_SURFACE_MAX_NB; i++)
	{
		for (msurface_t *surf = gl_lightmapState.lightmapSurfaces[i]; surf != 0; surf = surf->lightmapchain)
		{
			for (glpoly_t *p = surf->polys; p; p = p->chain)
			{
				for (int j = 2; j < p->numverts; j++)
				{
					OglwVertex *vtx = oglwAllocateLineLoop(3);
                    if (vtx == NULL)
                        break;
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

//--------------------------------------------------------------------------------
// Water.
//--------------------------------------------------------------------------------
static const float r_turbsin[] =
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

static void R_Surface_boundPoly(int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int i, j;
	float *v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;

	for (i = 0; i < numverts; i++)
	{
		for (j = 0; j < 3; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
	}
}

static void R_Surface_subdividePolygon(msurface_t *surface, int numverts, float *verts, float subdivisionSize)
{
	if (numverts > 60)
	{
		R_error(ERR_DROP, "numverts = %i", numverts);
	}

	vec3_t mins, maxs;
	R_Surface_boundPoly(numverts, verts, mins, maxs);

	vec3_t front[64], back[64];
	for (int i = 0; i < 3; i++)
	{
		float m = (mins[i] + maxs[i]) * 0.5f;
		m = subdivisionSize * floorf(m / subdivisionSize + 0.5f);

		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		/* cut it */
		float *v = verts + i;

        float dist[64];
        int j;
		for (j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;
		dist[j] = dist[0]; // wrap cases

		v -= i;
		VectorCopy(verts, v);

		int f = 0, b = 0;
		v = verts;
		for (j = 0; j < numverts; j++, v += 3)
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

			if ((dist[j] == 0) || (dist[j + 1] == 0))
				continue;

			if ((dist[j] > 0) != (dist[j + 1] > 0))
			{
				/* clip point */
				float frac = dist[j] / (dist[j] - dist[j + 1]);
				for (int k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}

		R_Surface_subdividePolygon(surface, f, front[0], subdivisionSize);
		R_Surface_subdividePolygon(surface, b, back[0], subdivisionSize);
		return;
	}

	/* add a point in the center to help keep warp valid */
	glpoly_t *poly = Hunk_Alloc(sizeof(glpoly_t) + ((numverts - 4) + 2) * VERTEXSIZE * sizeof(float));
	poly->next = surface->polys;
	surface->polys = poly;
	poly->numverts = numverts + 2;
	vec3_t total;
	VectorClear(total);
	float total_s = 0, total_t = 0;
	for (int i = 0; i < numverts; i++, verts += 3)
	{
		VectorCopy(verts, poly->verts[i + 1]);
		float s = DotProduct(verts, surface->texinfo->vecs[0]);
		float t = DotProduct(verts, surface->texinfo->vecs[1]);
		total_s += s;
		total_t += t;
		VectorAdd(total, verts, total);
		poly->verts[i + 1][3] = s;
		poly->verts[i + 1][4] = t;
	}

	VectorScale(total, (1.0f / numverts), poly->verts[0]);
	poly->verts[0][3] = total_s / numverts;
	poly->verts[0][4] = total_t / numverts;

	/* copy first vertex to last */
	memcpy(poly->verts[numverts + 1], poly->verts[1], sizeof(poly->verts[0]));
}

/*
 * Breaks a polygon up along axial 64 unit
 * boundaries so that turbulent and sky warps
 * can be done reasonably.
 */
void R_Surface_subdivide(model_t *model, msurface_t *surface, float subdivisionSize)
{
	vec3_t verts[64];

    int n = surface->numedges;
    if (n > 64)
        return;

	/* convert edges back to a normal polygon */
	for (int i = 0; i < n; i++)
	{
        float *vec;
		int lindex = model->surfedges[surface->firstedge + i];
		if (lindex > 0)
			vec = model->vertexes[model->edges[lindex].v[0]].position;
		else
			vec = model->vertexes[model->edges[-lindex].v[1]].position;
		VectorCopy(vec, verts[i]);
	}

	R_Surface_subdividePolygon(surface, n, verts[0], subdivisionSize);
}

/*
 * Does a water warp on the pre-fragmented glpoly_t chain
 */
void R_Water_draw(msurface_t *fa, float intensity, float alpha)
{
	float rdt = r_newrefdef.time;
    float TURBSCALE  = (256.0f / (2 * Q_PI));

	float scroll;
	if (fa->texinfo->flags & SURF_FLOWING)
	{
		float t = rdt * 0.5f;
		scroll = -64 * (t - (int)t);
	}
	else
	{
		scroll = 0;
	}

	for (glpoly_t *p = fa->polys; p; p = p->next)
	{
		int n = p->numverts;
		OglwVertex *vtx = oglwAllocateTriangleFan(n);
        if (vtx == NULL)
            continue;
		float *v = p->verts[0];
		for (int i = 0; i < n; i++, v += VERTEXSIZE)
		{
			float os = v[3];
			float ot = v[4];

			float ts = os + r_turbsin[(int)((ot * 0.125f + rdt) * TURBSCALE) & 255] * 0.5f;
			ts += scroll;
			ts *= (1.0f / 64);

			float tt = ot + r_turbsin[(int)((os * 0.125f + rdt) * TURBSCALE) & 255] * 0.5f;
			tt *= (1.0f / 64);

			vtx = AddVertex3D_CT1(vtx, v[0], v[1], v[2], intensity, intensity, intensity, alpha, ts, tt);
		}
	}
}

//--------------------------------------------------------------------------------
// Sky.
//--------------------------------------------------------------------------------
#define MAX_CLIP_VERTS 64
#define ON_EPSILON 0.1f /* point on plane side epsilon */

static char r_sky_name[MAX_QPATH];
static float r_sky_rotate;
static vec3_t r_sky_axis;
static image_t *r_sky_images[6];

static const char r_sky_texOrder[6] = { 0, 2, 1, 3, 4, 5 };

// 3dstudio environment map names
static const char *r_sky_suffix[6] = { "rt", "bk", "lf", "ft", "up", "dn" };

static const vec3_t r_sky_clip[6] =
{
	{ 1, 1, 0 },
	{ 1, -1, 0 },
	{ 0, -1, 1 },
	{ 0, 1, 1 },
	{ 1, 0, 1 },
	{ -1, 0, 1 }
};

int c_sky;

static const signed char r_sky_st_to_vec[6][3] =
{
	{ 3, -1, 2 },
	{ -3, 1, 2 },

	{ 1, 3, 2 },
	{ -1, -3, 2 },

	{ -2, -1, 3 }, /* 0 degrees yaw, look straight up */
	{ 2, -1, -3 } /* look straight down */
};

static const signed char r_sky_vec_to_st[6][3] =
{
	{ -2, 3, 1 },
	{ 2, 3, -1 },

	{ 1, 3, 2 },
	{ -1, 3, -2 },

	{ -2, -1, 3 },
	{ -2, 1, -3 }
};

static float r_sky_mins[2][6], r_sky_maxs[2][6];
static float r_sky_min, r_sky_max;

static void R_Sky_update(int nump, vec3_t vecs)
{
	int i, j;
	vec3_t v, av;
	float s, t, dv;

	c_sky++;

	/* decide which face it maps to */
	VectorClear(v);

	float *vp;
	for (i = 0, vp = vecs; i < nump; i++, vp += 3)
		VectorAdd(vp, v, v);

	av[0] = fabsf(v[0]);
	av[1] = fabsf(v[1]);
	av[2] = fabsf(v[2]);

	int axis;
	if ((av[0] > av[1]) && (av[0] > av[2]))
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if ((av[1] > av[2]) && (av[1] > av[0]))
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	/* project new texture coords */
	for (i = 0; i < nump; i++, vecs += 3)
	{
		j = r_sky_vec_to_st[axis][2];

		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];

		if (dv < 0.001f)
			continue; /* don't divide by zero */

		j = r_sky_vec_to_st[axis][0];

		if (j < 0)
			s = -vecs[-j - 1] / dv;
		else
			s = vecs[j - 1] / dv;

		j = r_sky_vec_to_st[axis][1];

		if (j < 0)
			t = -vecs[-j - 1] / dv;
		else
			t = vecs[j - 1] / dv;

		if (s < r_sky_mins[0][axis])
			r_sky_mins[0][axis] = s;

		if (t < r_sky_mins[1][axis])
			r_sky_mins[1][axis] = t;

		if (s > r_sky_maxs[0][axis])
			r_sky_maxs[0][axis] = s;

		if (t > r_sky_maxs[1][axis])
			r_sky_maxs[1][axis] = t;
	}
}

static void R_Sky_clipPolygon(int nump, vec3_t vecs, int stage)
{
	if (nump > MAX_CLIP_VERTS - 2)
		R_error(ERR_DROP, "R_Sky_clipPolygon: MAX_CLIP_VERTS");

	if (stage == 6)
	{
		/* fully clipped, so draw it */
		R_Sky_update(nump, vecs);
		return;
	}

	bool front = false, back = false;
	const float *norm = r_sky_clip[stage];
	float dists[MAX_CLIP_VERTS];
	int sides[MAX_CLIP_VERTS];
	int i;
	float *v;
	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		float d = DotProduct(v, norm);
		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{
		/* not clipped */
		R_Sky_clipPolygon(nump, vecs, stage + 1);
		return;
	}

	/* clip it */
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy(vecs, (vecs + (i * 3)));

	int newc[2];
	newc[0] = newc[1] = 0;
	vec3_t newv[2][MAX_CLIP_VERTS];

	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy(v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy(v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy(v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy(v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if ((sides[i] == SIDE_ON) || (sides[i + 1] == SIDE_ON) || (sides[i + 1] == sides[i]))
			continue;

		float d = dists[i] / (dists[i] - dists[i + 1]);

		for (int j = 0; j < 3; j++)
		{
			float e = v[j] + d * (v[j + 3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}

		newc[0]++;
		newc[1]++;
	}

	/* continue */
	R_Sky_clipPolygon(newc[0], newv[0][0], stage + 1);
	R_Sky_clipPolygon(newc[1], newv[1][0], stage + 1);
}

void R_Sky_addSurface(msurface_t *fa)
{
	vec3_t verts[MAX_CLIP_VERTS];

	/* calculate vertex values for sky box */
	glpoly_t *p;
	for (p = fa->polys; p; p = p->next)
	{
        int n = p->numverts;
		for (int i = 0; i < n; i++)
		{
			VectorSubtract(p->verts[i], r_origin, verts[i]);
		}
		R_Sky_clipPolygon(p->numverts, verts[0], 0);
	}
}

void R_Sky_clearBox()
{
	for (int i = 0; i < 6; i++)
	{
		r_sky_mins[0][i] = r_sky_mins[1][i] = 9999;
		r_sky_maxs[0][i] = r_sky_maxs[1][i] = -9999;
	}
}

static OglwVertex* R_Sky_drawBoxVertex(OglwVertex *vtx, float s, float t, int axis)
{
	vec3_t v, b;

	if (gl_farsee->value == 0)
	{
		b[0] = s * 2300;
		b[1] = t * 2300;
		b[2] = 2300;
	}
	else
	{
		b[0] = s * 4096;
		b[1] = t * 4096;
		b[2] = 4096;
	}

	for (int j = 0; j < 3; j++)
	{
		int k = r_sky_st_to_vec[axis][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
	}

	/* avoid bilerp seam */
	s = (s + 1) * 0.5f;
	t = (t + 1) * 0.5f;

	if (s < r_sky_min)
		s = r_sky_min;
	if (s > r_sky_max)
		s = r_sky_max;

	if (t < r_sky_min)
		t = r_sky_min;
	if (t > r_sky_max)
		t = r_sky_max;
	t = 1.0f - t;

	return AddVertex3D_T1(vtx, v[0], v[1], v[2], s, t);
}

void R_Sky_drawBox()
{
	int i;

	if (r_sky_rotate)
	{ /* check for no sky at all */
		for (i = 0; i < 6; i++)
		{
			if ((r_sky_mins[0][i] < r_sky_maxs[0][i]) && (r_sky_mins[1][i] < r_sky_maxs[1][i]))
				break;
		}
		if (i == 6)
			return; /* nothing visible */
	}

	oglwPushMatrix();
	oglwTranslate(r_origin[0], r_origin[1], r_origin[2]);
	oglwRotate(r_newrefdef.time * r_sky_rotate, r_sky_axis[0], r_sky_axis[1], r_sky_axis[2]);

	for (i = 0; i < 6; i++)
	{
		if (r_sky_rotate)
		{
			r_sky_mins[0][i] = -1;
			r_sky_mins[1][i] = -1;
			r_sky_maxs[0][i] = 1;
			r_sky_maxs[1][i] = 1;
		}

		if ((r_sky_mins[0][i] >= r_sky_maxs[0][i]) || (r_sky_mins[1][i] >= r_sky_maxs[1][i]))
			continue;

		oglwBindTexture(0, r_sky_images[(int)r_sky_texOrder[i]]->texnum);
		oglwBegin(GL_TRIANGLES);
		OglwVertex *vtx = oglwAllocateQuad(4);
        if (vtx != NULL)
        {
            vtx = R_Sky_drawBoxVertex(vtx, r_sky_mins[0][i], r_sky_mins[1][i], i);
            vtx = R_Sky_drawBoxVertex(vtx, r_sky_mins[0][i], r_sky_maxs[1][i], i);
            vtx = R_Sky_drawBoxVertex(vtx, r_sky_maxs[0][i], r_sky_maxs[1][i], i);
            vtx = R_Sky_drawBoxVertex(vtx, r_sky_maxs[0][i], r_sky_mins[1][i], i);
        }
		oglwEnd();
	}

	oglwPopMatrix();
}

void R_Sky_set(char *name, float rotate, vec3_t axis)
{
	Q_strlcpy(r_sky_name, name, sizeof(r_sky_name));
	r_sky_rotate = rotate;
	VectorCopy(axis, r_sky_axis);

	for (int i = 0; i < 6; i++)
	{
        char pathname[MAX_QPATH];
		Com_sprintf(pathname, sizeof(pathname), "env/%s%s.tga", r_sky_name, r_sky_suffix[i]);

		r_sky_images[i] = R_FindImage(pathname, it_sky);
		if (!r_sky_images[i])
			r_sky_images[i] = r_notexture;

		r_sky_min = 1.0f / 512;
		r_sky_max = 511.0f / 512;
	}
}

//--------------------------------------------------------------------------------
// Surface.
//--------------------------------------------------------------------------------
// Returns the proper texture for a given time and base texture
static image_t* R_Surface_getAnimatedTexture(entity_t *e, mtexinfo_t *tex)
{
	if (tex->next)
	{
		int c = e->frame % tex->numframes;
		while (c)
		{
			tex = tex->next;
			c--;
		}
	}
	return tex->image;
}

//--------------------------------------------------------------------------------
// Surface rendering without sorting.
//--------------------------------------------------------------------------------
static void R_Surface_drawBase(msurface_t *surf, float intensity, float alpha)
{
	float scroll = 0.0f;
	if (surf->texinfo->flags & SURF_FLOWING)
	{
		float t = r_newrefdef.time * (1.0f / 40.0f);
		scroll = -64 * (t - (int)t);
		if (scroll == 0.0f)
			scroll = -64.0f;
	}

	for (glpoly_t *p = surf->polys; p; p = p->chain)
	{
        int n = p->numverts;
        OglwVertex *vtx = oglwAllocateTriangleFan(n);
        if (vtx == NULL)
            continue;
        float *v = p->verts[0];
        for (int i = 0; i < n; i++, v += VERTEXSIZE)
            vtx = AddVertex3D_CT1(vtx, v[0], v[1], v[2], intensity, intensity, intensity, alpha, (v[3] + scroll), v[4]);
    }
}

static void R_Surface_drawMultitextured(msurface_t *surf, float intensity, float alpha)
{
	float scroll = 0.0f;
	if (surf->texinfo->flags & SURF_FLOWING)
	{
		float t = r_newrefdef.time * (1.0f / 40.0f);
		scroll = -64 * (t - (int)t);
		if (scroll == 0.0f)
			scroll = -64.0f;
	}

	for (glpoly_t *p = surf->polys; p; p = p->chain)
	{
		int nv = p->numverts;
		OglwVertex *vtx = oglwAllocateTriangleFan(nv);
        if (vtx == NULL)
            continue;
		float *v = p->verts[0];
		for (int i = 0; i < nv; i++, v += VERTEXSIZE)
			vtx = AddVertex3D_CT2(vtx, v[0], v[1], v[2], intensity, intensity, intensity, alpha, (v[3] + scroll), v[4], v[5], v[6]);
	}
}

static void R_Surface_drawTranslucent(msurface_t *surf, float intensity, float alpha)
{
	if (surf->texinfo->flags & SURF_TRANS33)
		alpha *= 0.33f;
	else if (surf->texinfo->flags & SURF_TRANS66)
		alpha *= 0.66f;

	if (surf->flags & SURF_DRAWTURB)
		R_Water_draw(surf, intensity, alpha);
	else
		R_Surface_drawBase(surf, intensity, alpha);
}

//--------------------------------------------------------------------------------
// Surface rendering with sorting.
//--------------------------------------------------------------------------------
static void R_Surface_chain(entity_t *e, msurface_t *surf)
{
	image_t *image = R_Surface_getAnimatedTexture(e, surf->texinfo);
	if (!image->used)
	{
		image->used = 1;
		image->image_chain_node = usedTextureList;
		usedTextureList = image;
	}
	surf->texturechain = image->texturechain;
	image->texturechain = surf;
}

static void R_Surface_drawChain(float alpha, int chain)
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
					R_Surface_drawBase(s, 1.0f, alpha);
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
					R_Water_draw(s, intensity, alpha);
				}
			}
			oglwEnd();
		}
		image_t *imageNext = image->image_chain_node;
		image->image_chain_node = NULL;
		image->texturechain = NULL;
		image->used = false;
		image = imageNext;
	}

	usedTextureList = NULL;

	oglwSetTextureBlending(0, GL_REPLACE);
}

//--------------------------------------------------------------------------------
// Alpha surfaces.
//--------------------------------------------------------------------------------
static void R_Surface_chainAlpha(entity_t *e, msurface_t *surf, int alpha, int chain)
{
	surf->texturechain = r_alpha_surfaces[chain];
    surf->alpha = alpha;
	r_alpha_surfaces[chain] = surf;
	image_t *image = R_Surface_getAnimatedTexture(e, surf->texinfo);
	surf->current_image = image;
}

/*
 * Draw water surfaces and windows.
 * The BSP tree is walked front to back, so unwinding the chain
 * of alpha_surfaces will draw back to front, giving proper ordering.
 */
void R_Surface_drawChainAlpha(float alpha, int chain)
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
		R_Surface_drawTranslucent(surf, intensity, alpha);
	}
	oglwEnd();

	oglwSetTextureBlending(0, GL_REPLACE);

	oglwEnableBlending(false);
	oglwEnableDepthWrite(true);

	r_alpha_surfaces[chain] = NULL;
}

//--------------------------------------------------------------------------------
// Surface.
//--------------------------------------------------------------------------------
static void R_Surface_draw(entity_t *entity, msurface_t *surf, bool immediate, float alpha, int chain)
{
    bool alphaFlag = alpha < 1.0f || (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) != 0;
    R_Lightmap_setup(surf, immediate && !alphaFlag);
    if (alphaFlag)
        R_Surface_chainAlpha(entity, surf, alpha, chain);
    else if (immediate)
    {
        c_brush_polys++;
        image_t *image = R_Surface_getAnimatedTexture(entity, surf->texinfo);
        if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
        {
            oglwEnableBlending(true);
            oglwEnableDepthWrite(false);
            oglwEnableTexturing(1, GL_FALSE);
            oglwBindTexture(0, image->texnum);
            oglwBegin(GL_TRIANGLES);
            R_Surface_drawTranslucent(surf, gl_state.inverse_intensity, alpha);
            oglwEnd();
            oglwEnableTexturing(1, GL_TRUE);
            oglwEnableBlending(false);
            oglwEnableDepthWrite(true);            
        }
        else if (!(surf->flags & SURF_DRAWTURB))
        {
            oglwBindTexture(0, image->texnum);
            oglwBegin(GL_TRIANGLES);
            R_Surface_drawMultitextured(surf, 1.0f, alpha);
            oglwEnd();            
        }
        else
        {
            oglwEnableTexturing(1, GL_FALSE);
            oglwBindTexture(0, image->texnum);
            oglwBegin(GL_TRIANGLES);
            R_Water_draw(surf, gl_state.inverse_intensity, alpha);
            oglwEnd();
            oglwEnableTexturing(1, GL_TRUE);
        }
    }
    else
        R_Surface_chain(entity, surf);
}

//--------------------------------------------------------------------------------
// Brush model.
//--------------------------------------------------------------------------------
static void R_BrushModel_drawPolys(entity_t *entity, float alpha)
{
    model_t *model = entity->model;
    
	/* calculate dynamic lighting for bmodel */
	if (!r_lightflash->value)
	{
		dlight_t *lt = r_newrefdef.dlights;
		for (int k = 0; k < r_newrefdef.num_dlights; k++, lt++)
			R_DynamicLighting_markLights(lt, 1 << k, model->nodes + model->firstnode);
	}

	qboolean multitexturing = r_multitexturing->value != 0;

	msurface_t *psurf = &model->surfaces[model->firstmodelsurface];
	for (int i = 0; i < model->nummodelsurfaces; i++, psurf++)
	{
		/* find which side of the node we are on */
		cplane_t *pplane = psurf->plane;
		float dot = DotProduct(modelorg, pplane->normal) - pplane->dist;

		/* draw the polygon */
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
			R_Surface_draw(entity, psurf, multitexturing, alpha, 1);
	}
}

static void R_BrushModel_drawBegin()
{
	usedTextureList = NULL;
    gl_lightmapState.staticLightmapNbInFrame = 0;
    gl_lightmapState.dynamicLightmapNbInFrame = 0;
	memset(gl_lightmapState.lightmapSurfaces, 0, sizeof(gl_lightmapState.lightmapSurfaces));

	oglwSetTextureBlending(0, GL_MODULATE);

	if (r_multitexturing->value != 0 && !r_lightmap_disabled->value)
	{
		oglwEnableTexturing(1, GL_TRUE);
		if (r_lightmap_only->value)
			oglwSetTextureBlending(1, GL_REPLACE);
		else
			oglwSetTextureBlending(1, GL_MODULATE);
	}
}

static void R_BrushModel_drawEnd()
{
	oglwSetTextureBlending(0, GL_REPLACE);
	oglwEnableTexturing(1, GL_FALSE);
	oglwSetTextureBlending(1, GL_REPLACE);
}

void R_BrushModel_draw(entity_t *entity)
{
    model_t *model = entity->model;
	if (model->nummodelsurfaces == 0)
		return;

	qboolean rotated;
	vec3_t mins, maxs;
	if (entity->angles[0] || entity->angles[1] || entity->angles[2])
	{
		rotated = true;
		for (int i = 0; i < 3; i++)
		{
			mins[i] = entity->origin[i] - model->radius;
			maxs[i] = entity->origin[i] + model->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd(entity->origin, model->mins, mins);
		VectorAdd(entity->origin, model->maxs, maxs);
	}

	if (R_CullBox(mins, maxs))
		return;

	VectorSubtract(r_newrefdef.vieworg, entity->origin, modelorg);

	if (rotated)
	{
		vec3_t temp;
		vec3_t forward, right, up;

		VectorCopy(modelorg, temp);
		AngleVectors(entity->angles, forward, right, up);
		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}

	if (gl_zfix->value)
		glEnable(GL_POLYGON_OFFSET_FILL);

	oglwPushMatrix();
	entity->angles[0] = -entity->angles[0];
	entity->angles[2] = -entity->angles[2];
	R_Entity_rotate(entity);
	entity->angles[0] = -entity->angles[0];
	entity->angles[2] = -entity->angles[2];

	float alpha = 1.0f;
	if (entity->flags & RF_TRANSLUCENT)
		alpha = entity->alpha;

	R_BrushModel_drawBegin();
	R_BrushModel_drawPolys(entity, alpha);
	R_BrushModel_drawEnd();
	R_Surface_drawChain(alpha, 1);
	R_Lightmap_drawChain(model, alpha);
    R_Surface_drawChainAlpha(alpha, 1);

	oglwPopMatrix();

	if (gl_zfix->value)
		glDisable(GL_POLYGON_OFFSET_FILL);
}

//--------------------------------------------------------------------------------
// World.
//--------------------------------------------------------------------------------
/*
 * Mark the leaves and nodes that are
 * in the PVS for the current cluster
 */
void R_World_markLeaves()
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
		return;

	r_visframecount++;
	r_oldviewcluster = r_viewcluster;
	r_oldviewcluster2 = r_viewcluster2;

	if (gl_novis->value || (r_viewcluster == -1) || !r_worldmodel->vis)
	{
		/* mark everything */
		for (i = 0; i < r_worldmodel->numleafs; i++)
			r_worldmodel->leafs[i].visframe = r_visframecount;
		for (i = 0; i < r_worldmodel->numnodes; i++)
			r_worldmodel->nodes[i].visframe = r_visframecount;
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

	for (i = 0, leaf = r_worldmodel->leafs; i < r_worldmodel->numleafs; i++, leaf++)
	{
		cluster = leaf->cluster;

		if (cluster == -1)
			continue;

		if (vis[cluster >> 3] & (1 << (cluster & 7)))
		{
			node = (mnode_t *)leaf;
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

static void R_World_drawR(entity_t *worldEntity, mnode_t *node)
{
	if (node->contents == CONTENTS_SOLID)
		return; /* solid */

	if (node->visframe != r_visframecount)
		return;

	if (R_CullBox(node->minmaxs, node->minmaxs + 3))
		return;

	/* if a leaf node, draw stuff */
	if (node->contents != -1)
	{
		mleaf_t *pleaf = (mleaf_t *)node;

		/* check for door connected areas */
		if (r_newrefdef.areabits)
		{
			if (!(r_newrefdef.areabits[pleaf->area >> 3] & (1 << (pleaf->area & 7))))
				return; /* not visible */
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
	R_World_drawR(worldEntity, node->children[side]);

	/* draw stuff */
	qboolean multitexturing = r_multitexturing->value != 0;

	{
		int c;
		msurface_t *surf;
		for (c = node->numsurfaces, surf = r_worldmodel->surfaces + node->firstsurface; c; c--, surf++)
		{
			if (surf->visframe != r_framecount)
				continue;

			if ((surf->flags & SURF_PLANEBACK) != sidebit)
				continue; /* wrong side */

			if (surf->texinfo->flags & SURF_SKY)
				R_Sky_addSurface(surf); // Just adds to visible sky bounds.
			else
				R_Surface_draw(worldEntity, surf, multitexturing, 1.0f, 0);
		}
	}

	/* recurse down the back side */
	R_World_drawR(worldEntity, node->children[!side]);
}

void R_World_draw()
{
	if (!gl_drawworld->value)
		return;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	VectorCopy(r_newrefdef.vieworg, modelorg);

	/* auto cycle the world frame for texture animation */
    entity_t *worldEntity = &r_worldEntity;
    model_t *worldModel = r_worldmodel;
	memset(worldEntity, 0, sizeof(entity_t));
    worldEntity->model = worldModel;
	worldEntity->frame = (int)(r_newrefdef.time * 2);

	R_Sky_clearBox();

	R_BrushModel_drawBegin();
	R_World_drawR(worldEntity, worldModel->nodes);
	R_BrushModel_drawEnd();
	R_Surface_drawChain(1.0f, 0);
	R_Lightmap_drawChain(worldModel, 1.0f);

	R_Sky_drawBox();
	R_Lightmap_drawOutlines();
}

//********************************************************************************
// Beam.
//********************************************************************************
#define NUM_BEAM_SEGS 6

static void R_Beam_draw(entity_t *e)
{
	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if (VectorNormalize(normalized_direction) == 0)
		return;

	PerpendicularVector(perpvec, normalized_direction);
	VectorScale(perpvec, e->frame / 2, perpvec);

	for (int i = 0; i < 6; i++)
	{
		RotatePointAroundVector(start_points[i], normalized_direction, perpvec,
			(360.0f / NUM_BEAM_SEGS) * i);
		VectorAdd(start_points[i], origin, start_points[i]);
		VectorAdd(start_points[i], direction, end_points[i]);
	}

    int alpha = 1.0f;
	if (e->flags & RF_TRANSLUCENT)
		alpha = e->alpha;
	if (alpha < 1.0f)
    {
		oglwEnableBlending(true);
		oglwEnableDepthWrite(false);
    }

	oglwEnableTexturing(0, GL_FALSE);

	oglwBegin(GL_TRIANGLES);

	unsigned char *pc = d_8to24table[e->skinnum & 0xff];
	float r = pc[0] * (1 / 255.0f);
	float g = pc[1] * (1 / 255.0f);
	float b = pc[2] * (1 / 255.0f);
	float a = alpha;

	OglwVertex *vtx = oglwAllocateTriangleStrip(NUM_BEAM_SEGS * 4);
    if (vtx)
    {
        for (int i = 0; i < NUM_BEAM_SEGS; i++)
        {
            float *p;
            p = start_points[i];
            vtx = AddVertex3D_C(vtx, p[0], p[1], p[2], r, g, b, a);
            p = end_points[i];
            vtx = AddVertex3D_C(vtx, p[0], p[1], p[2], r, g, b, a);
            p = start_points[(i + 1) % NUM_BEAM_SEGS];
            vtx = AddVertex3D_C(vtx, p[0], p[1], p[2], r, g, b, a);
            p = end_points[(i + 1) % NUM_BEAM_SEGS];
            vtx = AddVertex3D_C(vtx, p[0], p[1], p[2], r, g, b, a);
        }
    }

	oglwEnd();

	oglwEnableTexturing(0, GL_TRUE);

	if (alpha < 1.0f)
    {
		oglwEnableBlending(false);
		oglwEnableDepthWrite(true);
    }
}

//********************************************************************************
// Null model.
//********************************************************************************
static void R_NullModel_draw(entity_t *entity)
{
	vec3_t shadelight;
	if (entity->flags & RF_FULLBRIGHT)
	{
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0F;
	}
	else
	{
        vec3_t lightSpot;
        cplane_t *lightPlane;
		R_Lighmap_lightPoint(entity, entity->origin, shadelight, lightSpot, &lightPlane);
	}

	float alpha = 1.0F;
	if (entity->flags & RF_TRANSLUCENT)
		alpha = entity->alpha;
	if (alpha < 1.0f)
    {
		oglwEnableBlending(true);
		oglwEnableDepthWrite(false);
    }

	oglwPushMatrix();
	R_Entity_rotate(entity);

	oglwEnableTexturing(0, GL_FALSE);

	oglwBegin(GL_TRIANGLES);

	OglwVertex *vtx;

	vtx = oglwAllocateTriangleFan(6);
    if (vtx)
    {
        vtx = AddVertex3D_C(vtx, 0.0f, 0.0f, -16.0f, shadelight[0], shadelight[1], shadelight[2], alpha);
        for (int i = 0; i <= 4; i++)
        {
            vtx = AddVertex3D_C(vtx, 16 * cosf(i * Q_PI / 2), 16 * sinf(i * Q_PI / 2), 0.0f, shadelight[0], shadelight[1], shadelight[2], alpha);
        }
    }

	vtx = oglwAllocateTriangleFan(6);
    if (vtx)
    {
        vtx = AddVertex3D_C(vtx, 0.0f, 0.0f, 16.0f, shadelight[0], shadelight[1], shadelight[2], alpha);
        for (int i = 4; i >= 0; i--)
        {
            vtx = AddVertex3D_C(vtx, 16 * cosf(i * Q_PI / 2), 16 * sinf(i * Q_PI / 2), 0.0f, shadelight[0], shadelight[1], shadelight[2], alpha);
        }
    }
    
	oglwEnd();

	oglwEnableTexturing(0, GL_TRUE);

	oglwPopMatrix();

	if (alpha < 1.0f)
    {
		oglwEnableBlending(false);
		oglwEnableDepthWrite(true);
    }
}

//********************************************************************************
// Sprite model.
//********************************************************************************
static void R_SpriteModel_draw(entity_t *entity)
{
    model_t *model = entity->model;
    
	/* don't even bother culling, because it's just
	   a single polygon without a surface cache */
	dsprite_t *psprite = (dsprite_t *)model->extradata;

	entity->frame %= psprite->numframes;
	dsprframe_t *frame = &psprite->frames[entity->frame];

	/* normal sprite */
	float *up = vup;
	float *right = vright;

	oglwBindTexture(0, model->skins[entity->frame]->texnum);
	oglwSetTextureBlending(0, GL_MODULATE);

	float alpha = 1.0F;
	if (entity->flags & RF_TRANSLUCENT)
		alpha = entity->alpha;
	if (alpha == 1.0f)
		oglwEnableAlphaTest(true);
    else
    {
		oglwEnableBlending(true);
		oglwEnableDepthWrite(false);
    }

	oglwBegin(GL_TRIANGLES);

	OglwVertex *vtx = oglwAllocateQuad(4);

	vec3_t p;

	VectorMA(entity->origin, -frame->origin_y, up, p);
	VectorMA(p, -frame->origin_x, right, p);
	vtx = AddVertex3D_CT1(vtx, p[0], p[1], p[2], 1.0f, 1.0f, 1.0f, alpha, 0.0f, 1.0f);

	VectorMA(entity->origin, frame->height - frame->origin_y, up, p);
	VectorMA(p, -frame->origin_x, right, p);
	vtx = AddVertex3D_CT1(vtx, p[0], p[1], p[2], 1.0f, 1.0f, 1.0f, alpha, 0.0f, 0.0f);

	VectorMA(entity->origin, frame->height - frame->origin_y, up, p);
	VectorMA(p, frame->width - frame->origin_x, right, p);
	vtx = AddVertex3D_CT1(vtx, p[0], p[1], p[2], 1.0f, 1.0f, 1.0f, alpha, 1.0f, 0.0f);

	VectorMA(entity->origin, -frame->origin_y, up, p);
	VectorMA(p, frame->width - frame->origin_x, right, p);
	vtx = AddVertex3D_CT1(vtx, p[0], p[1], p[2], 1.0f, 1.0f, 1.0f, alpha, 1.0f, 1.0f);

	oglwEnd();

	if (alpha == 1.0f)
		oglwEnableAlphaTest(false);
    else
    {
		oglwEnableBlending(false);
		oglwEnableDepthWrite(true);
    }

	oglwSetTextureBlending(0, GL_REPLACE);
}

//********************************************************************************
// Entity.
//********************************************************************************
static void R_Entity_draw(entity_t *entity)
{
	if (entity->flags & RF_BEAM)
	{
		R_Beam_draw(entity);
	}
	else
	{
		model_t *model = entity->model;
		if (!model)
		{
			R_NullModel_draw(entity);
			return;
		}
		switch (model->type)
		{
		case mod_alias:
			R_AliasModel_draw(entity);
			break;
		case mod_brush:
			R_BrushModel_draw(entity);
			break;
		case mod_sprite:
			R_SpriteModel_draw(entity);
			break;
		default:
			R_error(ERR_DROP, "Bad modeltype");
			break;
		}
	}
}

static void R_Entity_drawAll()
{
	if (!gl_drawentities->value)
		return;

	// draw non-transparent first
    int entityNb = r_newrefdef.num_entities;
	entity_t *entities = r_newrefdef.entities;
	for (int entityIndex = 0; entityIndex < entityNb; entityIndex++)
	{
		entity_t *entity = &entities[entityIndex];
		R_Entity_draw(entity);
	}
}

//********************************************************************************
// Particles.
//********************************************************************************
static void R_Particles_initialize()
{
	// Particle texture.
	{
		int particleSize = 16;
		byte *particleData = malloc(particleSize * particleSize * 4);
		float radius = (float)((particleSize >> 1) - 2), radius2 = radius * radius, center = (float)(particleSize >> 1);
		for (int y = 0; y < particleSize; y++)
		{
			for (int x = 0; x < particleSize; x++)
			{
				float rx = (float)x - center, ry = (float)y - center;
				float rd2 = rx * rx + ry * ry;
				float a;
				if (rd2 > radius2)
				{
					a = 1.0f - (sqrtf(rd2) - radius);
					if (a < 0.0f)
						a = 0.0f;
					if (a > 1.0f)
						a = 1.0f;
				}
				else
					a = 1.0f;
				byte *d = &particleData[(x + y * particleSize) * 4];
				d[0] = 255;
				d[1] = 255;
				d[2] = 255;
				d[3] = (byte)(a * 255.0f);
			}
		}
		r_particletexture = R_LoadPic("***particle***", particleData, particleSize, 0, particleSize, 0, it_sprite, 32);
		free(particleData);
	}
}

static float R_Particles_computeSize(float distance2, float distance)
{
	float attenuation = gl_particle_att_a->value;
	float factorB = gl_particle_att_b->value;
	float factorC = gl_particle_att_c->value;
	float particleSize = gl_particle_size->value;
	float particleMinSize = gl_particle_min_size->value;
	float particleMaxSize = gl_particle_max_size->value;

	if (factorB > 0.0f || factorC > 0.0f)
	{
		attenuation += factorC * distance2;
		if (factorB > 0.0f)
		{
			attenuation += factorB * distance;
		}
	}
	float size;
	if (attenuation <= 0.0f)
	{
		size = particleSize;
	}
	else
	{
		attenuation = 1.0f / sqrtf(attenuation);
		size = particleSize * attenuation;
		if (size < particleMinSize)
			size = particleMinSize;
		if (size > particleMaxSize)
			size = particleMaxSize;
	}
	if (size < 1.0f)
		size = 1.0f;

	return size;
}

static void R_Particles_drawWithQuads(int num_particles, const particle_t particles[])
{
	oglwBindTexture(0, r_particletexture->texnum);
	oglwSetTextureBlending(0, GL_MODULATE);

	vec3_t up, right;
	VectorScale(vup, 1.0f, up);
	VectorScale(vright, 1.0f, right);

	oglwBegin(GL_TRIANGLES);
	OglwVertex *vtx = oglwAllocateQuad(num_particles * 4);
    if (vtx)
    {
        float pixelWidthAtDepth1 = 2.0f * tanf(r_newrefdef.fov_x * Q_PI / 360.0f) / (float)r_newrefdef.width; // Pixel width if the near plane is at depth 1.0.

        // The dot is 14 pixels instead of 16 because of borders. Take it into account.
        pixelWidthAtDepth1 *= 16.0f / 14.0f;

        int i;
        const particle_t *p;
        for (p = particles, i = 0; i < num_particles; i++, p++)
        {
            float dx = p->origin[0] - r_origin[0], dy = p->origin[1] - r_origin[1], dz = p->origin[2] - r_origin[2];
            float distance2 = dx * dx + dy * dy + dz * dz;
            float distance = sqrtf(distance2);

            // Size in pixels, like OpenGLES.
            float size = R_Particles_computeSize(distance2, distance);

            // Size in world space.
            size = size * pixelWidthAtDepth1 * distance;

            unsigned char *pc = d_8to24table[p->color & 0xff];
            float r = pc[0] * (1.0f / 255.0f);
            float g = pc[1] * (1.0f / 255.0f);
            float b = pc[2] * (1.0f / 255.0f);
            float a = p->alpha;

            float rx = size * right[0], ry = size * right[1], rz = size * right[2];
            float ux = size * up[0], uy = size * up[1], uz = size * up[2];

            // Center each quad.
            float px = p->origin[0] - 0.5f * (rx + ux);
            float py = p->origin[1] - 0.5f * (ry + uy);
            float pz = p->origin[2] - 0.5f * (rz + uz);

            vtx = AddVertex3D_CT1(vtx, px, py, pz, r, g, b, a, 0.0f, 0.0f);
            vtx = AddVertex3D_CT1(vtx, px + ux, py + uy, pz + uz, r, g, b, a, 1.0f, 0.0f);
            vtx = AddVertex3D_CT1(vtx, px + ux + rx, py + uy + ry, pz + uz + rz, r, g, b, a, 1.0f, 1.0f);
            vtx = AddVertex3D_CT1(vtx, px + rx, py + ry, pz + rz, r, g, b, a, 0.0f, 1.0f);
        }
    }
	oglwEnd();

	oglwSetTextureBlending(0, GL_REPLACE);
}

#if !defined(EGLW_GLES2)
static void R_Particles_drawWithPoints(int num_particles, const particle_t particles[])
{
	bool particleTextureUsed = gl_particle_sprite->value != 0.0f;
	if (particleTextureUsed)
	{
		glEnable(GL_POINT_SPRITE_OES);
		glTexEnvi(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE);
		oglwBindTexture(0, r_particletexture->texnum);
		oglwSetTextureBlending(0, GL_MODULATE);
	}
	else
	{
		oglwEnableTexturing(0, GL_FALSE);
	}

	glPointSize(gl_particle_size->value);

	oglwBegin(GL_POINTS);
	OglwVertex *vtx = oglwAllocateVertex(num_particles);
    if (vtx)
    {
        int i;
        const particle_t *p;
        for (i = 0, p = particles; i < num_particles; i++, p++)
        {
            unsigned char *pc = d_8to24table[p->color & 0xff];
            float r = pc[0] * (1.0f / 255.0f);
            float g = pc[1] * (1.0f / 255.0f);
            float b = pc[2] * (1.0f / 255.0f);
            float a = p->alpha;

            vtx = AddVertex3D_C(vtx, p->origin[0], p->origin[1], p->origin[2], r, g, b, a);
        }
    }

	oglwEnd();

	if (particleTextureUsed)
	{
		glDisable(GL_POINT_SPRITE_OES);
		glTexEnvi(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_FALSE);
		oglwSetTextureBlending(0, GL_REPLACE);
	}
	else
	{
		oglwEnableTexturing(0, GL_TRUE);
	}
}
#endif

static void R_Particles_draw()
{
	int particleNb = r_newrefdef.num_particles;
	if (particleNb <= 0)
		return;

	oglwEnableBlending(true);
	oglwEnableDepthWrite(false);

	#if !defined(EGLW_GLES2)
	qboolean stereo_split_tb = ((gl_state.stereo_mode == STEREO_SPLIT_VERTICAL) && gl_state.camera_separation);
	qboolean stereo_split_lr = ((gl_state.stereo_mode == STEREO_SPLIT_HORIZONTAL) && gl_state.camera_separation);
	if (gl_particle_point->value && !(stereo_split_tb || stereo_split_lr))
	{
		R_Particles_drawWithPoints(particleNb, r_newrefdef.particles);
	}
	else
	#endif
	{
		R_Particles_drawWithQuads(particleNb, r_newrefdef.particles);
	}

	oglwEnableBlending(false);
	oglwEnableDepthWrite(true);
}

//********************************************************************************
// Fullscreen flash.
//********************************************************************************
static void R_FullscreenFlash_draw()
{
	if (!r_fullscreenflash->value)
		return;

	if (!v_blend[3])
		return;

	oglwEnableBlending(true);
	oglwEnableDepthTest(false);
	oglwEnableDepthWrite(false);

	oglwEnableTexturing(0, GL_FALSE);

	oglwLoadIdentity();

	oglwRotate(-90, 1, 0, 0); /* put Z going up */
	oglwRotate(90, 0, 0, 1); /* put Z going up */

	oglwBegin(GL_TRIANGLES);
	OglwVertex *vtx = oglwAllocateQuad(4);
    if (vtx)
    {
        vtx = AddVertex3D_C(vtx, 10, 100, 100, v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
        vtx = AddVertex3D_C(vtx, 10, -100, 100, v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
        vtx = AddVertex3D_C(vtx, 10, -100, -100, v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
        vtx = AddVertex3D_C(vtx, 10, 100, -100, v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
    }
	oglwEnd();

	oglwEnableTexturing(0, GL_TRUE);

	oglwEnableBlending(false);
	oglwEnableDepthTest(true);
	oglwEnableDepthWrite(true);
}

//********************************************************************************
// 2D rendering.
//********************************************************************************
static void R_Setup2DViewport()
{
	qboolean stereo_split_tb = ((gl_state.stereo_mode == STEREO_SPLIT_VERTICAL) && gl_state.camera_separation);
	qboolean stereo_split_lr = ((gl_state.stereo_mode == STEREO_SPLIT_HORIZONTAL) && gl_state.camera_separation);
    int eyeIndex = gl_state.eyeIndex;

	int x = 0, y = 0;
	int w = viddef.width, h = viddef.height;

	if (stereo_split_lr)
	{
		w = w / 2;
		x = eyeIndex == 0 ? 0 : w;
	}

	if (stereo_split_tb)
	{
		h = h / 2;
		y = eyeIndex == 0 ? h : 0;
	}

	oglwSetViewport(x, y, w, h);
	oglwMatrixMode(GL_PROJECTION);
	oglwLoadIdentity();
	oglwOrtho(0, viddef.width, viddef.height, 0, -99999, 99999);
	oglwMatrixMode(GL_MODELVIEW);
	oglwLoadIdentity();

	glDisable(GL_CULL_FACE);
}

static void R_Setup2D()
{
    R_Setup2DViewport();

    // ROP for 2D.
	oglwEnableBlending(true);
	oglwEnableAlphaTest(true);
	oglwEnableDepthTest(false);
	oglwEnableStencilTest(false);
	oglwEnableDepthWrite(false);
}

//********************************************************************************
// View.
//********************************************************************************
void R_View_setupProjection(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar)
{
	GLfloat xmin, xmax, ymin, ymax;

	ymax = zNear * tanf(fovy * Q_PI / 360.0f);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	xmin += -gl_stereo_convergence->value * (2 * gl_state.camera_separation) / zNear;
	xmax += -gl_stereo_convergence->value * (2 * gl_state.camera_separation) / zNear;

	oglwFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

static void R_View_setup()
{
	r_framecount++;

	/* build the transformation matrix for the given view angles */
	VectorCopy(r_newrefdef.vieworg, r_origin);

	AngleVectors(r_newrefdef.viewangles, vpn, vright, vup);

	/* current viewcluster */
	if (!(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		mleaf_t *leaf = Mod_PointInLeaf(r_origin, r_worldmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		/* check above and below so crossing solid water doesn't draw wrong */
		if (!leaf->contents)
		{
			/* look down a bit */
			vec3_t temp;

			VectorCopy(r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf(temp, r_worldmodel);

			if (!(leaf->contents & CONTENTS_SOLID) &&
			    (leaf->cluster != r_viewcluster2))
			{
				r_viewcluster2 = leaf->cluster;
			}
		}
		else
		{
			/* look up a bit */
			vec3_t temp;

			VectorCopy(r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf(temp, r_worldmodel);

			if (!(leaf->contents & CONTENTS_SOLID) &&
			    (leaf->cluster != r_viewcluster2))
			{
				r_viewcluster2 = leaf->cluster;
			}
		}
	}

	for (int i = 0; i < 4; i++)
		v_blend[i] = r_newrefdef.blend[i];

	c_brush_polys = 0;
	c_alias_polys = 0;

	/* clear out the portion of the screen that the NOWORLDMODEL defines */
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		oglwEnableDepthWrite(true);
		glEnable(GL_SCISSOR_TEST);
		glClearColor(0.3, 0.3, 0.3, 1);
		glScissor(r_newrefdef.x, viddef.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height);
		oglwClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glClearColor(1, 0, 0.5f, 0.5f);
		glDisable(GL_SCISSOR_TEST);
	}
}

static int R_View_computeSignBitsForPlane(cplane_t *out)
{
	/* for fast box on planeside test */
	int bits = 0;
	for (int j = 0; j < 3; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}

static void R_View_setupFrustum()
{
	/* rotate VPN right by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[0].normal, vup, vpn,
		-(90 - r_newrefdef.fov_x / 2));
	/* rotate VPN left by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[1].normal,
		vup, vpn, 90 - r_newrefdef.fov_x / 2);
	/* rotate VPN up by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[2].normal,
		vright, vpn, 90 - r_newrefdef.fov_y / 2);
	/* rotate VPN down by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[3].normal, vright, vpn,
		-(90 - r_newrefdef.fov_y / 2));

	for (int i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
		frustum[i].signbits = R_View_computeSignBitsForPlane(&frustum[i]);
	}
}

static void R_View_setup3D()
{
	/* set up viewport */
	int x = floorf(r_newrefdef.x * viddef.width / viddef.width);
	int x2 = ceilf((r_newrefdef.x + r_newrefdef.width) * viddef.width / viddef.width);
	int y = floorf(viddef.height - r_newrefdef.y * viddef.height / viddef.height);
	int y2 = ceilf(viddef.height - (r_newrefdef.y + r_newrefdef.height) * viddef.height / viddef.height);

	int w = x2 - x;
	int h = y - y2;

	qboolean stereo_split_tb = ((gl_state.stereo_mode == STEREO_SPLIT_VERTICAL) && gl_state.camera_separation);
	qboolean stereo_split_lr = ((gl_state.stereo_mode == STEREO_SPLIT_HORIZONTAL) && gl_state.camera_separation);
    int eyeIndex = gl_state.eyeIndex;

	if (stereo_split_lr)
	{
		w = w / 2;
		x = eyeIndex ? (x / 2) : (x + viddef.width) / 2;
	}

	if (stereo_split_tb)
	{
		h = h / 2;
		y2 = eyeIndex ? (y2 + viddef.height) / 2 : (y2 / 2);
	}

	oglwSetViewport(x, y2, w, h);

	/* set up projection matrix */
	float screenaspect = (float)r_newrefdef.width / r_newrefdef.height;
	oglwMatrixMode(GL_PROJECTION);
	oglwLoadIdentity();
	if (gl_farsee->value == 0)
		R_View_setupProjection(r_newrefdef.fov_y, screenaspect, 4, 4096);
	else
		R_View_setupProjection(r_newrefdef.fov_y, screenaspect, 4, 8192);

	oglwMatrixMode(GL_MODELVIEW);
	oglwLoadIdentity();
	oglwRotate(-90, 1, 0, 0); // put Z going up
	oglwRotate(90, 0, 0, 1); // put Z going up
	oglwRotate(-r_newrefdef.viewangles[2], 1, 0, 0);
	oglwRotate(-r_newrefdef.viewangles[0], 0, 1, 0);
	oglwRotate(-r_newrefdef.viewangles[1], 0, 0, 1);
	oglwTranslate(-r_newrefdef.vieworg[0], -r_newrefdef.vieworg[1], -r_newrefdef.vieworg[2]);

	oglwGetMatrix(GL_MODELVIEW, r_world_matrix);

	glCullFace(GL_FRONT);
	if (gl_cull->value)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	oglwEnableBlending(false);
	oglwEnableAlphaTest(false);
	oglwEnableDepthTest(true);
	oglwEnableDepthWrite(true);
	oglwEnableStencilTest(false);
	if (gl_shadows->value && r_stencilAvailable && gl_stencilshadow->value)
	{
		glStencilFunc(GL_EQUAL, 0, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
	}
}

// r_newrefdef must be set before the first call
void R_View_draw(refdef_t *fd)
{
	if (r_norefresh->value)
		return;

	if ((gl_state.stereo_mode != STEREO_MODE_NONE) && gl_state.camera_separation)
	{
		int eyeIndex = gl_state.eyeIndex;
		switch (gl_state.stereo_mode)
		{
		case STEREO_MODE_ANAGLYPH:
		{
			// Work out the colour for each eye.
			int anaglyph_colours[] = { 0x4, 0x3 };                 // Left = red, right = cyan.

			if (Q_strlen(gl_stereo_anaglyph_colors->string) == 2)
			{
				int eye, colour, missing_bits;
				// Decode the colour name from its character.
				for (eye = 0; eye < 2; ++eye)
				{
					colour = 0;
					switch (toupper(gl_stereo_anaglyph_colors->string[eye]))
					{
					case 'B': ++colour;                         // 001 Blue
					case 'G': ++colour;                         // 010 Green
					case 'C': ++colour;                         // 011 Cyan
					case 'R': ++colour;                         // 100 Red
					case 'M': ++colour;                         // 101 Magenta
					case 'Y': ++colour;                         // 110 Yellow
						anaglyph_colours[eye] = colour;
						break;
					}
				}
				// Fill in any missing bits.
				missing_bits = ~(anaglyph_colours[0] | anaglyph_colours[1]) & 0x3;
				for (eye = 0; eye < 2; ++eye)
				{
					anaglyph_colours[eye] |= missing_bits;
				}
			}

			// Set the current colour.
			glColorMask(
				!!(anaglyph_colours[eyeIndex] & 0x4),
				!!(anaglyph_colours[eyeIndex] & 0x2),
				!!(anaglyph_colours[eyeIndex] & 0x1),
				GL_TRUE
			        );
		}
		break;
		default:
			break;
		}
	}

	r_newrefdef = *fd;

	if (!r_worldmodel && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		R_error(ERR_DROP, "R_View_draw: NULL worldmodel");
	}

	if (gl_speeds->value)
	{
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	R_DynamicLighting_push();
	R_View_setup();
	R_View_setupFrustum();
	R_View_setup3D();
	R_World_markLeaves(); /* done here so we know if we're in water */
	R_World_draw();
	R_Entity_drawAll();
	R_DynamicLighting_draw();
	R_Particles_draw();
	R_Surface_drawChainAlpha(1.0f, 0);
	R_FullscreenFlash_draw();

	if (gl_speeds->value)
	{
		R_printf(PRINT_ALL, "%4i wpoly %4i epoly %i tex %i lmaps\n",
			c_brush_polys, c_alias_polys, c_visible_textures,
			c_visible_lightmaps);
	}

	switch (gl_state.stereo_mode)
	{
	case STEREO_MODE_ANAGLYPH:
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		break;
	default:
		break;
	}

    R_Setup2D();
}

void R_View_setLightLevel()
{
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

    entity_t *entity = &r_worldEntity;
    
	/* save off light value for server to look at */
	vec3_t shadelight;
    vec3_t lightSpot;
    cplane_t *lightPlane;
	R_Lighmap_lightPoint(entity, r_newrefdef.vieworg, shadelight, lightSpot, &lightPlane);

	/* pick the greatest component, which should be the same as the mono value returned by software */
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

//********************************************************************************
// Gamma.
//********************************************************************************
#if !defined(__GCW_ZERO__)
#define HARDWARE_GAMMA_ENABLED
#endif

static void R_Gamma_initialize()
{
	#if defined(HARDWARE_GAMMA_ENABLED)
	R_printf(PRINT_ALL, "Using hardware gamma via SDL.\n");
	#endif
	gl_state.hwgamma = true;
	r_gamma->modified = true;
}

#if defined(HARDWARE_GAMMA_ENABLED)

/*
 *  from SDL2 SDL_CalculateGammaRamp, adjusted for arbitrary ramp sizes
 *  because xrandr seems to support ramp sizes != 256 (in theory at least)
 */
static void R_Gamma_calculateRamp(float gamma, Uint16 * ramp, int len)
{
	int i;

	/* Input validation */
	if (gamma < 0.0f)
		return;
	if (ramp == NULL)
		return;

	/* 0.0 gamma is all black */
	if (gamma == 0.0f)
	{
		for (i = 0; i < len; ++i)
			ramp[i] = 0;
	}
	else if (gamma == 1.0f)
	{
		/* 1.0 gamma is identity */
		for (i = 0; i < len; ++i)
			ramp[i] = (i << 8) | i;
	}
	else
	{
		/* Calculate a real gamma ramp */
		gamma = 1.0f / gamma;
		for (i = 0; i < len; ++i)
		{
			int value = (int)(powf((float)i / (float)len, gamma) * 65535.0f + 0.5f);
			if (value > 65535)
				value = 65535;
			ramp[i] = (Uint16)value;
		}
	}
}
#endif

// Sets the hardware gamma
static void R_Gamma_update()
{
	#if defined(HARDWARE_GAMMA_ENABLED)
	float gamma = (r_gamma->value);

	Uint16 ramp[256];
	R_Gamma_calculateRamp(gamma, ramp, 256);
	if (SDL_SetWindowGammaRamp(sdlwContext->window, ramp, ramp, ramp) != 0)
	{
		R_printf(PRINT_ALL, "Setting gamma failed: %s\n", SDL_GetError());
	}
	#endif
}

//********************************************************************************
// Frame.
//********************************************************************************
static void R_Frame_clear(int eyeIndex)
{
	GLbitfield clearFlags = 0;

	// Color buffer.
	if (gl_clear->value && eyeIndex == 0)
	{
		#if defined(DEBUG)
		glClearColor(1.0f, 0.0f, 0.5f, 0.5f);
		#else
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		#endif
		clearFlags |= GL_COLOR_BUFFER_BIT;
	}

	// Depth buffer.
	if (gl_ztrick->value)
	{
		static int trickframe = 0;
		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999f;
			glDepthFunc(GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5f;
			glDepthFunc(GL_GEQUAL);
		}
	}
	else
	{
		clearFlags |= GL_DEPTH_BUFFER_BIT;
		gldepthmin = 0;
		gldepthmax = 1;
		glClearDepthf(1.0f);
		glDepthFunc(GL_LEQUAL);
        oglwEnableDepthWrite(true);
	}

	// Stencil buffer shadows.
	if (gl_shadows->value && r_stencilAvailable && gl_stencilshadow->value)
	{
		glClearStencil(0);
		clearFlags |= GL_STENCIL_BUFFER_BIT;
	}

	if (clearFlags != 0)
		oglwClear(clearFlags);

	oglwSetDepthRange(gldepthmin, gldepthmax);

	if (gl_zfix->value)
	{
		if (gldepthmax > gldepthmin)
			glPolygonOffset(0.05f, 1);
		else
			glPolygonOffset(-0.05f, -1);
	}
}

void R_Frame_begin(float camera_separation, int eyeIndex)
{
	gl_state.camera_separation = camera_separation;
    gl_state.eyeIndex = eyeIndex;

	// force a r_restart if gl_stereo has been modified.
	if (gl_state.stereo_mode != gl_stereo->value)
		gl_state.stereo_mode = gl_stereo->value;

	if (r_gamma->modified)
	{
		r_gamma->modified = false;
		if (gl_state.hwgamma)
			R_Gamma_update();
	}

	/* texturemode stuff */
	if (r_texture_filter->modified || (gl_config.anisotropic && r_texture_anisotropy->modified))
	{
		R_TextureMode(r_texture_filter->string);
		r_texture_filter->modified = false;
		r_texture_anisotropy->modified = false;
	}

	if (r_texture_alphaformat->modified)
	{
		R_TextureAlphaMode(r_texture_alphaformat->string);
		r_texture_alphaformat->modified = false;
	}

	if (r_texture_solidformat->modified)
	{
		R_TextureSolidMode(r_texture_solidformat->string);
		r_texture_solidformat->modified = false;
	}

    R_Setup2DViewport();

	R_Frame_clear(eyeIndex);

    // ROP for 2D.
	oglwEnableBlending(true);
	oglwEnableAlphaTest(true);
	oglwEnableDepthTest(false);
	oglwEnableDepthWrite(false);
	oglwEnableStencilTest(false);
}

void R_Frame_end()
{
	if (r_discardframebuffer->value && gl_config.discardFramebuffer)
	{
		static const GLenum attachements[] = { GL_DEPTH_EXT, GL_STENCIL_EXT };
        #if defined(EGLW_GLES2)
		gl_config.discardFramebuffer(GL_FRAMEBUFFER, 2, attachements);
        #else
		gl_config.discardFramebuffer(GL_FRAMEBUFFER_OES, 2, attachements);
        #endif
	}
	eglwSwapBuffers();
	Gles_checkGlesError();
	Gles_checkEglError();
}

//********************************************************************************
// Screenshot.
//********************************************************************************
typedef struct _TargaHeader
{
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
} TargaHeader;

static void R_ScreenShot()
{
	/* create the scrnshots directory if it doesn't exist */
	char checkname[MAX_OSPATH];
	Com_sprintf(checkname, sizeof(checkname), "%s/screenshots", FS_WritableGamedir());
	Sys_Mkdir(checkname);

	/* find a file name to save it to */
	char picname[80];
	strcpy(picname, "quake00.tga");
    
    int i;
	for (i = 0; i <= 99; i++)
	{
		picname[5] = i / 10 + '0';
		picname[6] = i % 10 + '0';
		Com_sprintf(checkname, sizeof(checkname), "%s/screenshots/%s", FS_WritableGamedir(), picname);
		FILE *f = fopen(checkname, "rb");
		if (!f)
			break; /* file doesn't exist */
		fclose(f);
	}
	if (i == 100)
	{
		R_printf(PRINT_ALL, "SCR_ScreenShot_f: Couldn't create a file\n");
		return;
	}

    int width = viddef.width, height = viddef.height;
	int headerLength = 18 + 4;
	int c = headerLength + width * height * 3;
	byte *buffer = malloc(c);
	if (!buffer)
	{
		R_printf(PRINT_ALL, "SCR_ScreenShot_f: Couldn't malloc %d bytes\n", c);
		return;
	}
	memset(buffer, 0, headerLength);
	buffer[0] = 4; // image ID: "yq2\0"
	buffer[2] = 2; /* uncompressed type */
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24; /* pixel size */
	buffer[17] = 0; // image descriptor
	buffer[18] = 'y'; // following: the 4 image ID fields
	buffer[19] = 'q';
	buffer[20] = '2';
	buffer[21] = '\0';

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer + headerLength);

	/* swap rgb to bgr */
	for (i = headerLength; i < c; i += 3)
	{
		byte temp = buffer[i];
		buffer[i] = buffer[i + 2];
		buffer[i + 2] = temp;
	}

    {
        FILE *f = fopen(checkname, "wb");
        if (f)
        {
            fwrite(buffer, 1, c, f);
            fclose(f);
            R_printf(PRINT_ALL, "Wrote %s\n", picname);
        }
        else
            R_printf(PRINT_ALL, "SCR_ScreenShot_f: Couldn't write %s\n", picname);
    }
	free(buffer);
}

//********************************************************************************
// No texture.
//********************************************************************************
static void R_NoTexture_Init()
{
    static const byte dottexture[8][8] =
    {
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 1, 1, 0, 0, 0 },
        { 0, 0, 1, 1, 1, 1, 0, 0 },
        { 0, 1, 1, 1, 1, 1, 1, 0 },
        { 0, 1, 1, 1, 1, 1, 1, 0 },
        { 0, 0, 1, 1, 1, 1, 0, 0 },
        { 0, 0, 0, 1, 1, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
    };

	// Use the dot for bad textures, without alpha.
	byte data[8][8][4];
	for (int y = 0; y < 8; y++)
	{
		for (int x = 0; x < 8; x++)
		{
			data[y][x][0] = dottexture[x][y] * 255;
			data[y][x][1] = 0;
			data[y][x][2] = 0;
			data[y][x][3] = 255;
		}
	}
	r_notexture = R_LoadPic("***r_notexture***", (byte *)data, 8, 0, 8, 0, it_wall, 32);
}

//********************************************************************************
// Window.
//********************************************************************************
//--------------------------------------------------------------------------------
// Icon.
//--------------------------------------------------------------------------------
// The 64x64 32bit window icon.
#include "backends/sdl/icon/q2icon64.h"

static void R_Window_setIcon()
{
	/* these masks are needed to tell SDL_CreateRGBSurface(From)
	   to assume the data it gets is byte-wise RGB(A) data */
	Uint32 rmask, gmask, bmask, amask;
	#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	int shift = (q2icon64.bytes_per_pixel == 3) ? 8 : 0;
	rmask = 0xff000000 >> shift;
	gmask = 0x00ff0000 >> shift;
	bmask = 0x0000ff00 >> shift;
	amask = 0x000000ff >> shift;
	#else /* little endian, like x86 */
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = (q2icon64.bytes_per_pixel == 3) ? 0 : 0xff000000;
	#endif

	SDL_Surface * icon = SDL_CreateRGBSurfaceFrom((void *)q2icon64.pixel_data, q2icon64.width,
			q2icon64.height, q2icon64.bytes_per_pixel * 8, q2icon64.bytes_per_pixel * q2icon64.width,
			rmask, gmask, bmask, amask);

	SDL_SetWindowIcon(sdlwContext->window, icon);

	SDL_FreeSurface(icon);
}

//--------------------------------------------------------------------------------
// Mode.
//--------------------------------------------------------------------------------
static void R_Window_finalize()
{
	SDL_ShowCursor(1);

	In_Grab(false);

	/* Clear the backbuffer and make it
	   current. This may help some broken
	   video drivers like the AMD Catalyst
	   to avoid artifacts in unused screen
	   areas. */
	if (oglwIsCreated())
	{
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		oglwEnableDepthWrite(false);
		glStencilMask(0xffffffff);

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClearDepthf(1.0f);
		glClearStencil(0);
		oglwClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		R_Frame_end();

		oglwDestroy();
	}

	eglwFinalize();
	sdlwDestroyWindow();

	gl_state.hwgamma = false;
}

static bool R_Window_getNearestDisplayMode(SDL_DisplayMode *nearestMode)
{
    int requestedWidth = r_fullscreen_width->value;
    int requestedHeight = r_fullscreen_height->value;
    int requestedBpp = r_fullscreen_bitsPerPixel->value;
    int requestedFrequency = r_fullscreen_frequency->value;

    if (requestedWidth < R_WIDTH_MIN)
        requestedWidth = R_WIDTH_MIN;
    if (requestedHeight < R_HEIGHT_MIN)
        requestedHeight = R_HEIGHT_MIN;
    if (requestedBpp < 15)
        requestedBpp = 15;
    if (requestedFrequency < 0)
        requestedFrequency = 0;

    int displayModeNb = SDL_GetNumDisplayModes(0);
    if (displayModeNb < 1)
    {
        SDL_Log("SDL_GetNumDisplayModes failed: %s", SDL_GetError());
        return true;
    }
    SDL_Log("SDL_GetNumDisplayModes: %i", displayModeNb);

    SDL_DisplayMode bestMode;
    int bestModeIndex = -1;
    for (int i = 0; i < displayModeNb; ++i) {
        SDL_DisplayMode mode;
        if (SDL_GetDisplayMode(0, i, &mode) != 0)
        {
            SDL_Log("SDL_GetDisplayMode %i failed: %s", i, SDL_GetError());
            break;
        }
        SDL_Log("Mode %i %ix%i %i bpp (%s) %i Hz", i, mode.w, mode.h, SDL_BITSPERPIXEL(mode.format), SDL_GetPixelFormatName(mode.format), mode.refresh_rate);
        bool nearer =
            abs(requestedWidth - mode.w) <= abs(requestedWidth - bestMode.w) && abs(requestedHeight - mode.h) <= abs(requestedHeight - bestMode.h) &&
            abs(requestedBpp - (int)SDL_BITSPERPIXEL(mode.format)) <= abs(requestedBpp - (int)SDL_BITSPERPIXEL(bestMode.format)) &&
            abs(requestedFrequency - mode.refresh_rate) <= abs(requestedFrequency - bestMode.refresh_rate)
            ;
        bool bestAbove = bestMode.w >= requestedWidth && bestMode.h >= requestedHeight && (int)SDL_BITSPERPIXEL(bestMode.format) >= requestedBpp && bestMode.refresh_rate >= requestedFrequency;
        bool above = mode.w >= requestedWidth && mode.h >= requestedHeight && (int)SDL_BITSPERPIXEL(mode.format) >= requestedBpp && mode.refresh_rate >= requestedFrequency;
        if ((above && !bestAbove) || nearer)
        {
            bestMode = mode;
            bestModeIndex = i;
        }
    }
    if (bestModeIndex < 0)
        return true;
    *nearestMode = bestMode;
    return false;
}

static void R_Window_getMaxWindowSize(int *windowWidth, int *windowHeight)
{
    int maxWindowWidth = 0, maxWindowHeight = 0;

    #if SDL_VERSION_ATLEAST(2, 0, 5)
    SDL_Rect rect;
    if (SDL_GetDisplayUsableBounds(0, &rect) == 0)
    {
        maxWindowWidth = rect.w;
        maxWindowHeight = rect.h;
    }
    else
    #endif
    {
        SDL_DisplayMode mode;
        if (SDL_GetDesktopDisplayMode(0, &mode) == 0)
        {
            maxWindowWidth = mode.w;
            maxWindowHeight = mode.h;
        }
    }
    
    *windowWidth = maxWindowWidth;
    *windowHeight = maxWindowHeight;
}

static void R_Window_getValidWindowSize(int maxWindowWidth, int maxWindowHeight, int requestedWidth, int requestedHeight, int *windowWidth, int *windowHeight)
{
    if (maxWindowWidth > 0 && requestedWidth > maxWindowWidth)
        requestedWidth = maxWindowWidth;
    if (maxWindowHeight > 0 && requestedHeight > maxWindowHeight)
        requestedHeight = maxWindowHeight;
    if (requestedWidth < R_WIDTH_MIN)
        requestedWidth = R_WIDTH_MIN;
    if (requestedHeight < R_HEIGHT_MIN)
        requestedHeight = R_HEIGHT_MIN;   
    *windowWidth = requestedWidth;
    *windowHeight = requestedHeight;
}

static bool R_Window_setup()
{
	SdlwContext *sdlw = sdlwContext;
    bool fullscreen = r_fullscreen->value;
    if (fullscreen)
    {
        SDL_DisplayMode displayMode;
        if (R_Window_getNearestDisplayMode(&displayMode))
            return true;
        if (SDL_SetWindowDisplayMode(sdlw->window, &displayMode) != 0)
            return true;
        SDL_SetWindowPosition(sdlw->window, 0, 0);
        SDL_SetWindowSize(sdlw->window, displayMode.w, displayMode.h); // Must be after SDL_SetWindowDisplayMode() (SDL bug ?).
        if (SDL_SetWindowFullscreen(sdlw->window, SDL_WINDOW_FULLSCREEN) != 0)  // This must be after SDL_SetWindowPosition() and SDL_SetWindowSize() when fullscreen is enabled !
            return true;
    }
    else
    {
        if (SDL_SetWindowFullscreen(sdlw->window, 0) != 0) // This must be before SDL_SetWindowPosition() and SDL_SetWindowSize() when fullscreen is disabled !
            return true;
        int windowX = r_window_x->value, windowY = r_window_y->value;
        SDL_SetWindowPosition(sdlw->window, windowX, windowY);
        int windowWidth = r_window_width->value, windowHeight = r_window_height->value;
        SDL_SetWindowSize(sdlw->window, windowWidth, windowHeight);
    }
    return false;
}

static bool R_Window_update(bool forceFlag)
{
	SdlwContext *sdlw = sdlwContext;

    int maxWindowWidth, maxWindowHeight;
    R_Window_getMaxWindowSize(&maxWindowWidth, &maxWindowHeight);
    
    while (1)
    {
        bool applyChange = false;

        int windowWidth, windowHeight;
        R_Window_getValidWindowSize(maxWindowWidth, maxWindowHeight, r_window_width->value, r_window_height->value, &windowWidth, &windowHeight);

        bool fullscreen = r_fullscreen->value;

        if (sdlw->window == NULL)
        {
            char windowName[64];
            snprintf(windowName, sizeof(windowName), QUAKE2_COMPLETE_NAME);
            windowName[63] = 0;
            int flags = SDL_WINDOW_RESIZABLE;
            if (fullscreen)
                flags |= SDL_WINDOW_FULLSCREEN;
            if (!sdlwCreateWindow(windowName, windowWidth, windowHeight, flags))
            {
                R_Window_setIcon();
                applyChange = true;
            }
        }

        if (sdlw->window != NULL)
        {
            bool currentFullscreen = (SDL_GetWindowFlags(sdlw->window) & SDL_WINDOW_FULLSCREEN) != 0;
            if (fullscreen != currentFullscreen)
            {
                applyChange = true;
                if (!r_fullscreen->modified)
                {
                    // The fullscreen state has been changed externally (by the user or the system).
                    fullscreen = currentFullscreen;
                }
                else
                {
                    // The fullscreen state has been changed internally (in the menus or via the console).
                }
            }
            
            if (fullscreen)
            {
                if (r_fullscreen_width->modified || r_fullscreen_height->modified || r_fullscreen_bitsPerPixel->modified || r_fullscreen_frequency->modified)
                    applyChange = true;
            }
            
            if (!currentFullscreen)
            {
                int currentWidth, currentHeight;
                SDL_GetWindowSize(sdlw->window, &currentWidth, &currentHeight);
                if (windowWidth != currentWidth || windowHeight != currentHeight)
                {
                    applyChange = true;
                    if (!r_window_width->modified && !r_window_height->modified)
                    {
                        // The window size has been changed externally (by the user or the system).
                        R_Window_getValidWindowSize(maxWindowWidth, maxWindowHeight, currentWidth, currentHeight, &windowWidth, &windowHeight);
                    }
                    else
                    {
                        // The window size has been changed internally (in the menus or via the console).
                    }
                }
            }

            if (!applyChange)
                break;
                
            int windowX, windowY;
            if (currentFullscreen)
            {
                windowX = r_window_x->value;
                windowY = r_window_y->value;
            }
            else
            {
                SDL_GetWindowPosition(sdlw->window, &windowX, &windowY);
            }
            #if 0
            if (windowX > maxWindowWidth - windowWidth)
                windowX = maxWindowWidth - windowWidth;
            if (windowX < 0)
                windowX = 0;
            if (windowY > maxWindowHeight - windowHeight)
                windowY = maxWindowHeight - windowHeight;
            if (windowY < 0)
                windowY = 0;
            #endif

            Cvar_SetValue("r_window_width", windowWidth);
            Cvar_SetValue("r_window_height", windowHeight);
            Cvar_SetValue("r_window_x", windowX);
            Cvar_SetValue("r_window_y", windowY);
            Cvar_SetValue("r_fullscreen", fullscreen);

            r_window_width->modified = false;
            r_window_height->modified = false;
            r_window_x->modified = false;
            r_window_y->modified = false;
            r_fullscreen->modified = false;
            r_fullscreen_width->modified = false;
            r_fullscreen_height->modified = false;
            r_fullscreen_bitsPerPixel->modified = false;
            r_fullscreen_frequency->modified = false;
            
            if (!R_Window_setup())
            {
                int effectiveWidth, effectiveHeight;
                SDL_GetWindowSize(sdlw->window, &effectiveWidth, &effectiveHeight);
                viddef.width = effectiveWidth;
                viddef.height = effectiveHeight;
                sdlwResize(effectiveWidth, effectiveHeight);
                break;
            }
        }
        
        R_printf(PRINT_ALL, "Failed to create a window.\n");
        if (fullscreen)
        {
            R_printf(PRINT_ALL, "Trying with fullscreen = %i.\n", false);
            Cvar_SetValue("r_fullscreen", false);
        }
        else
        {
            R_printf(PRINT_ALL, "All attempts failed\n");
            return true;
        }
    }
    
    return false;
}

static bool R_Window_createContext()
{
	while (1)
	{
		EglwConfigInfo cfgiMinimal;
		cfgiMinimal.redSize = 5; cfgiMinimal.greenSize = 5; cfgiMinimal.blueSize = 5; cfgiMinimal.alphaSize = 0;
		cfgiMinimal.depthSize = 16; cfgiMinimal.stencilSize = 0; cfgiMinimal.samples = 0;
		EglwConfigInfo cfgiRequested;
		cfgiRequested.redSize = 5; cfgiRequested.greenSize = 5; cfgiRequested.blueSize = 5; cfgiRequested.alphaSize = 0;
		cfgiRequested.depthSize = 16; cfgiRequested.stencilSize = 1; cfgiRequested.samples = (int)r_msaa_samples->value;

		if (eglwInitialize(&cfgiMinimal, &cfgiRequested, false))
		{
            R_printf(PRINT_ALL, "Cannot create an OpenGL ES context.\n");
			if (r_msaa_samples->value)
			{
				R_printf(PRINT_ALL, "Trying without MSAA.\n");
				Cvar_SetValue("r_msaa_samples", 0);
			}
			else
			{
				R_printf(PRINT_ALL, "All attempts failed.\n");
				goto on_error;
			}
		}
		else
		{
			break;
		}
	}

	r_msaaAvailable = (eglwContext->configInfoAbilities.samples > 0);
	Cvar_SetValue("r_msaa_samples", eglwContext->configInfo.samples);
	r_stencilAvailable = (eglwContext->configInfo.stencilSize > 0);

	eglSwapInterval(eglwContext->display, gl_swapinterval->value ? 1 : 0);

	if (oglwCreate())
		goto on_error;

	R_Gamma_initialize();

	SDL_ShowCursor(0);

	return true;

on_error:
	R_Window_finalize();
	return false;
}

//--------------------------------------------------------------------------------
// Video.
//--------------------------------------------------------------------------------
void R_Window_toggleFullScreen()
{
    cvar_t *fullscreen = r_fullscreen;
    if (!fullscreen->value)
    {
        fullscreen->value = 1;
        fullscreen->modified = true;
    }
    else
    {
        fullscreen->value = 0;
        fullscreen->modified = true;
    }
}

//********************************************************************************
// Log.
//********************************************************************************
#define MAXPRINTMSG 4096

void R_printf(int print_level, char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end(argptr);

	if (print_level == PRINT_ALL)
	{
		Com_Printf("%s", msg);
	}
	else
	{
		Com_DPrintf("%s", msg);
	}
}

void R_error(int err_level, char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end(argptr);

	Com_Error(err_level, "%s", msg);
}

//********************************************************************************
// Initialization and shutdown.
//********************************************************************************
static bool r_active = false; /* Is the refresher being used? */
static bool r_restart = false;

static void R_Strings()
{
	R_printf(PRINT_ALL, "GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	R_printf(PRINT_ALL, "GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	R_printf(PRINT_ALL, "GL_VERSION: %s\n", glGetString(GL_VERSION));
	R_printf(PRINT_ALL, "GL_EXTENSIONS: %s\n", glGetString(GL_EXTENSIONS));
}

static void R_Register()
{
	r_window_width = Cvar_Get("r_window_width", R_WIDTH_MIN_STRING, CVAR_ARCHIVE);
	r_window_height = Cvar_Get("r_window_height", R_HEIGHT_MIN_STRING, CVAR_ARCHIVE);
	r_window_x = Cvar_Get("r_window_x", "0", 0);
	r_window_y = Cvar_Get("r_window_y", "0", 0);
	r_fullscreen = Cvar_Get("r_fullscreen", GL_FULLSCREEN_DEFAULT_STRING, CVAR_ARCHIVE);
	r_fullscreen_width = Cvar_Get("r_fullscreen_width", R_WIDTH_MIN_STRING, CVAR_ARCHIVE);
	r_fullscreen_height = Cvar_Get("r_fullscreen_height", R_HEIGHT_MIN_STRING, CVAR_ARCHIVE);
	r_fullscreen_bitsPerPixel = Cvar_Get("r_fullscreen_bitsPerPixel", "24", CVAR_ARCHIVE);
	r_fullscreen_frequency = Cvar_Get("r_fullscreen_frequency", "60", CVAR_ARCHIVE);
    
	r_gamma = Cvar_Get("r_gamma", "1", CVAR_ARCHIVE);
	r_msaa_samples = Cvar_Get("r_msaa_samples", "0", CVAR_ARCHIVE);

	r_discardframebuffer = Cvar_Get("r_discardframebuffer", "1", CVAR_ARCHIVE);
    #if defined(__RASPBERRY_PI_) || defined(__GCW_ZERO__) || defined(__CREATOR_CI20__)
	gl_clear = Cvar_Get("gl_clear", "1", CVAR_ARCHIVE); // Enabled by default for embedded GPU.
    #else
	gl_clear = Cvar_Get("gl_clear", "0", CVAR_ARCHIVE);
    #endif
	gl_ztrick = Cvar_Get("gl_ztrick", "0", CVAR_ARCHIVE);
	gl_zfix = Cvar_Get("gl_zfix", "0", 0);
	gl_swapinterval = Cvar_Get("gl_swapinterval", "1", CVAR_ARCHIVE);

	gl_speeds = Cvar_Get("gl_speeds", "0", 0);
	r_norefresh = Cvar_Get("r_norefresh", "0", 0);
	gl_drawentities = Cvar_Get("gl_drawentities", "1", 0);
	gl_drawworld = Cvar_Get("gl_drawworld", "1", 0);

	gl_novis = Cvar_Get("gl_novis", "0", 0);
	gl_nocull = Cvar_Get("gl_nocull", "0", 0);
	gl_cull = Cvar_Get("gl_cull", "1", 0);
	gl_lockpvs = Cvar_Get("gl_lockpvs", "0", 0);

	gl_lefthand = Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	gl_farsee = Cvar_Get("gl_farsee", "0", CVAR_LATCH | CVAR_ARCHIVE);
	gl_lerpmodels = Cvar_Get("gl_lerpmodels", "1", 0);
	gl_lightlevel = Cvar_Get("gl_lightlevel", "0", 0);
	gl_modulate = Cvar_Get("gl_modulate", "1", CVAR_ARCHIVE);
	r_lightmap_saturate = Cvar_Get("r_lightmap_saturate", "0", 0);

	gl_particle_min_size = Cvar_Get("gl_particle_min_size", "2", CVAR_ARCHIVE);
	gl_particle_max_size = Cvar_Get("gl_particle_max_size", "40", CVAR_ARCHIVE);
	gl_particle_size = Cvar_Get("gl_particle_size", "40", CVAR_ARCHIVE);
	gl_particle_att_a = Cvar_Get("gl_particle_att_a", "0.01", CVAR_ARCHIVE);
	gl_particle_att_b = Cvar_Get("gl_particle_att_b", "0.0", CVAR_ARCHIVE);
	gl_particle_att_c = Cvar_Get("gl_particle_att_c", "0.01", CVAR_ARCHIVE);
	#if defined(EGLW_GLES1)
    // Points and point sprites are disabled by default because they are not well supported by most GLES 1 implementations.
	gl_particle_point = Cvar_Get("gl_particle_point", "0", CVAR_ARCHIVE);
	gl_particle_sprite = Cvar_Get("gl_particle_sprite", "0", CVAR_ARCHIVE);
	#endif

    // Multitexturing is disabled by default because it is slow on embedded platforms. When used, textures cannot be sorted so it causes more state changes.
	r_multitexturing = Cvar_Get("r_multitexturing", "0", CVAR_ARCHIVE);
	r_lightmap_disabled = Cvar_Get("r_lightmap_disabled", "0", 0);
	r_lightmap_only = Cvar_Get("r_lightmap_only", "0", 0);
	r_lightmap_mipmap = Cvar_Get("r_lightmap_mipmap", "0", 0); // Mipmap generation creates slowdowns in-game, do not enable it on embedded platforms.
    r_lightmap_backface_lighting = Cvar_Get("r_lightmap_backface_lighting", "1", CVAR_ARCHIVE);
    #if defined(__RASPBERRY_PI_) || defined(__GCW_ZERO__) || defined(__CREATOR_CI20__)
    // Dynamic lightmaps are disabled by default on embedded platforms because it currently causes huge slow downs, or glitches (GCW Zero).
    // With tile rendering, used by most embedded GPU, the whole frame must be flushed before updating a used texture.
	r_lightmap_dynamic = Cvar_Get("r_lightmap_dynamic", "0", CVAR_ARCHIVE);
	r_lightflash = Cvar_Get("r_lightflash", "1", CVAR_ARCHIVE);
    #else
	r_lightmap_dynamic = Cvar_Get("r_lightmap_dynamic", "1", CVAR_ARCHIVE);
	r_lightflash = Cvar_Get("r_lightflash", "0", CVAR_ARCHIVE);
    #endif
	r_lightmap_outline = Cvar_Get("r_lightmap_outline", "0", 0);
	r_nobind = Cvar_Get("r_nobind", "0", 0);
    r_subdivision = Cvar_Get("r_subdivision", "64", CVAR_ARCHIVE);

	r_fullscreenflash = Cvar_Get("r_fullscreenflash", "1", 0);

	r_texture_retexturing = Cvar_Get("r_texture_retexturing", "1", CVAR_ARCHIVE);
	r_texture_filter = Cvar_Get("r_texture_filter", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE);
	r_texture_anisotropy = Cvar_Get("r_texture_anisotropy", "2", CVAR_ARCHIVE);
	r_texture_anisotropy_available = Cvar_Get("r_texture_anisotropy_available", "0", 0);
	r_texture_alphaformat = Cvar_Get("r_texture_alphaformat", "default", CVAR_ARCHIVE);
	r_texture_solidformat = Cvar_Get("r_texture_solidformat", "default", CVAR_ARCHIVE);
	r_texture_rounddown = Cvar_Get("r_texture_rounddown", "0", 0);
	r_texture_scaledown = Cvar_Get("r_texture_scaledown", "0", 0);

	gl_shadows = Cvar_Get("gl_shadows", "1", CVAR_ARCHIVE);
	gl_stencilshadow = Cvar_Get("gl_stencilshadow", "1", CVAR_ARCHIVE);

	gl_stereo = Cvar_Get("gl_stereo", "0", CVAR_ARCHIVE);
	gl_stereo_separation = Cvar_Get("gl_stereo_separation", "-0.4", CVAR_ARCHIVE);
	gl_stereo_convergence = Cvar_Get("gl_stereo_convergence", "1", CVAR_ARCHIVE);
	gl_stereo_anaglyph_colors = Cvar_Get("gl_stereo_anaglyph_colors", "rc", CVAR_ARCHIVE);

	Cmd_AddCommand("imagelist", R_ImageList_f);
	Cmd_AddCommand("screenshot", R_ScreenShot);
	Cmd_AddCommand("modellist", Mod_Modellist_f);
	Cmd_AddCommand("gl_strings", R_Strings);
}

static bool R_setup()
{
	Swap_Init();
	Draw_GetPalette();
	R_Register();

	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if (SDL_Init(SDL_INIT_VIDEO) == -1)
			return false;

		const char * driverName = SDL_GetCurrentVideoDriver();
		R_printf(PRINT_ALL, "SDL video driver is \"%s\".\n", driverName);
	}

    // Create a window.
    if (R_Window_update(true))
    {
		R_printf(PRINT_ALL, "Could not create a window\n");
        return false;
    }
    
	// Create a rendering context.
	if (!R_Window_createContext())
	{
		R_printf(PRINT_ALL, "Could not create a rendering context\n");
		return false;
	}

	R_Strings();

	R_printf(PRINT_ALL, "\n\nProbing for OpenGL extensions:\n");
	const char *extensions_string = (const char *)glGetString(GL_EXTENSIONS);

	if (strstr(extensions_string, "GL_EXT_discard_framebuffer"))
	{
		R_printf(PRINT_ALL, "Using GL_EXT_discard_framebuffer\n");
		gl_config.discardFramebuffer = (PFNGLDISCARDFRAMEBUFFEREXTPROC)eglGetProcAddress("glDiscardFramebufferEXT");
	}
	else
	{
		R_printf(PRINT_ALL, "GL_EXT_discard_framebuffer not found\n");
		gl_config.discardFramebuffer = NULL;
	}

	if (strstr(extensions_string, "GL_EXT_texture_filter_anisotropic"))
	{
		R_printf(PRINT_ALL, "Using GL_EXT_texture_filter_anisotropic\n");
		gl_config.anisotropic = true;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_config.max_anisotropy);
		Cvar_SetValue("r_texture_anisotropy_available", gl_config.max_anisotropy);
	}
	else
	{
		R_printf(PRINT_ALL, "GL_EXT_texture_filter_anisotropic not found\n");
		gl_config.anisotropic = false;
		gl_config.max_anisotropy = 0.0f;
		Cvar_SetValue("r_texture_anisotropy_available", 0.0f);
	}

	#if 0
	if (strstr(extensions_string, "OES_texture_npot"))
	{
		R_printf(PRINT_ALL, "Using OES_texture_npot\n");
		gl_config.tex_npot = true;
	}
	#endif

	/* set our "safe" mode */
	gl_state.stereo_mode = gl_stereo->value;

	Cvar_Set("scr_drawall", "0");

	R_SetDefaultState();

	R_InitImages();
	Mod_Init();
	R_NoTexture_Init();
	R_Particles_initialize();
	Draw_InitLocal();

	int err = glGetError();
	if (err != GL_NO_ERROR)
	{
		R_printf(PRINT_ALL, "glGetError() = 0x%x\n", err);
	}

	return true;
}

void R_finalize()
{
    r_restart = false;
	if (!r_active)
        return;
    r_active = false;

	Cmd_RemoveCommand("modellist");
	Cmd_RemoveCommand("screenshot");
	Cmd_RemoveCommand("imagelist");
	Cmd_RemoveCommand("gl_strings");

	Mod_FreeAll();

	R_ShutdownImages();

	R_Window_finalize();
	if (SDL_WasInit(SDL_INIT_VIDEO))
	{
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
}

static void R_restart()
{
	r_restart = true;
}

static void R_start()
{
    S_StopAllSounds();

    /* refresh has changed */
    cl.refresh_prepped = false;
    cl.cinematicpalette_active = false;
    cls.disable_screen = true;

	R_finalize();

	Com_Printf("----- renderer initialization -----\n");

	r_active = true;
	if (!R_setup())
	{
        R_error(ERR_FATAL, "Could not initialize rendering");
		R_finalize();
	}

	// Ensure that all key states are cleared.
	Key_MarkAllUp();

    cls.disable_screen = false;
}

/*
 * This function gets called once just before drawing each frame, and
 * it's sole purpose in life is to check to see if any of the video mode
 * parameters have changed, and if they have to update the refresh
 * and/or video mode to match.
 */
void R_checkChanges()
{
	if (r_restart)
	{
        r_restart = false;
        R_start();
	}
    else
    {
        R_Window_update(false);
    }
}

void R_initialize()
{
    r_restart = false;
    r_active = false;

	Cmd_AddCommand("r_restart", R_restart);

	R_start();
}
