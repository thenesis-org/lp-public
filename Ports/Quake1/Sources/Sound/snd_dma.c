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
// Main control for any streaming sound output device

#include "Client/client.h"
#include "Client/console.h"
#include "Client/keys.h"
#include "Common/cmd.h"
#include "Common/quakedef.h"
#include "Common/sys.h"
#include "Rendering/gl_model.h"
#include "Sound/sound.h"

#include <stdlib.h>
#include <string.h>

void S_Play();
void S_PlayVol();
void S_SoundList();
void S_StopAllSounds(qboolean clear);
void S_StopAllSoundsC();

// =======================================================================
// Internal sound data & structures
// =======================================================================
channel_t channels[MAX_CHANNELS];
int total_channels;

static qboolean snd_ambient = true;
static qboolean snd_initialized = false;
static bool sound_started = false;

Audio *g_audio = NULL;

vec3_t listener_origin;
vec3_t listener_forward;
vec3_t listener_right;
vec3_t listener_up;
vec_t sound_nominal_clip_dist = 1000.0f;

#define MAX_SFX 512
static sfx_t *known_sfx; // hunk allocated [MAX_SFX]
static int num_sfx;

sfx_t *ambient_sfx[NUM_AMBIENTS];

cvar_t bgmvolume = { "bgmvolume", "1", true };
cvar_t volume = { "volume", "0.7", true };

cvar_t nosound = { "nosound", "0" };
cvar_t precache = { "precache", "1" };
cvar_t loadas8bit = { "loadas8bit", "0" };
cvar_t ambient_level = { "ambient_level", "0.3" };
cvar_t ambient_fade = { "ambient_fade", "100" };
cvar_t snd_noextraupdate = { "snd_noextraupdate", "0" };
cvar_t snd_show = { "snd_show", "0" };
cvar_t _snd_mixahead = { "_snd_mixahead", "0.1", true };

// ====================================================================
// User-setable variables
// ====================================================================

void S_AmbientOff()
{
	snd_ambient = false;
}

void S_AmbientOn()
{
	snd_ambient = true;
}

void S_SoundInfo_f()
{
	if (!sound_started || !g_audio)
	{
		Con_Printf("sound system not started\n");
		return;
	}
    Audio *audio = g_audio;
	Con_Printf("%5d stereo\n", audio->channels - 1);
	Con_Printf("%5d samplebits\n", audio->samplebits);
	Con_Printf("%5d frequency\n", audio->frequency);
	Con_Printf("%5d total_channels\n", total_channels);
}

void S_Init()
{
	Con_Printf("\nSound Initialization\n");

	if (COM_CheckParm("-nosound"))
		return;

	Cmd_AddCommand("play", S_Play);
	Cmd_AddCommand("playvol", S_PlayVol);
	Cmd_AddCommand("stopsound", S_StopAllSoundsC);
	Cmd_AddCommand("soundlist", S_SoundList);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);

	Cvar_RegisterVariable(&nosound);
	Cvar_RegisterVariable(&volume);
	Cvar_RegisterVariable(&precache);
	Cvar_RegisterVariable(&loadas8bit);
	Cvar_RegisterVariable(&bgmvolume);
	Cvar_RegisterVariable(&ambient_level);
	Cvar_RegisterVariable(&ambient_fade);
	Cvar_RegisterVariable(&snd_noextraupdate);
	Cvar_RegisterVariable(&snd_show);
	Cvar_RegisterVariable(&_snd_mixahead);

	if (host_parms.memsize < 0x800000)
	{
		Cvar_Set("loadas8bit", "1");
		Con_Printf("loading all sounds as 8bit\n");
	}

	snd_initialized = true;

    int rc = Audio_initialize();
    if (!rc)
    {
        Con_Printf("S_Startup: Audio_initialize failed.\n");
        sound_started = false;
        return;
    }
	sound_started = true;

	known_sfx = Hunk_AllocName(MAX_SFX * sizeof(sfx_t), "sfx_t");
	num_sfx = 0;


	Con_Printf("Sound sampling rate: %i\n", g_audio->frequency);

	ambient_sfx[AMBIENT_WATER] = S_PrecacheSound("ambience/water1.wav");
	ambient_sfx[AMBIENT_SKY] = S_PrecacheSound("ambience/wind2.wav");

	S_StopAllSounds(true);
}

void S_Shutdown()
{
	if (!sound_started)
		return;
	sound_started = false;

	Audio_finalize();
}

// Load a sound
sfx_t* S_FindName(char *name)
{
	if (!name)
		Sys_Error("S_FindName: NULL\n");

	if (Q_strlen(name) >= MAX_QPATH)
		Sys_Error("Sound name too long: %s", name);

	// see if already loaded
    int i;
	for (i = 0; i < num_sfx; i++)
		if (!Q_strcmp(known_sfx[i].name, name))
		{
			return &known_sfx[i];
		}

	if (num_sfx == MAX_SFX)
		Sys_Error("S_FindName: out of sfx_t");

	sfx_t *sfx = &known_sfx[i];
	strcpy(sfx->name, name);

	num_sfx++;

	return sfx;
}

void S_TouchSound(char *name)
{
	if (!sound_started)
		return;

	sfx_t *sfx = S_FindName(name);
	Cache_Check(&sfx->cache);
}

sfx_t* S_PrecacheSound(char *name)
{
	if (!sound_started || nosound.value)
		return NULL;

	sfx_t *sfx = S_FindName(name);

	// cache it in
	if (precache.value)
		S_LoadSound(sfx);

	return sfx;
}

//=============================================================================

channel_t* SND_PickChannel(int entnum, int entchannel)
{
	// Check for replacement sound, or find the best one to replace
	int first_to_die = -1;
	int life_left = 0x7fffffff;
	for (int ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++)
	{
		if (entchannel != 0 // channel 0 never overrides
		    && channels[ch_idx].entnum == entnum
		    && (channels[ch_idx].entchannel == entchannel || entchannel == -1)) // allways override sound from same entity
		{
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (channels[ch_idx].entnum == cl.viewentity && entnum != cl.viewentity && channels[ch_idx].sfx)
			continue;

		if (channels[ch_idx].length < life_left)
		{
			life_left = channels[ch_idx].length;
			first_to_die = ch_idx;
		}
	}

	if (first_to_die == -1)
		return NULL;

	if (channels[first_to_die].sfx)
		channels[first_to_die].sfx = NULL;

	return &channels[first_to_die];
}

void SND_Spatialize(channel_t *ch)
{
	// anything coming from the view entity will allways be full volume
	if (ch->entnum == cl.viewentity)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	// calculate stereo separation and distance attenuation
	vec3_t source_vec;
	VectorSubtract(ch->origin, listener_origin, source_vec);
	vec_t dist = VectorNormalize(source_vec) * ch->dist_mult;
	vec_t dot = DotProduct(listener_right, source_vec);

	vec_t lscale, rscale;
	if (g_audio->channels == 1)
	{
		rscale = 1.0f;
		lscale = 1.0f;
	}
	else
	{
		rscale = 1.0f + dot;
		lscale = 1.0f - dot;
	}

	// add in distance effect
    vec_t scale;
    int master_vol = ch->master_vol;
    
	scale = (1.0f - dist) * rscale;
    int rightvol = (int)(master_vol * scale);
	if (rightvol < 0)
		rightvol = 0;
	ch->rightvol = rightvol;

	scale = (1.0f - dist) * lscale;
    int leftvol = (int)(master_vol * scale);
	if (leftvol < 0)
		leftvol = 0;
	ch->leftvol = leftvol;
}

// Start a sound effect
void S_StartSound(int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
	if (!sound_started)
		return;

	if (!sfx)
		return;

	if (nosound.value)
		return;

	int vol = fvol * 255;

	// pick a channel to play on
	channel_t *target_chan = SND_PickChannel(entnum, entchannel);
	if (!target_chan)
		return;

	// spatialize
	memset(target_chan, 0, sizeof(*target_chan));
	VectorCopy(origin, target_chan->origin);
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	SND_Spatialize(target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol)
		return;                                               // not audible at all

	// new channel
	sfxcache_t *sc = S_LoadSound(sfx);
	if (!sc)
	{
		target_chan->sfx = NULL;
		return; // couldn't load the sound's data
	}

	target_chan->sfx = sfx;
	target_chan->pos = 0;
	target_chan->length = sc->length;

	// if an identical sound has also been started this frame, offset the pos
	// a bit to keep it from just making the first one louder
    int skipMax = (int)(0.1f * g_audio->frequency);
	channel_t *check = &channels[NUM_AMBIENTS];
	for (int ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++, check++)
	{
		if (check == target_chan)
			continue;
		if (check->sfx == sfx && !check->pos)
		{
			int skip = rand() % skipMax;
            int length = target_chan->length;
			if (skip >= length)
				skip = length - 1;
			target_chan->pos += skip;
			target_chan->length = length - skip;
			break;
		}
	}
}

void S_StopSound(int entnum, int entchannel)
{
	for (int i = 0; i < MAX_DYNAMIC_CHANNELS; i++)
	{
		if (channels[i].entnum == entnum && channels[i].entchannel == entchannel)
		{
			channels[i].length = 0;
			channels[i].sfx = NULL;
			return;
		}
	}
}

void S_StopAllSounds(qboolean clear)
{
	if (!sound_started)
		return;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS; // no statics

	for (int i = 0; i < MAX_CHANNELS; i++)
		if (channels[i].sfx)
			channels[i].sfx = NULL;


	Q_memset(channels, 0, MAX_CHANNELS * sizeof(channel_t));
}

void S_StopAllSoundsC()
{
	S_StopAllSounds(true);
}

void S_StaticSound(sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	if (!sfx)
		return;

	if (total_channels == MAX_CHANNELS)
	{
		Con_Printf("total_channels == MAX_CHANNELS\n");
		return;
	}

	channel_t *ss = &channels[total_channels];
	total_channels++;

	sfxcache_t *sc = S_LoadSound(sfx);
	if (!sc)
		return;

	if (sc->loopstart == -1)
	{
		Con_Printf("Sound %s not looped\n", sfx->name);
		return;
	}

	ss->sfx = sfx;
	VectorCopy(origin, ss->origin);
	ss->master_vol = vol;
	ss->dist_mult = (attenuation / 64) / sound_nominal_clip_dist;
    ss->pos = 0;
	ss->length = sc->length;

	SND_Spatialize(ss);
}

static void S_UpdateAmbientSounds()
{
	if (!snd_ambient)
		return;

	if (!cl.worldmodel)
		return;

	mleaf_t *l = Mod_PointInLeaf(listener_origin, cl.worldmodel);
	if (!l || !ambient_level.value)
	{
		for (int ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++)
			channels[ambient_channel].sfx = NULL;
		return;
	}

    int master_vol_step = (int)(host_frametime * ambient_fade.value);
    float level = ambient_level.value;
	for (int ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++)
	{
		channel_t *chan = &channels[ambient_channel];
		chan->sfx = ambient_sfx[ambient_channel];

		int vol = (int)(level * l->ambient_sound_level[ambient_channel]);
		if (vol < 8)
			vol = 0;

		// don't adjust volume too fast
        int master_vol = chan->master_vol;
		if (master_vol < vol)
		{
			master_vol += master_vol_step;
			if (master_vol > vol)
				master_vol = vol;
		}
		else if (chan->master_vol > vol)
		{
			master_vol -= master_vol_step;
			if (master_vol < vol)
				master_vol = vol;
		}
		chan->master_vol = chan->leftvol = chan->rightvol = master_vol;
	}
}

void S_Update_()
{
	if (!sound_started)
		return;

    Audio *audio = g_audio;

    int frameNb = (int)(_snd_mixahead.value * audio->frequency);

    // We avoid to mix too far ahead to avoid having too much latency.
    int frameAvailableForReading = RingBuffer_getLengthAvailableForReading(&audio->ringBuffer);
    frameNb -= frameAvailableForReading;

    // We cannot mix more than the remaining size of the buffer.
    int frameAvailableForWriting = RingBuffer_getLengthAvailableForWriting(&audio->ringBuffer);
    if (frameNb > frameAvailableForWriting)
        frameNb = frameAvailableForWriting;

	S_PaintChannels(frameNb);
}

/*
   Called once each time through the main loop
 */
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	if (!sound_started)
		return;

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);

	// update general area ambient sound sources
	S_UpdateAmbientSounds();

	channel_t *combine = NULL;

	// update spatialization for static and dynamic sounds
	channel_t *ch = channels + NUM_AMBIENTS;
	for (int i = NUM_AMBIENTS; i < total_channels; i++, ch++)
	{
		if (!ch->sfx)
			continue;
		SND_Spatialize(ch); // respatialize channel
		if (!ch->leftvol && !ch->rightvol)
			continue;

		// try to combine static sounds with a previous channel of the same
		// sound effect so we don't mix five torches every frame

		if (i >= MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS)
		{
			// see if it can just use the last one
			if (combine && combine->sfx == ch->sfx)
			{
				combine->leftvol += ch->leftvol;
				combine->rightvol += ch->rightvol;
				ch->leftvol = ch->rightvol = 0;
				continue;
			}
			// search for one
			combine = channels + MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;
            int j;
			for (j = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS; j < i; j++, combine++)
				if (combine->sfx == ch->sfx)
					break;
			if (j == total_channels)
				combine = NULL;
			else
			{
				if (combine != ch)
				{
					combine->leftvol += ch->leftvol;
					combine->rightvol += ch->rightvol;
					ch->leftvol = ch->rightvol = 0;
				}
				continue;
			}
		}
	}

	//
	// debugging output
	//
	if (snd_show.value)
	{
		int total = 0;
		ch = channels;
		for (int i = 0; i < total_channels; i++, ch++)
			if (ch->sfx && (ch->leftvol || ch->rightvol))
			{
				//Con_Printf ("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}

		Con_Printf("----(%i)----\n", total);
	}

	// mix some sound
	S_Update_();
}

void S_ExtraUpdate()
{
	if (snd_noextraupdate.value)
		return; // don't pollute timings
	S_Update_();
}

/*
   ===============================================================================

   console functions

   ===============================================================================
 */

void S_Play()
{
	static int hash = 345;
	int i = 1;
	while (i < Cmd_Argc())
	{
        char name[256];
		if (!Q_strrchr(Cmd_Argv(i), '.'))
		{
			Q_strcpy(name, Cmd_Argv(i));
			Q_strcat(name, ".wav");
		}
		else
			Q_strcpy(name, Cmd_Argv(i));
		sfx_t *sfx = S_PrecacheSound(name);
		S_StartSound(hash++, 0, sfx, listener_origin, 1.0, 1.0);
		i++;
	}
}

void S_PlayVol()
{
	static int hash = 543;
	int i = 1;
	while (i < Cmd_Argc())
	{
        char name[256];
		if (!Q_strrchr(Cmd_Argv(i), '.'))
		{
			Q_strcpy(name, Cmd_Argv(i));
			Q_strcat(name, ".wav");
		}
		else
			Q_strcpy(name, Cmd_Argv(i));
		sfx_t *sfx = S_PrecacheSound(name);
		float vol = Q_atof(Cmd_Argv(i + 1));
		S_StartSound(hash++, 0, sfx, listener_origin, vol, 1.0);
		i += 2;
	}
}

void S_SoundList()
{
	int i;
	sfx_t *sfx;
	int total = 0;
	for (sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++)
	{
		sfxcache_t *sc = Cache_Check(&sfx->cache);
		if (!sc)
			continue;
		int size = sc->length * sc->width * (sc->stereo + 1);
		total += size;
		if (sc->loopstart >= 0)
			Con_Printf("L");
		else
			Con_Printf(" ");
		Con_Printf("(%2db) %6i : %s\n", sc->width * 8, size, sfx->name);
	}
	Con_Printf("Total resident: %i\n", total);
}

void S_LocalSound(char *sound)
{
	if (nosound.value)
		return;
	if (!sound_started)
		return;
	sfx_t *sfx = S_PrecacheSound(sound);
	if (!sfx)
	{
		Con_Printf("S_LocalSound: can't cache %s\n", sound);
		return;
	}
	S_StartSound(cl.viewentity, -1, sfx, vec3_origin, 1, 1);
}
