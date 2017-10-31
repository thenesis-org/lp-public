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
// Client sound i/o functions

#ifndef sound_h
#define sound_h

#include "Common/bspfile.h"
#include "Common/common.h"
#include "Common/cvar.h"
#include "Common/mathlib.h"
#include "Common/zone.h"

// It can be used safely if one thread is writing (only) and another is reading (only).
// getLengthAvailable(), isEmpty() and isFull() can be used safely on both threads.
typedef struct RingBuffer_ {
	int length;
	int writePosition;
	int writeCounter; // Wrapping counter.
	int readPosition;
	int readCounter; // Wrapping counter.
} RingBuffer;

void RingBuffer_initialize(RingBuffer *rb, int length);
void RingBuffer_reset(RingBuffer *rb);
bool RingBuffer_write(RingBuffer *rb, int n);
bool RingBuffer_read(RingBuffer *rb, int n);
int RingBuffer_getLengthAvailableForWriting(const RingBuffer *rb);
int RingBuffer_getLengthAvailableForReading(const RingBuffer *rb);
int RingBuffer_getWriteIndex(const RingBuffer *rb, int position);
int RingBuffer_getReadIndex(const RingBuffer *rb, int position);
bool RingBuffer_isEmpty(const RingBuffer *rb, int n);
bool RingBuffer_isFull(const RingBuffer *rb, int n);

typedef struct
{
	int channels;
	int samplebits;
	int frequency;

    // Thread safe ring buffer (no lock needed if only one read thread and only one write thread.
    short *buffer;
    RingBuffer ringBuffer;
} Audio;

extern Audio *g_audio;

bool Audio_initialize();
void Audio_finalize();

#define DEFAULT_SOUND_PACKET_VOLUME 255
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0

typedef struct sfx_s
{
	char name[MAX_QPATH];
	cache_user_t cache;
} sfx_t;

void S_Init();
void S_Shutdown();
void S_StartSound(int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation);
void S_StaticSound(sfx_t *sfx, vec3_t origin, float vol, float attenuation);
void S_StopSound(int entnum, int entchannel);
void S_StopAllSounds(qboolean clear);
void S_Update(vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up);
void S_ExtraUpdate();

sfx_t* S_PrecacheSound(char *sample);
void S_TouchSound(char *sample);

void S_LocalSound(char *s);

int CDAudio_Init();
void CDAudio_Play(byte track, qboolean looping);
void CDAudio_Stop();
void CDAudio_Pause();
void CDAudio_Resume();
void CDAudio_Shutdown();
void CDAudio_Update();

extern cvar_t s_loadas8bit;
extern cvar_t s_bgmvolume;
extern cvar_t s_volume;

#endif
