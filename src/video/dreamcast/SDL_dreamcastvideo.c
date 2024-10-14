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
//Custom code for 60Hz
static int sdl_dc_no_ask_60hz=0;
static int sdl_dc_default_60hz=0;
#include "60hz.h"

void SDL_DC_ShowAskHz(SDL_bool value)
{
	sdl_dc_no_ask_60hz=!value;
}

void SDL_DC_Default60Hz(SDL_bool value)
{
	sdl_dc_default_60hz=value;
}//Custom code for 60Hz
static int sdl_dc_no_ask_60hz=0;
static int sdl_dc_default_60hz=0;
/* Initialization/Query functions */
static int DREAMCAST_VideoInit(_THIS);
static void DREAMCAST_VideoQuit(_THIS);
void DREAMCAST_GetDisplayModes(_THIS, SDL_VideoDisplay *display);
int DREAMCAST_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
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
int dreamcast_text_input_enabled=SDL_FALSE;
void DREAMCAST_StartTextInput(SDL_VideoDevice *device)
{
    if (!dreamcast_text_input_enabled)
    {
        dreamcast_text_input_enabled = SDL_TRUE;
        // printf("DREAMCAST_StartTextInput: Text input mode enabled.\n");
    }
}

void DREAMCAST_StopTextInput(SDL_VideoDevice *device)
{
    if (dreamcast_text_input_enabled)
    {
        dreamcast_text_input_enabled = SDL_FALSE;
        // printf("DREAMCAST_StopTextInput: Text input mode disabled.\n");
    }
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
    device->GetDisplayModes = DREAMCAST_GetDisplayModes;
    device->SetDisplayMode = DREAMCAST_SetDisplayMode;
    // device->HasScreenKeyboardSupport = DREAMCAST_HasScreenKeyboardSupport;
    device->StartTextInput = DREAMCAST_StartTextInput;
    device->StopTextInput = DREAMCAST_StopTextInput;    
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
    device->quirk_flags = VIDEO_DEVICE_QUIRK_FULLSCREEN_ONLY;
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
int __sdl_dc_is_60hz=0;
int DREAMCAST_VideoInit(_THIS) {
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;
    int disp_mode;
    int pixel_mode;
    int width = 640, height = 480; // Default to 640x480, modify based on requirements
    int bpp = 16;  // Bits per pixel
    Uint32 Rmask, Gmask, Bmask;

    SDL_zero(current_mode);

    // Determine if we are in 60Hz or 50Hz mode based on cable type and region
    if (!vid_check_cable()) {
        __sdl_dc_is_60hz = 1; // 60Hz for VGA
        if (width == 320 && height == 240) disp_mode = DM_320x240_VGA;
        else if (width == 640 && height == 480) disp_mode = DM_640x480_VGA;
        else if (width == 768 && height == 480) disp_mode = DM_768x480_PAL_IL;
        else {
            SDL_SetError("Couldn't find requested mode in list");
            return -1;
        }
    } else if (flashrom_get_region() != FLASHROM_REGION_US && !sdl_dc_ask_60hz()) {
        __sdl_dc_is_60hz = 0;
        if (width == 320 && height == 240) disp_mode = DM_320x240_PAL;
        else if (width == 640 && height == 480) disp_mode = DM_640x480_PAL_IL;
        else {
            SDL_SetError("Couldn't find requested mode in list");
            return -1;
        }
    } else {
        __sdl_dc_is_60hz = 1; // Default to 60Hz
        if (width == 320 && height == 240) disp_mode = DM_320x240;
        else if (width == 640 && height == 480) disp_mode = DM_640x480;
        else if (width == 768 && height == 480) disp_mode = DM_768x480;
        else {
            SDL_SetError("Couldn't find requested mode in list");
            return -1;
        }
    }

    // Set pixel mode based on bpp
    switch (bpp) {
        case 15: // ARGB1555
            pixel_mode = PM_RGB555;
            Rmask = 0x00007C00; // 5 bits for Red
            Gmask = 0x000003E0; // 5 bits for Green
            Bmask = 0x0000001F; // 5 bits for Blue
            current_mode.format = SDL_PIXELFORMAT_ARGB1555; // Set the correct format
            break;
        case 16: // RGB565
            pixel_mode = PM_RGB565;
            Rmask = 0x0000F800;
            Gmask = 0x000007E0;
            Bmask = 0x0000001F;
            current_mode.format = SDL_PIXELFORMAT_RGB565; // Set the correct format
            break;
        case 24: // RGB888
        case 32: // ARGB8888
            pixel_mode = PM_RGB888;
            Rmask = 0x00FF0000;
            Gmask = 0x0000FF00;
            Bmask = 0x000000FF;
            current_mode.format = SDL_PIXELFORMAT_RGB888; // Set the correct format
            break;
        default:
            SDL_SetError("Unsupported pixel format");
            return -1;
    }

    // Initialize other properties of current_mode as needed
    current_mode.w = width;
    current_mode.h = height;
    current_mode.refresh_rate = (__sdl_dc_is_60hz) ? 60 : 50; // Set refresh rate based on __sdl_dc_is_60hz
    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    display.driverdata = NULL;

    SDL_AddDisplayMode(&display, &current_mode);
    SDL_AddVideoDisplay(&display, SDL_TRUE);

    // Set the mode using KOS
    vid_set_mode(disp_mode, pixel_mode);

    SDL_Log("SDL2 Dreamcast video initialized.");
    return 1; // Success
}


void DREAMCAST_GetDisplayModes(_THIS, SDL_VideoDisplay *display) {
    SDL_DisplayMode mode;
    int refresh_rate = __sdl_dc_is_60hz ? 60 : 50; // Determine refresh rate based on flag

    // Adding a 320x240 mode in ARGB1555 format
    SDL_zero(mode);
    mode.w = 320;
    mode.h = 240;
    mode.format = SDL_PIXELFORMAT_ARGB1555; // Change as necessary
    mode.refresh_rate = refresh_rate; // Use the determined refresh rate
    SDL_AddDisplayMode(display, &mode);

    // Adding a 640x480 mode in RGB565 format
    SDL_zero(mode);
    mode.w = 640;
    mode.h = 480;
    mode.format = SDL_PIXELFORMAT_RGB565; // Change as necessary
    mode.refresh_rate = refresh_rate; // Use the determined refresh rate
    SDL_AddDisplayMode(display, &mode);

    // Adding a 768x480 mode in RGB565 format
    SDL_zero(mode);
    mode.w = 768;
    mode.h = 480;
    mode.format = SDL_PIXELFORMAT_RGB565; // Change as necessary
    mode.refresh_rate = refresh_rate; // Use the determined refresh rate
    SDL_AddDisplayMode(display, &mode);

    // Adding a 640x480 mode in RGB888 format
    SDL_zero(mode);
    mode.w = 640;
    mode.h = 480;
    mode.format = SDL_PIXELFORMAT_RGB888; // Change as necessary
    mode.refresh_rate = refresh_rate; // Use the determined refresh rate
    SDL_AddDisplayMode(display, &mode);

    // Optionally, you can add more modes depending on your requirements
    // e.g., 800x600, 1024x768, etc. 

    SDL_Log("Display modes for Dreamcast retrieved successfully.");
}

int DREAMCAST_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode) {
    int pixel_mode;
    int disp_mode = -1; // Initialize to an invalid value

    // Determine the appropriate display mode based on width and height
    if (__sdl_dc_is_60hz) {
        if (mode->w == 320 && mode->h == 240) {
            disp_mode = DM_320x240; // 60Hz mode
        } else if (mode->w == 640 && mode->h == 480) {
            disp_mode = DM_640x480; // 60Hz mode
        } else if (mode->w == 768 && mode->h == 480) {
            disp_mode = DM_768x480; // 60Hz mode
        }
    } else { // 50Hz
        if (mode->w == 320 && mode->h == 240) {
            disp_mode = DM_320x240_PAL; // 50Hz mode
        } else if (mode->w == 640 && mode->h == 480) {
            disp_mode = DM_640x480_PAL_IL; // 50Hz mode
        }
    }

    // Check if the determined display mode is valid
    if (disp_mode < 0) {
        SDL_SetError("Unsupported display mode");
        return -1;
    }

    // Determine pixel mode based on the format in the mode
    switch (mode->format) {
        case SDL_PIXELFORMAT_ARGB1555:
            pixel_mode = PM_RGB555; // Use the correct constant
            break;
        case SDL_PIXELFORMAT_RGB565:
            pixel_mode = PM_RGB565; // Keep as is
            break;
        case SDL_PIXELFORMAT_RGB888:
            pixel_mode = PM_RGB888; // Keep as is
            break;
        case SDL_PIXELFORMAT_UNKNOWN:
        default:
            SDL_SetError("Unsupported pixel format");
            return -1;
    }

    // Set the video mode using KOS (no return value handling)
    vid_set_mode(disp_mode, pixel_mode); // Assuming this function works fine without return checks

    SDL_Log("Display mode set to: %dx%d @ %dHz", mode->w, mode->h, mode->refresh_rate);
    return 0; // Return success
}

void DREAMCAST_VideoQuit(_THIS)
{
#ifdef SDL_INPUT_LINUXEV
    SDL_EVDEV_Quit();
#endif
}

#endif /* SDL_VIDEO_DRIVER_DREAMCAST */

/* vi: set ts=4 sw=4 expandtab: */
