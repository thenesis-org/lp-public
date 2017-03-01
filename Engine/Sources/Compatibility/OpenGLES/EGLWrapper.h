#ifndef EGLWrapper_h
#define EGLWrapper_h

//#define EGLW_SDL_DISPLAY
//#define EGLW_GLES1
//#define EGLW_GLES2

#if !(defined(EGLW_GLES1) ^ defined(EGLW_GLES2))
#error "Either EGLW_GLES1 or EGLW_GLES2 must be defined"
#endif

#if defined(__RASPBERRY_PI__)
//#include "bcm_host.h"
#endif

#include <EGL/egl.h>
#if defined(EGLW_SDL_DISPLAY)
#include <EGL/eglThenesis.h>
#endif

#include <stdbool.h>

typedef struct {
    EGLint redSize, greenSize, blueSize, alphaSize, depthSize, stencilSize, samples;
} EglwConfigInfo;

void eglwClearConfigInfo(EglwConfigInfo *ci);

typedef struct {
    EGLDisplay display;
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;
    EglwConfigInfo configInfoAbilities;
    EglwConfigInfo configInfo;
} EglwContext;

extern EglwContext *eglwContext;

/// If \a minimalCfgi is not NULL, there must be a config that has at least these minimum features.
/// If \a requestedCfgi is not NULL, try to find a config that match at least this one.
/// If \a requestedCfgi is NULL, the config with the best quality is used if \a maxQualityFlag is true, otherwise the minimum quality is used.
bool eglwInitialize(EglwConfigInfo *minimalCfgi, EglwConfigInfo *requestedCfgi, bool maxQualityFlag);
void eglwFinalize();
void eglwSwapBuffers();

#endif
