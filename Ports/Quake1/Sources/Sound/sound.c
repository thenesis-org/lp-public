#include "Client/client.h"
#include "Client/console.h"
#include "Client/keys.h"
#include "Common/cmd.h"
#include "Common/quakedef.h"
#include "Common/sys.h"
#include "Rendering/r_model.h"
#include "Sound/sound.h"

#include <stdlib.h>
#include <string.h>

Audio *g_audio = NULL;

static bool s_initialized = false;
static bool s_started = false;
static bool s_ambient = true;

static vec3_t s_listener_origin;
static vec3_t s_listener_forward;
static vec3_t s_listener_right;
static vec3_t s_listener_up;
static vec_t s_nominal_clip_dist = 1000.0f;

cvar_t s_bgmvolume = { "s_bgmvolume", "1", true };
cvar_t s_volume = { "s_volume", "0.7", true };
cvar_t s_disabled = { "s_disabled", "0" };
cvar_t s_precache = { "s_precache", "1" };
cvar_t s_loadas8bit = { "s_loadas8bit", "0" };
cvar_t s_ambient_level = { "s_ambient_level", "0.3" };
cvar_t s_ambient_fade = { "s_ambient_fade", "100" };
cvar_t s_noextraupdate = { "s_noextraupdate", "0" };
cvar_t s_log = { "s_log", "0" };
cvar_t s_mixahead = { "s_mixahead", "0.1", true };

//--------------------------------------------------------------------------------
// Wav file loading.
//--------------------------------------------------------------------------------
typedef struct
{
	int rate;
	int width;
	int channels;
	int loopstart;
	int samples;
	int dataofs; // chunk starts this many bytes from file start
} wavinfo_t;

typedef struct WavContext_ {
    byte *data_p;
    byte *iff_end;
    byte *last_chunk;
    byte *iff_data;
    int iff_chunk_len;
} WavContext;

static short S_WaveFile_getLittleShort(WavContext *wavContext)
{
    byte *d = wavContext->data_p;
	wavContext->data_p = d + 2;
	short val = d[0] | (d[1] << 8);
	return val;
}

static int S_WaveFile_getLittleLong(WavContext *wavContext)
{
    byte *d = wavContext->data_p;
	wavContext->data_p = d + 4;
	int val = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
	return val;
}

static void S_WaveFile_findNextChunk(WavContext *wavContext, char *name)
{
	while (1)
	{
		wavContext->data_p = wavContext->last_chunk;

		if (wavContext->data_p >= wavContext->iff_end) // didn't find the chunk
		{
			wavContext->data_p = NULL;
			return;
		}

		wavContext->data_p += 4;
		wavContext->iff_chunk_len = S_WaveFile_getLittleLong(wavContext);
		if (wavContext->iff_chunk_len < 0)
		{
			wavContext->data_p = NULL;
			return;
		}
		//		if (iff_chunk_len > 1024*1024)
		//			Sys_Error ("S_WaveFile_findNextChunk: %i length is past the 1 meg sanity limit", iff_chunk_len);
		wavContext->data_p -= 8;
		wavContext->last_chunk = wavContext->data_p + 8 + ((wavContext->iff_chunk_len + 1) & ~1);
		if (!Q_strncmp((char *)wavContext->data_p, name, 4))
			return;
	}
}

static void S_WaveFile_findChunk(WavContext *wavContext, char *name)
{
	wavContext->last_chunk = wavContext->iff_data;
	S_WaveFile_findNextChunk(wavContext, name);
}

void S_WaveFile_dumpChunks(WavContext *wavContext)
{
	char str[5];
	str[4] = 0;
	wavContext->data_p = wavContext->iff_data;
	do
	{
		memcpy(str, wavContext->data_p, 4);
		wavContext->data_p += 4;
		wavContext->iff_chunk_len = S_WaveFile_getLittleLong(wavContext);
		Con_Printf("0x%08x : %s (%d)\n", (uintptr_t)(wavContext->data_p - 4), str, wavContext->iff_chunk_len);
		wavContext->data_p += (wavContext->iff_chunk_len + 1) & ~1;
	}
	while (wavContext->data_p < wavContext->iff_end);
}

static wavinfo_t S_WaveFile_getInfo(char *name, byte *wav, int wavlength)
{
	wavinfo_t info;
	memset(&info, 0, sizeof(info));

	if (!wav)
		return info;

    WavContext wavContext_;
    WavContext *wavContext = &wavContext_;

	wavContext->iff_data = wav;
	wavContext->iff_end = wav + wavlength;

	// find "RIFF" chunk
	S_WaveFile_findChunk(wavContext, "RIFF");
	if (!(wavContext->data_p && !Q_strncmp((char *)wavContext->data_p + 8, "WAVE", 4)))
	{
		Con_Printf("Missing RIFF/WAVE chunks\n");
		return info;
	}

	// get "fmt " chunk
	wavContext->iff_data = wavContext->data_p + 12;
	// S_WaveFile_dumpChunks();

	S_WaveFile_findChunk(wavContext, "fmt ");
	if (!wavContext->data_p)
	{
		Con_Printf("Missing fmt chunk\n");
		return info;
	}
	wavContext->data_p += 8;
	int format = S_WaveFile_getLittleShort(wavContext);
	if (format != 1)
	{
		Con_Printf("Microsoft PCM format only\n");
		return info;
	}

	info.channels = S_WaveFile_getLittleShort(wavContext);
	info.rate = S_WaveFile_getLittleLong(wavContext);
	wavContext->data_p += 4 + 2;
	info.width = S_WaveFile_getLittleShort(wavContext) / 8;

	// get cue chunk
	S_WaveFile_findChunk(wavContext, "cue ");
	if (wavContext->data_p)
	{
		wavContext->data_p += 32;
		info.loopstart = S_WaveFile_getLittleLong(wavContext);
		//		Con_Printf("loopstart=%d\n", sfx->loopstart);

		// if the next chunk is a LIST chunk, look for a cue length marker
		S_WaveFile_findNextChunk(wavContext, "LIST");
		if (wavContext->data_p)
		{
			if (!strncmp((const char *)wavContext->data_p + 28, "mark", 4)) // this is not a proper parse, but it works with cooledit...
			{
				wavContext->data_p += 24;
				int i = S_WaveFile_getLittleLong(wavContext); // samples in loop
				info.samples = info.loopstart + i;
				//	Con_Printf("looped length: %i\n", i);
			}
		}
	}
	else
		info.loopstart = -1;

	// find data chunk
	S_WaveFile_findChunk(wavContext, "data");
	if (!wavContext->data_p)
	{
		Con_Printf("Missing data chunk\n");
		return info;
	}

	wavContext->data_p += 4;
	int samples = S_WaveFile_getLittleLong(wavContext) / info.width;

	if (info.samples)
	{
		if (samples < info.samples)
			Sys_Error("Sound %s has a bad loop length", name);
	}
	else
		info.samples = samples;

	info.dataofs = wavContext->data_p - wav;

	return info;
}

//--------------------------------------------------------------------------------
// Sound loading.
//--------------------------------------------------------------------------------
typedef struct
{
	int length;
	int loopstart;
	int speed;
	int width;
	int stereo;
	byte data[1]; // variable sized
} sfxcache_t;

static void S_resampleSfx(sfx_t *sfx, int inrate, int inwidth, byte *data)
{
	sfxcache_t *sc = Cache_Check(&sfx->cache);
	if (!sc)
		return;

	float stepscale = (float)inrate / g_audio->frequency; // this is usually 0.5, 1, or 2

	int outcount = sc->length / stepscale;
	sc->length = outcount;
	if (sc->loopstart != -1)
		sc->loopstart = sc->loopstart / stepscale;

	sc->speed = g_audio->frequency;
	if (s_loadas8bit.value)
		sc->width = 1;
	else
		sc->width = inwidth;
	sc->stereo = 0;

	// resample / decimate to the current source rate

	if (stepscale == 1 && inwidth == 1 && sc->width == 1)
	{
		// fast special case
		for (int i = 0; i < outcount; i++)
			((signed char *)sc->data)[i] = (int)((unsigned char)(data[i]) - 128);
	}
	else
	{
		// general case
		int samplefrac = 0;
		int fracstep = stepscale * 256;
		for (int i = 0; i < outcount; i++)
		{
			int srcsample = samplefrac >> 8;
			samplefrac += fracstep;
            int sample;
			if (inwidth == 2)
				sample = LittleShort(((short *)data)[srcsample]);
			else
				sample = (int)((unsigned char)(data[srcsample]) - 128) << 8;
			if (sc->width == 2)
				((short *)sc->data)[i] = sample;
			else
				((signed char *)sc->data)[i] = sample >> 8;
		}
	}
}

static sfxcache_t* S_LoadSound(sfx_t *s)
{
	// see if still in memory
	sfxcache_t *sc = Cache_Check(&s->cache);
	if (sc)
		return sc;

	char namebuffer[256];
	Q_strncpy(namebuffer, "sound/", 256);
	Q_strncat(namebuffer, s->name, 256);
	//	Con_Printf ("loading %s\n",namebuffer);

	byte stackbuf[1 * 1024]; // avoid dirtying the cache heap
	byte *data = COM_LoadStackFile(namebuffer, stackbuf, sizeof(stackbuf));
	if (!data)
	{
		Con_Printf("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	wavinfo_t info = S_WaveFile_getInfo(s->name, data, com_filesize);
	if (info.channels != 1)
	{
		Con_Printf("%s is a stereo sample\n", s->name);
		return NULL;
	}

	float stepscale = (float)info.rate / g_audio->frequency;
	int len = info.samples / stepscale;
	len = len * info.width * info.channels;
	sc = Cache_Alloc(&s->cache, len + sizeof(sfxcache_t), s->name);
	if (!sc)
		return NULL;

	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	S_resampleSfx(s, sc->speed, sc->width, data + info.dataofs);

	return sc;
}

//--------------------------------------------------------------------------------
// Sound cache.
//--------------------------------------------------------------------------------
#define MAX_SFX 512
static int s_sfx_nb;
static sfx_t *s_sfx; // hunk allocated [MAX_SFX]
static sfx_t *s_sfx_ambient[NUM_AMBIENTS];

// Load a sound
sfx_t* S_FindName(char *name)
{
	if (!name)
		Sys_Error("S_FindName: NULL\n");

	if (Q_strlen(name) >= MAX_QPATH)
		Sys_Error("Sound name too long: %s", name);

	// see if already loaded
    int i;
	for (i = 0; i < s_sfx_nb; i++)
		if (!Q_strcmp(s_sfx[i].name, name))
		{
			return &s_sfx[i];
		}

	if (s_sfx_nb == MAX_SFX)
		Sys_Error("S_FindName: out of sfx_t");

	sfx_t *sfx = &s_sfx[i];
	Q_strncpy(sfx->name, name, MAX_QPATH);

	s_sfx_nb++;

	return sfx;
}

void S_TouchSound(char *name)
{
	if (!s_started)
		return;

	sfx_t *sfx = S_FindName(name);
	Cache_Check(&sfx->cache);
}

sfx_t* S_PrecacheSound(char *name)
{
	if (!s_started || s_disabled.value)
		return NULL;

	sfx_t *sfx = S_FindName(name);

	// cache it in
	if (s_precache.value)
		S_LoadSound(sfx);

	return sfx;
}

//--------------------------------------------------------------------------------
// Channel.
//--------------------------------------------------------------------------------
typedef struct
{
	sfx_t *sfx; // sfx number
	int master_vol; // 0-255 master volume
	int leftvol; // 0-255 volume
	int rightvol; // 0-255 volume
	int length; // length in sample frames
	int pos; // sample position in sfx
	int looping; // where to loop, -1 = no looping
	int entnum; // to allow overriding a specific sound
	int entchannel; //
	vec3_t origin; // origin of sound effect
	vec_t dist_mult; // distance multiplier (attenuation/clipK)
} channel_t;

// 0 to MAX_DYNAMIC_CHANNELS-1	= normal entity sounds
// MAX_DYNAMIC_CHANNELS to MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS -1 = water, etc
// MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS to total_channels = static sounds
#define MAX_CHANNELS 128
#define MAX_DYNAMIC_CHANNELS 8
static channel_t s_channels[MAX_CHANNELS];
static int total_channels;

static channel_t* S_pickChannel(int entnum, int entchannel)
{
	// Check for replacement sound, or find the best one to replace
	int first_to_die = -1;
	int life_left = 0x7fffffff;
	for (int ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++)
	{
		if (entchannel != 0 // channel 0 never overrides
		    && s_channels[ch_idx].entnum == entnum
		    && (s_channels[ch_idx].entchannel == entchannel || entchannel == -1)) // allways override sound from same entity
		{
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (s_channels[ch_idx].entnum == cl.viewentity && entnum != cl.viewentity && s_channels[ch_idx].sfx)
			continue;

		if (s_channels[ch_idx].length < life_left)
		{
			life_left = s_channels[ch_idx].length;
			first_to_die = ch_idx;
		}
	}

	if (first_to_die == -1)
		return NULL;

	if (s_channels[first_to_die].sfx)
		s_channels[first_to_die].sfx = NULL;

	return &s_channels[first_to_die];
}

static void S_spatialize(channel_t *ch)
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
	VectorSubtract(ch->origin, s_listener_origin, source_vec);
	vec_t dist = VectorNormalize(source_vec) * ch->dist_mult;
	vec_t dot = DotProduct(s_listener_right, source_vec);

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
	if (!s_started)
		return;

	if (!sfx)
		return;

	if (s_disabled.value)
		return;

	int vol = fvol * 255;

	// pick a channel to play on
	channel_t *target_chan = S_pickChannel(entnum, entchannel);
	if (!target_chan)
		return;

	// spatialize
	memset(target_chan, 0, sizeof(*target_chan));
	VectorCopy(origin, target_chan->origin);
	target_chan->dist_mult = attenuation / s_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	S_spatialize(target_chan);

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
	channel_t *check = &s_channels[NUM_AMBIENTS];
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
		if (s_channels[i].entnum == entnum && s_channels[i].entchannel == entchannel)
		{
			s_channels[i].length = 0;
			s_channels[i].sfx = NULL;
			return;
		}
	}
}

void S_StopAllSounds(qboolean clear)
{
	if (!s_started)
		return;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS; // no statics

	for (int i = 0; i < MAX_CHANNELS; i++)
		if (s_channels[i].sfx)
			s_channels[i].sfx = NULL;


	Q_memset(s_channels, 0, MAX_CHANNELS * sizeof(channel_t));
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

	channel_t *ss = &s_channels[total_channels];
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
	ss->dist_mult = (attenuation / 64) / s_nominal_clip_dist;
    ss->pos = 0;
	ss->length = sc->length;

	S_spatialize(ss);
}

void S_LocalSound(char *sound)
{
	if (s_disabled.value)
		return;
	if (!s_started)
		return;
	sfx_t *sfx = S_PrecacheSound(sound);
	if (!sfx)
	{
		Con_Printf("S_LocalSound: can't cache %s\n", sound);
		return;
	}
	S_StartSound(cl.viewentity, -1, sfx, vec3_origin, 1, 1);
}

//--------------------------------------------------------------------------------
// Ring buffer.
// Thread safe if only one thread read and only one thread write (cannot mix both).
//--------------------------------------------------------------------------------
enum {
	RingBufferCounterMask = 0x7fffffff
};

void RingBuffer_initialize(RingBuffer *rb, int length) {
	rb->length = length;
	RingBuffer_reset(rb);
}

void RingBuffer_reset(RingBuffer *rb) {
	rb->writePosition = 0;
	rb->writeCounter = 0;
	rb->readPosition = 0;
	rb->readCounter = 0;
}

bool RingBuffer_write(RingBuffer *rb, int n) {
	if (RingBuffer_isFull(rb, n))
		return false;
	rb->writePosition = (rb->writePosition + n) % rb->length;
	rb->writeCounter = (rb->writeCounter + n) & RingBufferCounterMask;
	return true;
}

bool RingBuffer_read(RingBuffer *rb, int n) {
	if (RingBuffer_isEmpty(rb, n))
		return false;
	rb->readPosition = (rb->readPosition + n) % rb->length;
	rb->readCounter = (rb->readCounter + n) & RingBufferCounterMask;
	return true;
}

int RingBuffer_getLengthAvailableForWriting(const RingBuffer *rb) {
	int lu = (rb->writeCounter - rb->readCounter) & RingBufferCounterMask;
	int la = rb->length - lu;
	return la;
}

int RingBuffer_getLengthAvailableForReading(const RingBuffer *rb) {
	int lu = (rb->writeCounter - rb->readCounter) & RingBufferCounterMask;
	return lu;
}

int RingBuffer_getWriteIndex(const RingBuffer *rb, int position) {
	int la = RingBuffer_getLengthAvailableForWriting(rb);
	if (position < -la || position >= la)
		return -1;
	if (position < 0)
		position += la;
	return (rb->writePosition + position) % rb->length;
}

int RingBuffer_getReadIndex(const RingBuffer *rb, int position) {
	int la = RingBuffer_getLengthAvailableForReading(rb);
	if (position < -la || position >= la)
		return -1;
	if (position < 0)
		position += la;
	return (rb->readPosition + position) % rb->length;
}

bool RingBuffer_isEmpty(const RingBuffer *rb, int n) {
	return RingBuffer_getLengthAvailableForReading(rb) < n;
}

bool RingBuffer_isFull(const RingBuffer *rb, int n) {
	return RingBuffer_getLengthAvailableForWriting(rb) < n;
}

//--------------------------------------------------------------------------------
// Conversion.
//--------------------------------------------------------------------------------
static void Audio_convertBuffer(int frameNb, int *paintBuffer, void *outputBuffer, int outputBufferSize, int outputBufferPosition, int volume)
{
    int outputBufferMask = outputBufferSize - 1;
    while (frameNb > 0)
    {
        short *outputBuffer16 = (short*)outputBuffer + (outputBufferPosition << 1);
        int loopFrameNb = outputBufferSize - outputBufferPosition;
        if (loopFrameNb > frameNb)
            loopFrameNb = frameNb;
        frameNb -= loopFrameNb;
        outputBufferPosition = (outputBufferPosition + loopFrameNb) & outputBufferMask;
        for (int i = 0; i < loopFrameNb; i++, paintBuffer += 2, outputBuffer16 += 2)
        {
            int val;

            val = (paintBuffer[0] * volume) >> 8;
            if (val > 0x7fff)
                val = 0x7fff;
            if (val < (short)0x8000)
                val = (short)0x8000;
            outputBuffer16[0] = (short)val;

            val = (paintBuffer[1] * volume) >> 8;
            if (val > 0x7fff)
                val = 0x7fff;
            if (val < (short)0x8000)
                val = (short)0x8000;
            outputBuffer16[1] = (short)val;
        }
    }
}

//--------------------------------------------------------------------------------
// Mixing.
//--------------------------------------------------------------------------------
#define PAINTBUFFER_SIZE 512
static int s_paintBuffer[PAINTBUFFER_SIZE * 2];

static void Audio_PaintChannelFrom8(channel_t *ch, sfxcache_t *sc, int count, int *paintBuffer)
{
	int leftvol = ch->leftvol;
	int rightvol = ch->rightvol;
	unsigned char *sfx = (unsigned char *)sc->data + ch->pos;
	for (int i = 0; i < count; i++, paintBuffer += 2)
	{
		int data = sfx[i];
//        if (data >= 128) data -= 0xff;
        data -= ((signed char)data >> 7) & 0xff;
		paintBuffer[0] += (data * leftvol);
		paintBuffer[1] += (data * rightvol);
	}
}

static void Audio_PaintChannelFrom16(channel_t *ch, sfxcache_t *sc, int count, int *paintBuffer)
{
	int leftvol = ch->leftvol;
	int rightvol = ch->rightvol;
	signed short *sfx = (signed short *)sc->data + ch->pos;
	for (int i = 0; i < count; i++, paintBuffer += 2)
	{
		int data = sfx[i];
		paintBuffer[0] += (data * leftvol) >> 8;
		paintBuffer[1] += (data * rightvol) >> 8;
	}
}

static void S_PaintChannels(int frameNb)
{
    Audio *audio = g_audio;
    int volumeInt = (int)(s_volume.value * 256);
	while (frameNb > 0)
	{
		int loopFrameNb = frameNb;
		if (loopFrameNb > PAINTBUFFER_SIZE)
			loopFrameNb = PAINTBUFFER_SIZE;
        frameNb -= loopFrameNb;

		// clear the paint buffer
		Q_memset(s_paintBuffer, 0, loopFrameNb * sizeof(int) * 2);

		// paint in the channels.
		channel_t *ch = s_channels;
		for (int channelIndex = 0; channelIndex < total_channels; channelIndex++, ch++)
		{
			if (!ch->sfx)
				continue;
			if (!ch->leftvol && !ch->rightvol)
				continue;
			sfxcache_t *sc = S_LoadSound(ch->sfx); // Fixme: Must not be called from a thread !
			if (!sc)
				continue;

			int channelFrameNb = loopFrameNb;
			while (channelFrameNb > 0)
			{
                int count = ch->length;
				if (count > channelFrameNb)
					count = channelFrameNb;
				if (count > 0)
				{
					channelFrameNb -= count;
					if (sc->width == 1)
						Audio_PaintChannelFrom8(ch, sc, count, s_paintBuffer);
					else
						Audio_PaintChannelFrom16(ch, sc, count, s_paintBuffer);
                    ch->pos += count;
                    ch->length -= count;
				}

				// if at end of loop, restart
				if (ch->length <= 0)
				{
					if (sc->loopstart >= 0)
					{
						ch->pos = sc->loopstart;
						ch->length = sc->length - ch->pos;
					}
					else // channel just stopped
					{
						ch->sfx = NULL;
						break;
					}
				}
			}
		}

        
        int ringBufferWriteOffset = RingBuffer_getWriteIndex(&audio->ringBuffer, 0);
		Audio_convertBuffer(loopFrameNb, s_paintBuffer, audio->buffer, audio->ringBuffer.length, ringBufferWriteOffset, volumeInt);
        RingBuffer_write(&audio->ringBuffer, loopFrameNb);
	}
}

//--------------------------------------------------------------------------------
// Update.
//--------------------------------------------------------------------------------
static void S_UpdateAmbientSounds()
{
	if (!s_ambient)
		return;

	if (!cl.worldmodel)
		return;

	mleaf_t *l = Mod_PointInLeaf(s_listener_origin, cl.worldmodel);
	if (!l || !s_ambient_level.value)
	{
		for (int ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++)
			s_channels[ambient_channel].sfx = NULL;
		return;
	}

    int master_vol_step = (int)(host_frametime * s_ambient_fade.value);
    float level = s_ambient_level.value;
	for (int ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++)
	{
		channel_t *chan = &s_channels[ambient_channel];
		chan->sfx = s_sfx_ambient[ambient_channel];

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
	if (!s_started)
		return;

    Audio *audio = g_audio;

    int frameNb = (int)(s_mixahead.value * audio->frequency);

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
	if (!s_started)
		return;

	VectorCopy(origin, s_listener_origin);
	VectorCopy(forward, s_listener_forward);
	VectorCopy(right, s_listener_right);
	VectorCopy(up, s_listener_up);

	// update general area ambient sound sources
	S_UpdateAmbientSounds();

	channel_t *combine = NULL;

	// update spatialization for static and dynamic sounds
	channel_t *ch = s_channels + NUM_AMBIENTS;
	for (int i = NUM_AMBIENTS; i < total_channels; i++, ch++)
	{
		if (!ch->sfx)
			continue;
		S_spatialize(ch); // respatialize channel
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
			combine = s_channels + MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;
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
	if (s_log.value)
	{
		int total = 0;
		ch = s_channels;
		for (int i = 0; i < total_channels; i++, ch++)
			if (ch->sfx && (ch->leftvol || ch->rightvol))
			{
				Con_Printf ("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}
		Con_Printf("----(%i)----\n", total);
	}

	// mix some sound
	S_Update_();
}

void S_ExtraUpdate()
{
	if (s_noextraupdate.value)
		return; // don't pollute timings
	S_Update_();
}

//--------------------------------------------------------------------------------
// Console functions.
//--------------------------------------------------------------------------------
static void S_Play()
{
	static int hash = 345;
	int i = 1;
	while (i < Cmd_Argc())
	{
        char name[256];
		if (!Q_strrchr(Cmd_Argv(i), '.'))
		{
			Q_strncpy(name, Cmd_Argv(i), 256);
			Q_strncat(name, ".wav", 256);
		}
		else
			Q_strncpy(name, Cmd_Argv(i), 256);
		sfx_t *sfx = S_PrecacheSound(name);
		S_StartSound(hash++, 0, sfx, s_listener_origin, 1.0, 1.0);
		i++;
	}
}

static void S_PlayVol()
{
	static int hash = 543;
	int i = 1, n = Cmd_Argc();
	while (i < n)
	{
        char name[256];
		if (!Q_strrchr(Cmd_Argv(i), '.'))
		{
			Q_strncpy(name, Cmd_Argv(i), 256);
			Q_strncat(name, ".wav", 256);
		}
		else
			Q_strncpy(name, Cmd_Argv(i), 256);
		sfx_t *sfx = S_PrecacheSound(name);
		float vol = Q_atof(Cmd_Argv(i + 1));
		S_StartSound(hash++, 0, sfx, s_listener_origin, vol, 1.0);
		i += 2;
	}
}

static void S_StopAllSoundsC()
{
	S_StopAllSounds(true);
}

static void S_SoundList()
{
	int i;
	sfx_t *sfx;
	int total = 0;
	for (sfx = s_sfx, i = 0; i < s_sfx_nb; i++, sfx++)
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

static void S_SoundInfo_f()
{
	if (!s_started || !g_audio)
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

/*
 * Not used
 
static void S_AmbientOff()
{
	s_ambient = false;
}

static void S_AmbientOn()
{
	s_ambient = true;
}
*/

//--------------------------------------------------------------------------------
// Initialization and finalization.
//--------------------------------------------------------------------------------
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

	Cvar_RegisterVariable(&s_disabled);
	Cvar_RegisterVariable(&s_volume);
	Cvar_RegisterVariable(&s_precache);
	Cvar_RegisterVariable(&s_loadas8bit);
	Cvar_RegisterVariable(&s_bgmvolume);
	Cvar_RegisterVariable(&s_ambient_level);
	Cvar_RegisterVariable(&s_ambient_fade);
	Cvar_RegisterVariable(&s_noextraupdate);
	Cvar_RegisterVariable(&s_log);
	Cvar_RegisterVariable(&s_mixahead);

	if (host_parms.memsize < 0x800000)
	{
		Cvar_Set("s_loadas8bit", "1");
		Con_Printf("loading all sounds as 8bit\n");
	}

	s_initialized = true;

    if (!Audio_initialize())
    {
        Con_Printf("S_Startup: Audio_initialize failed.\n");
        s_started = false;
        return;
    }
	s_started = true;

	s_sfx = Hunk_AllocName(MAX_SFX * sizeof(sfx_t), "sfx_t");
	s_sfx_nb = 0;

	Con_Printf("Sound sampling rate: %i\n", g_audio->frequency);

	s_sfx_ambient[AMBIENT_WATER] = S_PrecacheSound("ambience/water1.wav");
	s_sfx_ambient[AMBIENT_SKY] = S_PrecacheSound("ambience/wind2.wav");

	S_StopAllSounds(true);
}

void S_Shutdown()
{
	if (!s_started)
		return;
	s_started = false;

	Audio_finalize();
}
