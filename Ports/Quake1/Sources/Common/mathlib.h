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
#ifndef mathlib_h
#define mathlib_h

#include "Common/common.h"

#include <math.h>

#define Q_PI 3.14159265358979323846f

#define IS_NAN(x) (isnan(x))

typedef int fixed4_t;
typedef int fixed8_t;
typedef int fixed16_t;

int Q_log2(int val);

void FloorDivMod(double numer, double denom, int *quotient, int *rem);

int GreatestCommonDivisor(int i1, int i2);

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec5_t[5];

extern vec3_t vec3_origin;

static inline float DotProduct(const float *x, const float *y)
{
    return x[0] * y[0] + x[1] * y[1] + x[2] * y[2];
}

static inline void VectorSubtract(const float *a, const float *b, float *c)
{
    float cx = a[0] - b[0];
    float cy = a[1] - b[1];
    float cz = a[2] - b[2];
    c[0] = cx;
    c[1] = cy;
    c[2] = cz;
}

static inline void VectorAdd(const float *a, const float *b, float *c)
{
    float cx = a[0] + b[0];
    float cy = a[1] + b[1];
    float cz = a[2] + b[2];
    c[0] = cx;
    c[1] = cy;
    c[2] = cz;
}

static inline void VectorCopy(const float *a, float *b)
{
    float bx = a[0];
    float by = a[1];
    float bz = a[2];
    b[0] = bx;
    b[1] = by;
    b[2] = bz;
}

static inline void VectorScale(const vec3_t in, vec_t scale, vec3_t out)
{
    float x = in[0] * scale;
    float y = in[1] * scale;
    float z = in[2] * scale;
	out[0] = x;
	out[1] = y;
	out[2] = z;
}

static inline void VectorInverse(vec3_t v)
{
    float x = -v[0];
    float y = -v[1];
    float z = -v[2];
	v[0] = x;
	v[1] = y;
	v[2] = z;
}

static inline void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross)
{
    float x = v1[1] * v2[2] - v1[2] * v2[1];
    float y = v1[2] * v2[0] - v1[0] * v2[2];
    float z = v1[0] * v2[1] - v1[1] * v2[0];
	cross[0] = x;
	cross[1] = y;
	cross[2] = z;
}

static inline vec_t Length(const vec3_t v)
{
	return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static inline bool VectorCompare(const vec3_t v1, const vec3_t v2)
{
    return v1[0] == v2[0] && v1[1] == v2[1] && v1[2] == v2[2];
}

static inline void VectorMA(const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc)
{
    float x = veca[0] + scale * vecb[0];
    float y = veca[1] + scale * vecb[1];
    float z = veca[2] + scale * vecb[2];
	vecc[0] = x;
	vecc[1] = y;
	vecc[2] = z;
}

static inline vec_t VectorDistance(const vec3_t a, const vec3_t b)
{
    float cx = a[0] - b[0];
    float cy = a[1] - b[1];
    float cz = a[2] - b[2];
	return sqrtf(cx * cx + cy * cy + cz * cz);
}

static inline vec_t Lerp(vec_t a, vec_t b, vec_t k)
{
    return a + k * (b - a);
}

float VectorNormalize(vec3_t v); // returns vector length

void R_ConcatRotations(float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4]);

float anglemod(float a);
void AngleVectors(vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);

typedef struct mplane_s
{
	vec3_t normal;
	float dist;
	byte type; // for texture axis selection and fast side tests
	byte signbits; // signx + signy<<1 + signz<<1
	byte pad[2];
} mplane_t;

void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);

int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct mplane_s *plane);

static inline int BOX_ON_PLANE_SIDE(vec3_t emins, vec3_t emaxs, struct mplane_s *p)
{
    int type = p->type;
    float dist = p->dist;
	return type < 3 ? (dist <= emins[type] ? 1 : (dist >= emaxs[type] ? 2 : 3)) : BoxOnPlaneSide(emins, emaxs, p);
}

#endif
