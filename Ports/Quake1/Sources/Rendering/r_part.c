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
#include "Common/cmd.h"
#include "Common/mathlib.h"
#include "Rendering/r_private.h"
#include "Server/server.h"

#include <stdlib.h>

cvar_t r_particles_min_size = { "r_particles_min_size", "2", true, false };
cvar_t r_particles_max_size = { "r_particles_max_size", "40", true, false };
cvar_t r_particles_size = { "r_particles_size", "40", true, false };
cvar_t r_particles_att_a = { "r_particles_att_a", "0.01", true, false };
cvar_t r_particles_att_b = { "r_particles_att_b", "0.0", true, false };
cvar_t r_particles_att_c = { "r_particles_att_c", "0.01", true, false };

typedef enum
{
	pt_static, pt_grav, pt_slowgrav, pt_fire, pt_explode, pt_explode2, pt_blob, pt_blob2
} ptype_t;

typedef struct particle_s
{
	// driver-usable fields
	vec3_t org;
	float color;
	// drivers never touch the following fields
	struct particle_s *next;
	vec3_t vel;
	float ramp;
	float die;
	ptype_t type;
} particle_t;

#define MAX_PARTICLES 2048 // default max # of particles at one time
#define ABSOLUTE_MIN_PARTICLES 512 // no fewer than this no matter what's on the command line

static particle_t *active_particles, *free_particles;
static particle_t *particles;
static int r_numparticles;

#define NUMVERTEXNORMALS 162

static const float r_avertexnormals[NUMVERTEXNORMALS][3] =
{
	#include "Rendering/anorms.h"
};

static vec3_t avelocities[NUMVERTEXNORMALS];
static float beamlength = 16;

static const unsigned char ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
static const unsigned char ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
static const unsigned char ramp3[8] = { 0x6d, 0x6b, 6, 5, 4, 3 };

void R_EntityParticles(entity_t *ent)
{
	if (!avelocities[0][0])
	{
		for (int i = 0; i < NUMVERTEXNORMALS; i++)
        {
            vec3_t *av = &avelocities[i];
			(*av)[0] = (rand() & 255) * 0.01f;
			(*av)[1] = (rand() & 255) * 0.01f;
			(*av)[2] = (rand() & 255) * 0.01f;
        }
	}

	float dist = 64;
	//int count = 50;
	for (int i = 0; i < NUMVERTEXNORMALS; i++)
	{
        float angle;
        float sr, sp, sy, cr, cp, cy;
		angle = cl.time * avelocities[i][0];
		sy = sinf(angle);
		cy = cosf(angle);
		angle = cl.time * avelocities[i][1];
		sp = sinf(angle);
		cp = cosf(angle);
		angle = cl.time * avelocities[i][2];
		sr = sinf(angle);
		cr = cosf(angle);

        vec3_t forward;
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		if (!free_particles)
			return;

        particle_t *p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 0.01f;
		p->color = 0x6f;
		p->type = pt_explode;

		p->org[0] = ent->origin[0] + r_avertexnormals[i][0] * dist + forward[0] * beamlength;
		p->org[1] = ent->origin[1] + r_avertexnormals[i][1] * dist + forward[1] * beamlength;
		p->org[2] = ent->origin[2] + r_avertexnormals[i][2] * dist + forward[2] * beamlength;
	}
}

void R_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count)
{
	int i, j;
	particle_t *p;

	for (i = 0; i < count; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		if (count == 1024) // rocket explosion
		{
			p->die = cl.time + 5;
			p->color = ramp1[0];
			p->ramp = rand() & 3;
			if (i & 1)
			{
				p->type = pt_explode;
				for (j = 0; j < 3; j++)
				{
					p->org[j] = org[j] + ((rand() % 32) - 16);
					p->vel[j] = (rand() % 512) - 256;
				}
			}
			else
			{
				p->type = pt_explode2;
				for (j = 0; j < 3; j++)
				{
					p->org[j] = org[j] + ((rand() % 32) - 16);
					p->vel[j] = (rand() % 512) - 256;
				}
			}
		}
		else
		{
			p->die = cl.time + 0.1 * (rand() % 5);
			p->color = (color & ~7) + (rand() & 7);
			p->type = pt_slowgrav;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() & 15) - 8);
				p->vel[j] = dir[j] * 15; // + (rand()%300)-150;
			}
		}
	}
}

// Parse an effect out of the server message
void R_ParseParticleEffect()
{
	vec3_t org, dir;
	int i, count, msgcount, color;

	for (i = 0; i < 3; i++)
		org[i] = MSG_ReadCoord();
	for (i = 0; i < 3; i++)
		dir[i] = MSG_ReadChar() * (1.0f / 16);
	msgcount = MSG_ReadByte();
	color = MSG_ReadByte();

	if (msgcount == 255)
		count = 1024;
	else
		count = msgcount;

	R_RunParticleEffect(org, dir, color, count);
}

void R_ParticleExplosion(vec3_t org)
{
	int i, j;
	particle_t *p;

	for (i = 0; i < 1024; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 5;
		p->color = ramp1[0];
		p->ramp = rand() & 3;
		if (i & 1)
		{
			p->type = pt_explode;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
		else
		{
			p->type = pt_explode2;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
	}
}

void R_ParticleExplosion2(vec3_t org, int colorStart, int colorLength)
{
	int i, j;
	particle_t *p;
	int colorMod = 0;

	for (i = 0; i < 512; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 0.3f;
		p->color = colorStart + (colorMod % colorLength);
		colorMod++;

		p->type = pt_blob;
		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand() % 32) - 16);
			p->vel[j] = (rand() % 512) - 256;
		}
	}
}

void R_BlobExplosion(vec3_t org)
{
	int i, j;
	particle_t *p;

	for (i = 0; i < 1024; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 1 + (rand() & 8) * 0.05f;

		if (i & 1)
		{
			p->type = pt_blob;
			p->color = 66 + rand() % 6;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
		else
		{
			p->type = pt_blob2;
			p->color = 150 + rand() % 6;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
	}
}

void R_LavaSplash(vec3_t org)
{
	int i, j, k;
	particle_t *p;
	float vel;
	vec3_t dir;

	for (i = -16; i < 16; i++)
		for (j = -16; j < 16; j++)
			for (k = 0; k < 1; k++)
			{
				if (!free_particles)
					return;

				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = cl.time + 2 + (rand() & 31) * 0.02f;
				p->color = 224 + (rand() & 7);
				p->type = pt_slowgrav;

				dir[0] = j * 8 + (rand() & 7);
				dir[1] = i * 8 + (rand() & 7);
				dir[2] = 256;

				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand() & 63);

				VectorNormalize(dir);
				vel = 50 + (rand() & 63);
				VectorScale(dir, vel, p->vel);
			}

}

void R_TeleportSplash(vec3_t org)
{
	int i, j, k;
	particle_t *p;
	float vel;
	vec3_t dir;

	for (i = -16; i < 16; i += 4)
		for (j = -16; j < 16; j += 4)
			for (k = -24; k < 32; k += 4)
			{
				if (!free_particles)
					return;

				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = cl.time + 0.2f + (rand() & 7) * 0.02f;
				p->color = 7 + (rand() & 7);
				p->type = pt_slowgrav;

				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				p->org[0] = org[0] + i + (rand() & 3);
				p->org[1] = org[1] + j + (rand() & 3);
				p->org[2] = org[2] + k + (rand() & 3);

				VectorNormalize(dir);
				vel = 50 + (rand() & 63);
				VectorScale(dir, vel, p->vel);
			}

}

void R_RocketTrail(vec3_t start, vec3_t end, int type)
{
	vec3_t vec;
	float len;
	int j;
	particle_t *p;
	int dec;
	static int tracercount;

	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);
	if (type < 128)
		dec = 3;
	else
	{
		dec = 1;
		type -= 128;
	}

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy(vec3_origin, p->vel);
		p->die = cl.time + 2;

		switch (type)
		{
		case 0: // rocket trail
			p->ramp = (rand() & 3);
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = start[j] + ((rand() % 6) - 3);
			break;

		case 1: // smoke smoke
			p->ramp = (rand() & 3) + 2;
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = start[j] + ((rand() % 6) - 3);
			break;

		case 2: // blood
			p->type = pt_grav;
			p->color = 67 + (rand() & 3);
			for (j = 0; j < 3; j++)
				p->org[j] = start[j] + ((rand() % 6) - 3);
			break;

		case 3:
		case 5: // tracer
			p->die = cl.time + 0.5f;
			p->type = pt_static;
			if (type == 3)
				p->color = 52 + ((tracercount & 4) << 1);
			else
				p->color = 230 + ((tracercount & 4) << 1);

			tracercount++;

			VectorCopy(start, p->org);
			if (tracercount & 1)
			{
				p->vel[0] = 30 * vec[1];
				p->vel[1] = 30 * -vec[0];
			}
			else
			{
				p->vel[0] = 30 * -vec[1];
				p->vel[1] = 30 * vec[0];
			}
			break;

		case 4: // slight blood
			p->type = pt_grav;
			p->color = 67 + (rand() & 3);
			for (j = 0; j < 3; j++)
				p->org[j] = start[j] + ((rand() % 6) - 3);
			len -= 3;
			break;

		case 6: // voor trail
			p->color = 9 * 16 + 8 + (rand() & 3);
			p->type = pt_static;
			p->die = cl.time + 0.3f;
			for (j = 0; j < 3; j++)
				p->org[j] = start[j] + ((rand() & 15) - 8);
			break;
		}

		VectorAdd(start, vec, start);
	}
}

static float R_Particles_computeSize(float distance2, float distance)
{
	float attenuation = r_particles_att_a.value;
	float factorB = r_particles_att_b.value;
	float factorC = r_particles_att_c.value;
	float particleSize = r_particles_size.value;
	float particleMinSize = r_particles_min_size.value;
	float particleMaxSize = r_particles_max_size.value;

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

static void R_UpdateParticles()
{
    extern cvar_t sv_gravity;

	float frametime = cl.time - cl.oldtime;
	float time3 = frametime * 15;
	float time2 = frametime * 10;
	float time1 = frametime * 5;
	float grav = frametime * sv_gravity.value * 0.05f;
	float dvel = 4 * frametime;

	for (particle_t *p = active_particles, *pp = NULL; p; )
	{
		particle_t *pn = p->next;

		if (p->die < cl.time)
		{
            if (pp == NULL) active_particles = pn;
            else pp->next = pn;
			p->next = free_particles;
			free_particles = p;
		}
		else
		{
			p->org[0] += p->vel[0] * frametime;
			p->org[1] += p->vel[1] * frametime;
			p->org[2] += p->vel[2] * frametime;

			switch (p->type)
			{
			case pt_static:
				break;
			case pt_fire:
				p->ramp += time1;
				if (p->ramp >= 6)
					p->die = -1;
				else
					p->color = ramp3[(int)p->ramp];
				p->vel[2] += grav;
				break;

			case pt_explode:
				p->ramp += time2;
				if (p->ramp >= 8)
					p->die = -1;
				else
					p->color = ramp1[(int)p->ramp];
				for (int i = 0; i < 3; i++)
					p->vel[i] += p->vel[i] * dvel;
				p->vel[2] -= grav;
				break;

			case pt_explode2:
				p->ramp += time3;
				if (p->ramp >= 8)
					p->die = -1;
				else
					p->color = ramp2[(int)p->ramp];
				for (int i = 0; i < 3; i++)
					p->vel[i] -= p->vel[i] * frametime;
				p->vel[2] -= grav;
				break;

			case pt_blob:
				for (int i = 0; i < 3; i++)
					p->vel[i] += p->vel[i] * dvel;
				p->vel[2] -= grav;
				break;

			case pt_blob2:
				for (int i = 0; i < 2; i++)
					p->vel[i] -= p->vel[i] * dvel;
				p->vel[2] -= grav;
				break;

			case pt_grav:
			case pt_slowgrav:
				p->vel[2] -= grav;
				break;
			}
            
            pp = p;
		}

		p = pn;
	}
}

static int r_particleTexture; // little dot for particles

static void R_InitParticleTexture()
{
	// Particle texture.
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

    r_particleTexture = R_Texture_create("", particleSize, particleSize, particleData, true, true, false, false);

    free(particleData);
}

static void R_ReadPointFile_f()
{  
	char name[MAX_OSPATH + 1];
	snprintf(name, MAX_OSPATH, "maps/%s.pts", sv.name);
    name[MAX_OSPATH] = 0;

	FILE *f;
	COM_FOpenFile(name, &f);
	if (!f)
	{
		Con_Printf("couldn't open %s\n", name);
		return;
	}

	Con_Printf("Reading %s...\n", name);
	int c = 0;
	for (;; )
	{
        vec3_t org;
		int r = fscanf(f, "%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;

		if (!free_particles)
		{
			Con_Printf("Not enough free particles\n");
			break;
		}
        particle_t *p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = 99999;
		p->color = (-c) & 15;
		p->type = pt_static;
		VectorCopy(vec3_origin, p->vel);
		VectorCopy(org, p->org);
	}

	fclose(f);
	Con_Printf("%i points read\n", c);
}

void R_Particles_initialize()
{
	Cvar_RegisterVariable(&r_particles_min_size);
	Cvar_RegisterVariable(&r_particles_max_size);
	Cvar_RegisterVariable(&r_particles_size);
	Cvar_RegisterVariable(&r_particles_att_a);
	Cvar_RegisterVariable(&r_particles_att_b);
	Cvar_RegisterVariable(&r_particles_att_c);

	Cmd_AddCommand("pointfile", R_ReadPointFile_f);

	int i = COM_CheckParm("-particles");
	if (i)
	{
		r_numparticles = (int)(Q_atoi(com_argv[i + 1]));
		if (r_numparticles < ABSOLUTE_MIN_PARTICLES)
			r_numparticles = ABSOLUTE_MIN_PARTICLES;
	}
	else
	{
		r_numparticles = MAX_PARTICLES;
	}
	particles = (particle_t *)Hunk_AllocName(r_numparticles * sizeof(particle_t), "particles");

	R_InitParticleTexture();
}

void R_Particles_clear()
{
	free_particles = &particles[0];
	active_particles = NULL;

	for (int i = 0; i < r_numparticles; i++)
		particles[i].next = &particles[i + 1];
	particles[r_numparticles - 1].next = NULL;
}

void R_Particles_draw()
{
	int particleNb = 0;
	for (particle_t *p = active_particles; p; p = p->next)
		particleNb++;
    if (particleNb <= 0)
        return;

	float pixelWidthAtDepth1 = 2.0f * tanf(r_refdef.fov_x * Q_PI / 360.0f) / (float)r_refdef.vrect.width; // Pixel width if the near plane is at depth 1.0.
	// The dot is 14 pixels instead of 16 because of borders. Take it into account.
	pixelWidthAtDepth1 *= 16.0f / 14.0f;

	vec3_t up, right;
	VectorScale(r_viewUp, 1.0f, up);
	VectorScale(r_viewRight, 1.0f, right);

	oglwBindTexture(0, r_particleTexture);
    oglwEnableBlending(true);
	oglwEnableDepthWrite(false);
	oglwSetTextureBlending(0, GL_MODULATE);
	oglwBegin(GL_TRIANGLES);

	OglwVertex *vtx = oglwAllocateQuad(particleNb * 4);
    if (vtx != NULL)
    {
        for (particle_t *p = active_particles; p; p = p->next)
        {
            float dx = p->org[0] - r_viewOrigin[0], dy = p->org[1] - r_viewOrigin[1], dz = p->org[2] - r_viewOrigin[2];
            float distance2 = dx * dx + dy * dy + dz * dz;
            float distance = sqrtf(distance2);

            // Size in pixels, like OpenGLES.
            float size = R_Particles_computeSize(distance2, distance);

            // Size in world space.
            size = size * pixelWidthAtDepth1 * distance;

            byte *pc = (byte *)&d_8to24table[(int)p->color & 0xff];
            float pck = 1.0f / 255.0f;
            float r = pc[0] * pck, g = pc[1] * pck, b = pc[2] * pck, a = 1.0f;

            float rx = size * right[0], ry = size * right[1], rz = size * right[2];
            float ux = size * up[0], uy = size * up[1], uz = size * up[2];

            // Center each quad.
            float px = p->org[0] - 0.5f * (rx + ux);
            float py = p->org[1] - 0.5f * (ry + uy);
            float pz = p->org[2] - 0.5f * (rz + uz);

            vtx = AddVertex3D_CT1(vtx, px, py, pz, r, g, b, a, 0.0f, 0.0f);
            vtx = AddVertex3D_CT1(vtx, px + ux, py + uy, pz + uz, r, g, b, a, 1.0f, 0.0f);
            vtx = AddVertex3D_CT1(vtx, px + ux + rx, py + uy + ry, pz + uz + rz, r, g, b, a, 1.0f, 1.0f);
            vtx = AddVertex3D_CT1(vtx, px + rx, py + ry, pz + rz, r, g, b, a, 0.0f, 1.0f);
        }
    }
    
	oglwEnd();
	oglwEnableDepthWrite(true);
    oglwEnableBlending(false);
	oglwSetTextureBlending(0, GL_REPLACE);

	R_UpdateParticles();
}
