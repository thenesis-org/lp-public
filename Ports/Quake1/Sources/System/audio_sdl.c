#include "Client/console.h"
#include "Common/common.h"
#include "Sound/sound.h"

#include <SDL2/SDL.h>

#include <stdio.h>
#include <string.h>

#define FREQUENCY_DEFAULT (11025)
#define BUFFER_SIZE (2048)

static SDL_AudioDeviceID l_audioDevice = 0;
static int underflows = 0;

static void Audio_paint(void *userData, Uint8 *stream, int len)
{
    Audio *audio = (Audio*)userData;
	if (!audio)
        return;

    int frameToRead = len >> 2;
    while (frameToRead > 0)
    {
        int loopFrameToRead = RingBuffer_getLengthAvailableForReading(&audio->ringBuffer);
        if (loopFrameToRead > frameToRead)
            loopFrameToRead = frameToRead;

        int loopSizeToCopy;
        if (loopFrameToRead <= 0)
        {
            // Underflow. Fill the remaining with silence.
            underflows++;
            loopFrameToRead = frameToRead;
            loopSizeToCopy = loopFrameToRead  << 2;
            memset(stream, 0, loopSizeToCopy);
        }
        else
        {
            int ringBufferReadOffset = RingBuffer_getReadIndex(&audio->ringBuffer, 0);
            int frameRemaining = audio->ringBuffer.length - ringBufferReadOffset;
            if (loopFrameToRead > frameRemaining)
                loopFrameToRead = frameRemaining;
            loopSizeToCopy = loopFrameToRead << 2;
            memcpy(stream, audio->buffer + (ringBufferReadOffset << 1), loopSizeToCopy);
            RingBuffer_read(&audio->ringBuffer, loopFrameToRead);
        }
        stream += loopSizeToCopy;
        frameToRead -= loopFrameToRead;
    }
}

qboolean Audio_initialize()
{
    Audio_finalize();

    Audio *audio = (Audio*)malloc(sizeof(Audio));
    if (audio == NULL)
        goto on_error;
    memset(audio, 0, sizeof(Audio));
    g_audio = audio;

    {
        SDL_AudioSpec desired, obtained;
        SDL_memset(&desired, 0, sizeof(desired));
        desired.freq = FREQUENCY_DEFAULT;
        desired.format = AUDIO_S16SYS;
        desired.channels = 2;
        desired.samples = 512;
        desired.callback = Audio_paint;
        desired.userdata = audio;
        SDL_AudioDeviceID audioDevice = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
        if (audioDevice <= 0)
        {
            Con_Printf("Couldn't open SDL audio: %s\n", SDL_GetError());
            goto on_error;
        }
        l_audioDevice = audioDevice;

        audio->channels = obtained.channels;
        audio->samplebits = (obtained.format & 0xFF);
        audio->frequency = obtained.freq;
        if ((audio->buffer = malloc(BUFFER_SIZE << 2)) == NULL)
            goto on_error;
        RingBuffer_initialize(&audio->ringBuffer, BUFFER_SIZE);

        SDL_PauseAudioDevice(audioDevice, 0);
    }
    
	return true;
on_error:
    Audio_finalize();
    return true;
}

void Audio_finalize()
{
    SDL_AudioDeviceID audioDevice = l_audioDevice;
    if (audioDevice)
    {
		SDL_CloseAudioDevice(audioDevice);
        l_audioDevice = 0;
    }
    
    Audio *audio = g_audio;
	if (audio)
	{
        g_audio = NULL;
        free(audio->buffer);
        free(audio);
	}
}
