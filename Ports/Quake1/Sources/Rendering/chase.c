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
// Chase camera code

#include "Common/cvar.h"
#include "Common/quakedef.h"
#include "Client/client.h"
#include "Server/server.h"
#include "Server/world.h"

#include <string.h>

cvar_t chase_back = { "chase_back", "100" };
cvar_t chase_up = { "chase_up", "16" };
cvar_t chase_right = { "chase_right", "0" };
cvar_t chase_active = { "chase_active", "0" };

void Chase_Init()
{
	Cvar_RegisterVariable(&chase_back);
	Cvar_RegisterVariable(&chase_up);
	Cvar_RegisterVariable(&chase_right);
	Cvar_RegisterVariable(&chase_active);
}

void Chase_Reset()
{
	// for respawning and teleporting start position 12 units behind head
}

static void Chase_traceLine(vec3_t start, vec3_t end, vec3_t impact)
{
	trace_t trace;
	memset(&trace, 0, sizeof(trace));
	SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0, 1, start, end, &trace);
	VectorCopy(trace.endpos, impact);
}

void Chase_Update()
{
	// if can't see player, reset
	vec3_t forward, up, right;
	AngleVectors(cl.viewangles, forward, right, up);

	// calc exact destination
    vec3_t chase_dest;
	for (int i = 0; i < 3; i++)
		chase_dest[i] = r_refdef.vieworg[i]
		        - forward[i] * chase_back.value
		        - right[i] * chase_right.value;
	chase_dest[2] = r_refdef.vieworg[2] + chase_up.value;

	// find the spot the player is looking at
	vec3_t dest, stop;
	VectorMA(r_refdef.vieworg, 4096, forward, dest);
	Chase_traceLine(r_refdef.vieworg, dest, stop);

	// calculate pitch to look at the same spot from camera
	VectorSubtract(stop, r_refdef.vieworg, stop);
	float dist = DotProduct(stop, forward);
	if (dist < 1)
		dist = 1;
	r_refdef.viewangles[PITCH] = -atan(stop[2] / dist) / Q_PI * 180;

	// move towards destination
	VectorCopy(chase_dest, r_refdef.vieworg);
}
