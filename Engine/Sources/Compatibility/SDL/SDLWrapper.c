#include "SDL/SDLWrapper.h"

#include <stdio.h>
#include <stdlib.h>

SdlwContext *sdlwContext = NULL;

//SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK

static void sdlwLogOutputFunction(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
    const char *categoryString = "";
    switch (category)
    {
    default:
        categoryString = "unknown";
        break;
    case SDL_LOG_CATEGORY_APPLICATION:
        categoryString = "application";
        break;
    case SDL_LOG_CATEGORY_ERROR:
        categoryString = "error";
        break;
    case SDL_LOG_CATEGORY_ASSERT:
        categoryString = "assert";
        break;
    case SDL_LOG_CATEGORY_SYSTEM:
        categoryString = "system";
        break;
    case SDL_LOG_CATEGORY_AUDIO:
        categoryString = "audio";
        break;
    case SDL_LOG_CATEGORY_VIDEO:
        categoryString = "video";
        break;
    case SDL_LOG_CATEGORY_RENDER:
        categoryString = "render";
        break;
    case SDL_LOG_CATEGORY_INPUT:
        categoryString = "input";
        break;
    case SDL_LOG_CATEGORY_TEST:
        categoryString = "test";
        break;
    }

    const char *priorityString = "unknown";
    switch (priority)
    {
    default:
        priorityString = "unknown";
        break;
    case SDL_LOG_PRIORITY_VERBOSE:
        priorityString = "verbose";
        break;
    case SDL_LOG_PRIORITY_DEBUG:
        priorityString = "debug";
        break;
    case SDL_LOG_PRIORITY_INFO:
        priorityString = "info";
        break;
    case SDL_LOG_PRIORITY_WARN:
        priorityString = "warn";
        break;
    case SDL_LOG_PRIORITY_ERROR:
        priorityString = "error";
        break;
    case SDL_LOG_PRIORITY_CRITICAL:
        priorityString = "critical";
        break;
    }
    
    printf("SDL - %s - %s - %s", categoryString, priorityString, message);
}

bool sdlwInitialize(SdlProcessEventFunction processEvent, Uint32 flags) {
    sdlwFinalize();
    
	SdlwContext *sdlw = malloc(sizeof(SdlwContext));
	if (sdlw == NULL) return true;
	sdlwContext = sdlw;
    sdlw->exitRequested = false;
    sdlw->defaultEventManagementEnabled = true;
	sdlw->processEvent = processEvent;
    sdlw->window = NULL;
    sdlw->windowWidth = 0;
    sdlw->windowHeight = 0;

    if (SDL_Init(flags) < 0) {
        printf("Unable to initialize SDL: %s\n", SDL_GetError());
        goto on_error;
    }
    
    SDL_LogSetOutputFunction(sdlwLogOutputFunction, NULL);
//    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Test\n");
    
//    if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);
    
    return false;
on_error:
	sdlwFinalize();
    return true;
}

void sdlwFinalize() {
	SdlwContext *sdlw = sdlwContext;
	if (sdlw == NULL) return;
    
    SDL_Quit();
    
    free(sdlw);
    sdlwContext = NULL;
}

bool sdlwCreateWindow(const char *windowName, int windowWidth, int windowHeight, Uint32 flags)
{
	SdlwContext *sdlw = sdlwContext;
	if (sdlw == NULL) return true;

    sdlwDestroyWindow();

    if (windowWidth < 0 || windowHeight < 0)
    {
        SDL_DisplayMode dm;
        if (SDL_GetDesktopDisplayMode(0, &dm) != 0)
        {
            printf("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
            goto on_error;
        }
        #if defined(__RASPBERRY_PI__) || defined(__GCW_ZERO__)
        // Windowed mode does not work on these platforms. So use full screen.
        windowWidth = dm.w;
        windowHeight = dm.h;
        #else
        windowWidth = dm.w >> 1;
        windowHeight = dm.h >> 1;
        #endif
    }
    sdlw->windowWidth = windowWidth;
    sdlw->windowHeight = windowHeight;
       
    int windowPos = SDL_WINDOWPOS_CENTERED;
   	if ((sdlw->window=SDL_CreateWindow(windowName, windowPos, windowPos, windowWidth, windowHeight, flags))==NULL) goto on_error;

    return false;
on_error:
    return true;
}

void sdlwDestroyWindow() {
	SdlwContext *sdlw = sdlwContext;
	if (sdlw == NULL) return;
    if (sdlw->window != NULL) {
        SDL_DestroyWindow(sdlw->window);
        sdlw->window=NULL;
        sdlw->windowWidth = 0;
        sdlw->windowHeight = 0;
    }
}

bool sdlwIsExitRequested()
{
	SdlwContext *sdlw = sdlwContext;
    if (sdlw == NULL) return true;
    return sdlw->exitRequested;
}

void sdlwRequestExit(bool flag)
{
	SdlwContext *sdlw = sdlwContext;
    if (sdlw == NULL) return;
    sdlw->exitRequested = flag;
}

bool sdlwResize(int w, int h) {
	SdlwContext *sdlw = sdlwContext;
	sdlw->windowWidth = w;
	sdlw->windowHeight = h;
    return false;
}

void sdlwEnableDefaultEventManagement(bool flag)
{
	SdlwContext *sdlw = sdlwContext;
    if (sdlw == NULL) return;
    sdlw->defaultEventManagementEnabled = flag;
}

static void sdlwManageEvent(SdlwContext *sdlw, SDL_Event *event) {
    switch (event->type) {
    default:
        break;

    case SDL_QUIT:
        printf("Exit requested by the system.");
        sdlwRequestExit(true);
        break;

    case SDL_WINDOWEVENT:
        switch (event->window.event) {
        case SDL_WINDOWEVENT_CLOSE:
            printf("Exit requested by the user (by closing the window).");
            sdlwRequestExit(true);
            break;
        case SDL_WINDOWEVENT_RESIZED:
            sdlwResize(event->window.data1, event->window.data2);
            break;
        }
        break;

    case SDL_KEYDOWN:
        switch (event->key.keysym.sym) {
        default:
            break;
        case 27:
            printf("Exit requested by the user (with a key).");
            sdlwRequestExit(true);
            break;
        }
        break;
    }
}

void sdlwCheckEvents() {
	SdlwContext *sdlw = sdlwContext;
    if (sdlw == NULL) return;
    
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
        bool eventManaged = false;
		SdlProcessEventFunction processEvent = sdlw->processEvent;
		if (processEvent != NULL)
			eventManaged = processEvent(&event);
        if (!eventManaged && sdlw->defaultEventManagementEnabled)
            sdlwManageEvent(sdlw, &event);
	}
}
