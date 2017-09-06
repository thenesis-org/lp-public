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
// portable code to mix sounds for snd_dma.c

#include "Common/sys.h"
#include "Sound/sound.h"

#define PAINTBUFFER_SIZE 512

static int l_paintBuffer[PAINTBUFFER_SIZE * 2];

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

void S_PaintChannels(int frameNb)
{
    Audio *audio = g_audio;
    int volumeInt = (int)(volume.value * 256);
	while (frameNb > 0)
	{
		int loopFrameNb = frameNb;
		if (loopFrameNb > PAINTBUFFER_SIZE)
			loopFrameNb = PAINTBUFFER_SIZE;
        frameNb -= loopFrameNb;

		// clear the paint buffer
		Q_memset(l_paintBuffer, 0, loopFrameNb * sizeof(int) * 2);

		// paint in the channels.
		channel_t *ch = channels;
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
						Audio_PaintChannelFrom8(ch, sc, count, l_paintBuffer);
					else
						Audio_PaintChannelFrom16(ch, sc, count, l_paintBuffer);
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
		Audio_convertBuffer(loopFrameNb, l_paintBuffer, audio->buffer, audio->ringBuffer.length, ringBufferWriteOffset, volumeInt);
        RingBuffer_write(&audio->ringBuffer, loopFrameNb);
	}
}
