/*
 * Copyright (C) 2010 Yamagi Burmeister
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * ----------------------------------------------------------------------
 *
 * Renderer_Gamma_CalculateRamp() is derived from SDL2's SDL_CalculateGammaRamp()
 * (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>
 * Published under zlib License: http://www.libsdl.org/license.php
 *
 * =======================================================================
 *
 * This file implements an OpenGL context and window handling through
 * SDL.
 *
 * =======================================================================
 */

#include "client/refresh/local.h"

#include "SDL/SDLWrapper.h"
#include "OpenGLES/EGLWrapper.h"
#include "OpenGLES/OpenGLWrapper.h"

#include <SDL2/SDL.h>

qboolean graphics_has_stencil = false;
qboolean graphics_support_msaa = false;

qboolean Renderer_initializeWindow();
static void Renderer_finalizeWindow();
static qboolean Renderer_IsFullscreen();
static void Renderer_SetFullscreen(bool flag);
static void Renderer_SetIcon();
static void Renderer_Gamma_Init();
static void Renderer_Gamma_CalculateRamp(float gamma, Uint16 * ramp, int len);

//********************************************************************************
// OpenGL ES.
//********************************************************************************
void Gles_checkEglError();
void Gles_checkGlesError();

void Gles_checkEglError()
{
	EGLint error = eglGetError();
	if (error && error != EGL_SUCCESS)
		VID_Printf(PRINT_ALL, "EGL Error: 0x%04x\n", (int)error);
}

void Gles_checkGlesError()
{
	GLenum error = glGetError();
	if (error != GL_NO_ERROR)
		VID_Printf(PRINT_ALL, "GLES Error: 0x%04x\n", (int)error);
}

//********************************************************************************
// Initialization.
//********************************************************************************
qboolean Renderer_Init()
{
	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if (SDL_Init(SDL_INIT_VIDEO) == -1)
			return false;

		const char * driverName = SDL_GetCurrentVideoDriver();
		VID_Printf(PRINT_ALL, "SDL video driver is \"%s\".\n", driverName);
	}
	return true;
}

void Renderer_Shutdown()
{
	Renderer_finalizeWindow();
	if (SDL_WasInit(SDL_INIT_VIDEO))
	{
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
}

//********************************************************************************
// Mode.
//********************************************************************************
qboolean Renderer_initializeWindow()
{
	SdlwContext *sdlw = sdlwContext;

	qboolean fullscreen = vid_fullscreen->value;
	vid_fullscreen->modified = false;

    int mode = gl_mode->value;
	gl_mode->modified = false;

    if (mode < 0)
    {
        vid.width = gl_customwidth->value;
        vid.height = gl_customheight->value;
    }
    else
    {
        while (!Renderer_GetModeInfo(mode, &vid.width, &vid.height))
        {
            VID_Printf(PRINT_ALL, "Mode %i is invalid.\n", mode);
            if (mode != gl_state.prev_mode)
            {
                VID_Printf(PRINT_ALL, "Trying with previous mode (%i).\n", gl_state.prev_mode);
                mode = gl_state.prev_mode;
            }
            else if (mode != GL_MODE_DEFAULT)
            {
                VID_Printf(PRINT_ALL, "Trying with default mode (%i).\n", GL_MODE_DEFAULT);
                mode = GL_MODE_DEFAULT;
                gl_state.prev_mode = GL_MODE_DEFAULT;
            }
            else
            {
                VID_Printf(PRINT_ALL, "Default mode (%i) is invalid. This is an internal error.\n", GL_MODE_DEFAULT);
                goto on_error;
            }
            Cvar_SetValue("gl_mode", mode);
            gl_mode->modified = false;
        }
    }

	{
		char windowName[64];
		snprintf(windowName, sizeof(windowName), QUAKE2_COMPLETE_NAME);
		windowName[63] = 0;
        while (1)
        {
            int flags = 0; // SDL_WINDOW_RESIZABLE
            if (fullscreen)
                flags |= SDL_WINDOW_FULLSCREEN;
            bool failed;
            if (sdlw->window != NULL)
            {
                if (fullscreen != Renderer_IsFullscreen())
                    Renderer_SetFullscreen(fullscreen);
                failed = fullscreen != Renderer_IsFullscreen();

                int width, height;
                SDL_GetWindowSize(sdlw->window, &width, &height);
                if (width != vid.width && height != vid.height)
                {
                    SDL_SetWindowSize(sdlw->window, vid.width, vid.height);
                }
            }
            else
            {
                failed = sdlwCreateWindow(windowName, vid.width, vid.height, flags);
            }
            
            if (!failed)
                break;
                
            VID_Printf(PRINT_ALL, "Failed to create a window.\n");
            if (fullscreen)
            {
                VID_Printf(PRINT_ALL, "Trying without fullscreen.\n");
                fullscreen = false;
                Cvar_SetValue("vid_fullscreen", 0);
                vid_fullscreen->modified = false;
            }
            else if (mode != gl_state.prev_mode)
            {
                VID_Printf(PRINT_ALL, "Trying with previous mode.\n");
                mode = gl_state.prev_mode;
                Cvar_SetValue("gl_mode", mode);
                gl_mode->modified = false;
            }
            else if (mode != gl_state.prev_mode)
            {
                VID_Printf(PRINT_ALL, "Trying with default mode.\n");
                mode = GL_MODE_DEFAULT;
                gl_state.prev_mode = GL_MODE_DEFAULT;
                Cvar_SetValue("gl_mode", mode);
                gl_mode->modified = false;
            }
            else
            {
                VID_Printf(PRINT_ALL, "All attempts failed\n");
                goto on_error;
            }
        }
        
        if (gl_mode->value == -1)
            gl_state.prev_mode = GL_MODE_DEFAULT; // Safe default for custom mode.
        else
            gl_state.prev_mode = gl_mode->value;

        VID_NewWindow(vid.width, vid.height);
	}

	while (1)
	{
		EglwConfigInfo cfgiMinimal;
		cfgiMinimal.redSize = 5; cfgiMinimal.greenSize = 5; cfgiMinimal.blueSize = 5; cfgiMinimal.alphaSize = 0;
		cfgiMinimal.depthSize = 16; cfgiMinimal.stencilSize = 0; cfgiMinimal.samples = 0;
		EglwConfigInfo cfgiRequested;
		cfgiRequested.redSize = 5; cfgiRequested.greenSize = 5; cfgiRequested.blueSize = 5; cfgiRequested.alphaSize = 0;
		cfgiRequested.depthSize = 16; cfgiRequested.stencilSize = 1; cfgiRequested.samples = (int)gl_msaa_samples->value;

		if (eglwInitialize(&cfgiMinimal, &cfgiRequested, false))
		{
            VID_Printf(PRINT_ALL, "Cannot create an OpenGL ES context.\n");
			if (gl_msaa_samples->value)
			{
				VID_Printf(PRINT_ALL, "Trying without MSAA.\n");
				Cvar_SetValue("gl_msaa_samples", 0);
			}
			else
			{
				VID_Printf(PRINT_ALL, "All attempts failed.\n");
				goto on_error;
			}
		}
		else
		{
			break;
		}
	}

	graphics_support_msaa = (eglwContext->configInfoAbilities.samples > 0);
	Cvar_SetValue("gl_msaa_samples", eglwContext->configInfo.samples);
	graphics_has_stencil = (eglwContext->configInfo.stencilSize > 0);

	eglSwapInterval(eglwContext->display, gl_swapinterval->value ? 1 : 0);

	if (oglwCreate())
		goto on_error;

	Renderer_SetIcon();
	Renderer_Gamma_Init();

	SDL_ShowCursor(0);

	return true;

on_error:
	Renderer_finalizeWindow();
	return false;
}

static void Renderer_finalizeWindow()
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

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClearDepthf(1.0f);
		glClearStencil(0);
		oglwClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		Renderer_EndFrame();

		oglwDestroy();
	}

	eglwFinalize();
	sdlwDestroyWindow();

	gl_state.hwgamma = false;
}

static qboolean Renderer_IsFullscreen()
{
	return (SDL_GetWindowFlags(sdlwContext->window) & SDL_WINDOW_FULLSCREEN) != 0;
}

static void Renderer_SetFullscreen(bool flag)
{
	SDL_SetWindowFullscreen(sdlwContext->window, flag ? SDL_WINDOW_FULLSCREEN : 0);
	Cvar_SetValue("vid_fullscreen", flag);
}

typedef struct vidmode_s
{
	int width, height;
} vidmode_t;

/* This must be the same as in videomenu.c! */
static const vidmode_t vid_modes[] =
{
	{ 320, 240 }, // 4:3
	#if !defined(__GCW_ZERO__)
	{ 640, 360 }, // 16:9
	{ 640, 480 }, // 4:3
	{ 800, 600 }, // 4:3
	{ 960, 540 }, // 16:9
	{ 960, 600 }, // 16:10
	{ 1024, 768 }, // 4:3
	{ 1280, 720 }, // 16:9
	{ 1366, 768 }, // 16:9
	{ 1600, 1200 }, // 4:3
	{ 1680, 1050 }, // 16:9
	{ 1920, 1080 }, // 16:9
	{ 1920, 1200 }, // 16:10
	{ 2048, 1536 }, // 4:3
	#endif
};

#define VID_NUM_MODES (int)(sizeof(vid_modes) / sizeof(vid_modes[0]))

int Renderer_GetModeNb()
{
    return VID_NUM_MODES;
}

qboolean Renderer_GetModeInfo(int mode, int *width, int *height)
{
	if (mode < 0 || mode >= VID_NUM_MODES)
		return false;
	*width = vid_modes[mode].width;
	*height = vid_modes[mode].height;
	return true;
}

//********************************************************************************
// Frame.
//********************************************************************************
void Renderer_EndFrame()
{
	if (gl_discardframebuffer->value && gl_config.discardFramebuffer)
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
// Icon.
//********************************************************************************
/*
 * Sets the window icon
 */

/* The 64x64 32bit window icon */
#include "icon/q2icon64.h"

static void Renderer_SetIcon()
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

//********************************************************************************
// Gamma.
//********************************************************************************
#if !defined(__GCW_ZERO__)
#define HARDWARE_GAMMA_ENABLED
#endif

static void Renderer_Gamma_Init()
{
	#if defined(HARDWARE_GAMMA_ENABLED)
	VID_Printf(PRINT_ALL, "Using hardware gamma via SDL.\n");
	#endif
	gl_state.hwgamma = true;
	vid_gamma->modified = true;
}

#if defined(HARDWARE_GAMMA_ENABLED)

/*
 *  from SDL2 SDL_CalculateGammaRamp, adjusted for arbitrary ramp sizes
 *  because xrandr seems to support ramp sizes != 256 (in theory at least)
 */
static void Renderer_Gamma_CalculateRamp(float gamma, Uint16 * ramp, int len)
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

/*
 * Sets the hardware gamma
 */
void Renderer_Gamma_Update()
{
	#if defined(HARDWARE_GAMMA_ENABLED)
	float gamma = (vid_gamma->value);

	Uint16 ramp[256];
	Renderer_Gamma_CalculateRamp(gamma, ramp, 256);
	if (SDL_SetWindowGammaRamp(sdlwContext->window, ramp, ramp, ramp) != 0)
	{
		VID_Printf(PRINT_ALL, "Setting gamma failed: %s\n", SDL_GetError());
	}
	#endif
}
