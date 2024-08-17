/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_DREAMCAST

/* Dummy SDL video driver implementation; this is just enough to make an
 *  SDL-based application THINK it's got a working video driver, for
 *  applications that call SDL_Init(SDL_INIT_VIDEO) when they don't need it,
 *  and also for use as a collection of stubs when porting SDL to a new
 *  platform for which you haven't yet written a valid video driver.
 *
 * This is also a great way to determine bottlenecks: if you think that SDL
 *  is a performance problem for a given platform, enable this driver, and
 *  then see if your application runs faster without video overhead.
 *
 * Initial work by Ryan C. Gordon (icculus@icculus.org). A good portion
 *  of this was cut-and-pasted from Stephane Peter's work in the AAlib
 *  SDL video driver.  Renamed to "DUMMY" by Sam Lantinga.
 */

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_dreamcastvideo.h"
#include "SDL_dreamcastevents_c.h"
#include "SDL_dreamcastframebuffer_c.h"
#include "SDL_hints.h"
#include <kos.h>
#include <dc/video.h>
#define DREAMCASTVID_DRIVER_NAME       "dcvideo"
#define DREAMCASTVID_DRIVER_EVDEV_NAME "dcevdev"

#include "SDL_dreamcastopengl.h"

/* Initialization/Query functions */
static int DREAMCAST_VideoInit(_THIS);
static void DREAMCAST_VideoQuit(_THIS);

#ifdef SDL_INPUT_LINUXEV
static int evdev = 0;
static void DREAMCAST_EVDEV_Poll(_THIS);
#endif


/* DREAMCAST driver bootstrap functions */
static int DREAMCAST_Available(void)
{
//     const char *envr = SDL_GetHint(SDL_HINT_VIDEODRIVER);
//     if (envr) {
//         if (SDL_strcmp(envr, DREAMCASTVID_DRIVER_NAME) == 0) {
//             return 1;
//         }
// #ifdef SDL_INPUT_LINUXEV
//         if (SDL_strcmp(envr, DREAMCASTVID_DRIVER_EVDEV_NAME) == 0) {
//             evdev = 1;
//             return 1;
//         }
// #endif
//     }
    return 1;

}

static void DREAMCAST_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
}

static SDL_VideoDevice *DREAMCAST_CreateDevice(void)
{
    SDL_VideoDevice *device;
    if (!DREAMCAST_Available()) {
        SDL_SetError("Dreamcast video driver is not available.");
        return 0;
    }

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return 0;
    }


    /* Set the function pointers */
    device->VideoInit = DREAMCAST_VideoInit;
    device->VideoQuit = DREAMCAST_VideoQuit;
    device->PumpEvents = DREAMCAST_PumpEvents;
#ifdef SDL_INPUT_LINUXEV
    if (evdev) {
        device->PumpEvents = DREAMCAST_EVDEV_Poll;
    }
#endif
    device->CreateWindowFramebuffer = SDL_DREAMCAST_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = SDL_DREAMCAST_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = SDL_DREAMCAST_DestroyWindowFramebuffer;

#ifdef SDL_VIDEO_OPENGL
    device->GL_LoadLibrary = DREAMCAST_GL_LoadLibrary;
    device->GL_GetProcAddress = DREAMCAST_GL_GetProcAddress;
    device->GL_MakeCurrent = DREAMCAST_GL_MakeCurrent;
    device->GL_SwapWindow = DREAMCAST_GL_SwapBuffers;
    device->GL_CreateContext = DREAMCAST_GL_CreateContext;
    device->GL_DeleteContext = DREAMCAST_GL_DeleteContext;
#endif

    device->free = DREAMCAST_DeleteDevice;

    return device;
}

VideoBootStrap DREAMCAST_bootstrap = {
    "DREAMCASTVID_DRIVER_NAME", "SDL dreamcast video driver",
    DREAMCAST_CreateDevice,
    NULL /* no ShowMessageBox implementation */
};

#ifdef SDL_INPUT_LINUXEV
VideoBootStrap DREAMCAST_evdev_bootstrap = {
    DREAMCASTVID_DRIVER_EVDEV_NAME, "SDL dreamcast video driver with evdev",
    DREAMCAST_CreateDevice,
    NULL /* no ShowMessageBox implementation */
};
void SDL_EVDEV_Init(void);
void SDL_EVDEV_Poll();
void SDL_EVDEV_Quit(void);
static void DREAMCAST_EVDEV_Poll(_THIS)
{
    (void)_this;
    SDL_EVDEV_Poll();
}
#endif

int DREAMCAST_VideoInit(_THIS)
{
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;

    SDL_zero(current_mode);

    current_mode.w = 640;
    current_mode.h = 480;
    current_mode.refresh_rate = 60;

    /* 32 bpp for default */
    current_mode.format = SDL_PIXELFORMAT_RGB888;
    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    display.driverdata = NULL;
    SDL_AddDisplayMode(&display, &current_mode);

    SDL_AddVideoDisplay(&display, SDL_TRUE);
        /* Set video mode using KOS */
    vid_set_mode(DM_640x480_NTSC_IL, PM_RGB888);
    return 1;
}

void DREAMCAST_VideoQuit(_THIS)
{
#ifdef SDL_INPUT_LINUXEV
    SDL_EVDEV_Quit();
#endif
}

#endif /* SDL_VIDEO_DRIVER_DREAMCAST */

/* vi: set ts=4 sw=4 expandtab: */
