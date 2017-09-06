#include "OpenGLES/EGLWrapper.h"
#include "SDL/SDLWrapper.h"

#include <SDL2/SDL_syswm.h>

#if defined(__RASPBERRY_PI__)
#include "bcm_host.h"
#endif

#include <stdio.h>
#include <stdlib.h>

EglwContext *eglwContext = NULL;

void eglwClearConfigInfo(EglwConfigInfo *ci)
{
    ci->redSize = 0;
    ci->greenSize = 0;
    ci->blueSize = 0;
    ci->alphaSize = 0;
    ci->depthSize = 0;
    ci->stencilSize = 0;
    ci->samples = 0;
}

static bool eglwIsConfigInfoBetter(const EglwConfigInfo *ciBest, const EglwConfigInfo *ci)
{
    bool better = true;
    if (ciBest->redSize > ci->redSize) better =false;
    if (ciBest->greenSize > ci->greenSize) better =false;
    if (ciBest->blueSize > ci->blueSize) better =false;
    if (ciBest->alphaSize > ci->alphaSize) better =false;
    if (ciBest->depthSize > ci->depthSize) better =false;
    if (ciBest->stencilSize > ci->stencilSize) better =false;
    if (ciBest->samples > ci->samples) better =false;
    return better;
}

static void eglwUpdateConfigInfoWithBestValues(EglwConfigInfo *ciBest, const EglwConfigInfo *ci)
{
    if (ciBest->redSize < ci->redSize) ciBest->redSize = ci->redSize;
    if (ciBest->greenSize < ci->greenSize) ciBest->greenSize = ci->greenSize;
    if (ciBest->blueSize < ci->blueSize) ciBest->blueSize = ci->blueSize;
    if (ciBest->alphaSize < ci->alphaSize) ciBest->alphaSize = ci->alphaSize;
    if (ciBest->depthSize < ci->depthSize) ciBest->depthSize = ci->depthSize;
    if (ciBest->stencilSize < ci->stencilSize) ciBest->stencilSize = ci->stencilSize;
    if (ciBest->samples < ci->samples) ciBest->samples = ci->samples;
}

static void eglwGetConfigInfo(EGLDisplay display, EGLConfig config, EglwConfigInfo *ci)
{
    eglGetConfigAttrib(display, config, EGL_RED_SIZE, &ci->redSize);
    eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &ci->greenSize);
    eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &ci->blueSize);
    eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &ci->alphaSize);
    eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &ci->depthSize);
    eglGetConfigAttrib(display, config, EGL_STENCIL_SIZE, &ci->stencilSize);
    eglGetConfigAttrib(display, config, EGL_SAMPLES, &ci->samples);
}

static const char* eglwGetConfigInfoString(const EglwConfigInfo *ci)
{
    static char buffer[256];
    snprintf(buffer, 256, "R=%i G=%i B=%i A=%i Depth=%i Stencil=%i Samples=%i", ci->redSize, ci->greenSize, ci->blueSize, ci->alphaSize, ci->depthSize, ci->stencilSize, ci->samples);
    buffer[255]=0;
    return buffer;
}

static bool eglwCheckAllConfigs(EglwContext *eglw, bool printConfigFlag)
{
    EGLint configNb;
    EGLConfig *configs = NULL;

    if (!eglGetConfigs(eglw->display, NULL, 0, &configNb) || configNb<=0) {
        printf("Cannot get configs.\n");
        goto on_error;
    }

    configs = malloc(sizeof(EGLConfig) * configNb);
    if (!eglGetConfigs(eglw->display, configs, configNb, &configNb) || configNb<=0) {
        printf("Cannot get configs.\n");
        goto on_error;
    }

    printf("All available configs:\n");
    eglwClearConfigInfo(&eglw->configInfoAbilities);
    for (int ci = 0; ci < configNb; ci++)
    {
        EGLConfig config = configs[ci];
        EglwConfigInfo cfgi;
        eglwGetConfigInfo(eglw->display, config, &cfgi);
        if (printConfigFlag)
        {
            printf("-Available config %i: %s\n", ci, eglwGetConfigInfoString(&cfgi));
        }
        eglwUpdateConfigInfoWithBestValues(&eglw->configInfoAbilities, &cfgi);
    }

    free(configs);
    return false;
on_error:
    free(configs);
    return true;
}

static bool eglwFindConfig(EglwContext *eglw, const EglwConfigInfo *minimalCfgi, const EglwConfigInfo *requestedCfgi, bool maxQualityFlag, bool printConfigFlag) {
    EGLint configNb;
    EGLConfig *configs = NULL;

    EglwConfigInfo defaultConfig;
    eglwClearConfigInfo(&defaultConfig);
    if (minimalCfgi == NULL)
        minimalCfgi = &defaultConfig;
    if (requestedCfgi == NULL)
        requestedCfgi = minimalCfgi;

    if (printConfigFlag)
    {
        printf("Minimal config: %s\n", eglwGetConfigInfoString(minimalCfgi));
        printf("Requested config: %s\n", eglwGetConfigInfoString(requestedCfgi));
    }

    EGLint frameBufferAttributes[10*2+1];
    {
        EGLint *fba = frameBufferAttributes;
        #if defined(EGLW_GLES2)
        fba[0] = EGL_RENDERABLE_TYPE; fba[1] = EGL_OPENGL_ES2_BIT; fba += 2;
        #else
        fba[0] = EGL_RENDERABLE_TYPE; fba[1] = EGL_OPENGL_ES_BIT; fba += 2;
        #endif
        fba[0] = EGL_SURFACE_TYPE; fba[1] = EGL_WINDOW_BIT; fba += 2;
        fba[0] = EGL_RED_SIZE; fba[1] = minimalCfgi->redSize; fba += 2;
        fba[0] = EGL_GREEN_SIZE; fba[1] = minimalCfgi->greenSize; fba += 2;
        fba[0] = EGL_BLUE_SIZE; fba[1] = minimalCfgi->blueSize; fba += 2;
        fba[0] = EGL_ALPHA_SIZE; fba[1] = minimalCfgi->alphaSize; fba += 2;
        fba[0] = EGL_DEPTH_SIZE; fba[1] = minimalCfgi->depthSize; fba += 2;
        fba[0] = EGL_STENCIL_SIZE; fba[1] = minimalCfgi->stencilSize; fba += 2;
        if (minimalCfgi->samples > 0) {
            fba[0] = EGL_SAMPLE_BUFFERS; fba[1] = 1; fba += 2;
            fba[0] = EGL_SAMPLES; fba[1] = minimalCfgi->samples; fba += 2;
        }
        fba[0] = EGL_NONE;
    }

    // Get an appropriate EGL frame buffer configuration.
    if (!eglChooseConfig(eglw->display, frameBufferAttributes, NULL, 0, &configNb) || configNb<=0) {
        printf("Cannot find an EGL config.\n");
        goto on_error;
    }

    configs = malloc(sizeof(EGLConfig) * configNb);
    if (!eglChooseConfig(eglw->display, frameBufferAttributes, configs, configNb, &configNb) || configNb<=0) {
        printf("Cannot find an EGL config.\n");
        goto on_error;
    }

    // Get the effective attributes.
    {
        EglwConfigInfo cfgiBest;
        eglwClearConfigInfo(&cfgiBest);
        int configBestIndex = 0; // Currently we take the first config. Later we could implement a smarter choice.
        bool greaterOrEqualToRequestedFound = false;
        for (int ci = 0; ci < configNb; ci++)
        {
            EGLConfig config = configs[ci];
            EglwConfigInfo cfgi;
            eglwGetConfigInfo(eglw->display, config, &cfgi);
            if (printConfigFlag)
            {
                printf("-Available config %i: %s\n", ci, eglwGetConfigInfoString(&cfgi));
            }
            if (maxQualityFlag)
            {
                if (eglwIsConfigInfoBetter(&cfgiBest, &cfgi))
                {
                    cfgiBest = cfgi;
                    configBestIndex = ci;
                }
            }
            else
            {
                if (!greaterOrEqualToRequestedFound)
                {
                    if (eglwIsConfigInfoBetter(requestedCfgi, &cfgi))
                    {
                        cfgiBest = cfgi;
                        configBestIndex = ci;
                        greaterOrEqualToRequestedFound = true;
                    }
                    else if (eglwIsConfigInfoBetter(&cfgiBest, &cfgi))
                    {
                        cfgiBest = cfgi;
                        configBestIndex = ci;
                    }
                }
            }
        }

        eglw->config = configs[configBestIndex];
        eglw->configInfo = cfgiBest;
        printf("Using config %i: %s\n", configBestIndex, eglwGetConfigInfoString(&cfgiBest));
    }

    free(configs);
    return false;
on_error:
    free(configs);
    return true;
}

#if defined(__RASPBERRY_PI__)
static DISPMANX_DISPLAY_HANDLE_T dispman_display;
static DISPMANX_ELEMENT_HANDLE_T dispman_element;
static EGL_DISPMANX_WINDOW_T l_dispmanWindow;
#endif

static EGLNativeWindowType eglwGetNativeWindow()
{
    EGLNativeWindowType nativeWindow = NULL;

    #if defined(EGLW_SDL_DISPLAY)

    nativeWindow=(EGLNativeWindowType)sdlwContext->window;

    #elif defined(__RASPBERRY_PI__)

   	dispman_display = vc_dispmanx_display_open(0 /* LCD */);

	int windowWidth, windowHeight;
    #if 0
    windowWidth = sdlwContext->windowWidth;
    windowHeight = sdlwContext->windowHeight;
    #else
	SDL_GetWindowSize(sdlwContext->window, &windowWidth, &windowHeight);
    #endif

    #if 0

	int windowX, windowY;
	SDL_GetWindowPosition(sdlwContext->window, &windowX, &windowY);

    VC_RECT_T src_rect;
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = windowWidth << 16;
    src_rect.height = windowHeight << 16;

    VC_RECT_T dst_rect;
    dst_rect.x = (uint32_t)windowX;
    dst_rect.y = (uint32_t)windowY;
    dst_rect.width = (uint32_t)windowWidth;
    dst_rect.height = (uint32_t)windowHeight;

    #else

	uint32_t screenWidth, screenHeight;
 	graphics_get_display_size(0 /* LCD */, &screenWidth, &screenHeight);
//    uint32_t windowX = (screenWidth - windowWidth) >> 1;
//    uint32_t windowY = (screenHeight - windowHeight) >> 1;

    VC_RECT_T src_rect;
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = windowWidth << 16;
    src_rect.height = windowHeight << 16;

    VC_RECT_T dst_rect;
    dst_rect.x = (uint32_t)0;
    dst_rect.y = (uint32_t)0;
    dst_rect.width = (uint32_t)screenWidth;
    dst_rect.height = (uint32_t)screenHeight;

    #endif

    // Disable alpha, otherwise the app looks composed with whatever dispman is showing (X11, console,etc).
    VC_DISPMANX_ALPHA_T dispman_alpha;
    dispman_alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
    dispman_alpha.opacity = 0xFF;
    dispman_alpha.mask = 0;

    DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start(0);

    EGL_DISPMANX_WINDOW_T *dispmanWindow = &l_dispmanWindow;
	dispman_element = vc_dispmanx_element_add (
        dispman_update,
        dispman_display,
        0 /* layer */,
        &dst_rect,
        0 /*src*/,
        &src_rect,
        DISPMANX_PROTECTION_NONE,
        &dispman_alpha /*alpha*/,
        0 /*clamp*/,
        0 /*transform*/);
    dispmanWindow->element = dispman_element;
    dispmanWindow->width = windowWidth;
    dispmanWindow->height = windowHeight;

    vc_dispmanx_update_submit_sync(dispman_update);

    nativeWindow=(EGLNativeWindowType)dispmanWindow;

    #else

    struct SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(sdlwContext->window, &wmInfo)) {
        printf("Cannot get the window handle.\n");
        return (EGLNativeWindowType)NULL;
    }

    #if defined(_WIN32)
    nativeWindow=wmInfo.info.win.window;
    #elif defined(__unix__) && defined(SDL_VIDEO_DRIVER_X11)
    nativeWindow=wmInfo.info.x11.window;
    #endif

    #endif

    return nativeWindow;
}

bool eglwInitialize(EglwConfigInfo *minimalCfgi, EglwConfigInfo *requestedCfgi, bool maxQualityFlag) {
    eglwFinalize();

	EglwContext *eglw = malloc(sizeof(EglwContext));
	if (eglw == NULL) return true;
	eglwContext = eglw;
	eglw->display = EGL_NO_DISPLAY;
	eglw->config = NULL;
	eglw->surface = EGL_NO_SURFACE;
	eglw->context = EGL_NO_CONTEXT;

	#if defined(__RASPBERRY_PI__)
	bcm_host_init();
	#endif

    // Get an EGL display connection.
    #if defined(EGLW_SDL_DISPLAY)
    eglw->display = eglGetDisplaySDL();
    #else
	EGLNativeDisplayType nativeDisplay = EGL_DEFAULT_DISPLAY;
	#if defined(__unix__) && !defined(__RASPBERRY_PI__) && defined(SDL_VIDEO_DRIVER_X11)
	{
        struct SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (!SDL_GetWindowWMInfo(sdlwContext->window, &wmInfo)) {
            printf("Cannot get the window handle.\n");
            goto on_error;
        }
        nativeDisplay = wmInfo.info.x11.display;
        // nativeDisplay = XOpenDisplay(NULL);
	}
	#endif
    eglw->display = eglGetDisplay(nativeDisplay);
    #endif
    if (eglw->display==EGL_NO_DISPLAY) {
        printf("Cannot get the default EGL display.\n");
        goto on_error;
    }

    // Initialize the EGL display connection.
	EGLint majorVersion, minorVersion;
    if (!eglInitialize(eglw->display, &majorVersion, &minorVersion)) {
        printf("Cannot initialize EGL.\n");
        goto on_error;
    }

    if (eglwCheckAllConfigs(eglw, true)) goto on_error;
    if (eglwFindConfig(eglw, minimalCfgi, requestedCfgi, maxQualityFlag, true)) goto on_error;

    // Create an EGL rendering context.
    {
        EGLint contextAttributes[1*2+1];
        {
            EGLint *ca = contextAttributes;
            #if defined(EGLW_GLES2)
            ca[0] = EGL_CONTEXT_CLIENT_VERSION; ca[1] = 2; ca += 2;
            #else
            ca[0] = EGL_CONTEXT_CLIENT_VERSION; ca[1] = 1; ca += 2;
            #endif
            ca[0] = EGL_NONE;
        }
        eglw->context = eglCreateContext(eglw->display, eglw->config, EGL_NO_CONTEXT, contextAttributes);
        if (eglw->context==NULL) {
            printf("Cannot create an EGL context.\n");
            goto on_error;
        }
    }

    // Create an EGL window surface.
    {
        static const EGLint SURFACE_ATTRIBUTES[] = { EGL_NONE };
		EGLNativeWindowType nativeWindow = eglwGetNativeWindow();
		eglw->surface = eglCreateWindowSurface(eglw->display, eglw->config, nativeWindow, SURFACE_ATTRIBUTES);
		if (eglw->surface==NULL) {
			printf("Cannot create a window surface.\n");
			goto on_error;
		}
	}

    // Connect the context to the surface.
    if (!eglMakeCurrent(eglw->display, eglw->surface, eglw->surface, eglw->context)) {
        printf("Cannot make current the EGL context.\n");
        goto on_error;
    }

    return false;
on_error:
	printf("EGL error: %04x\n", eglGetError());
	eglwFinalize();
    return true;
}

void eglwFinalize() {
	EglwContext *eglw = eglwContext;
	if (eglw != NULL)
	{
		eglMakeCurrent(eglw->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroySurface(eglw->display, eglw->surface);

		#if defined(__RASPBERRY_PI__)
		{
		   DISPMANX_UPDATE_HANDLE_T dispman_update;
		   dispman_update = vc_dispmanx_update_start(0);
		   int s = vc_dispmanx_element_remove(dispman_update, dispman_element);
		   vc_dispmanx_update_submit_sync(dispman_update);
		   s = vc_dispmanx_display_close(dispman_display);
		}
		#endif

		eglDestroyContext(eglw->display, eglw->context);
		eglTerminate(eglw->display);
		free(eglw);
		eglwContext = NULL;
	}
}

void eglwSwapBuffers() {
	EglwContext *eglw = eglwContext;
	if (!eglSwapBuffers(eglw->display, eglw->surface)) {
		printf("Cannot swap buffers.\n");
	}
}
