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
#include <kos.h> 
#include "../SDL_sysvideo.h"
#include "SDL_dreamcastframebuffer_c.h"

#define DREAMCAST_SURFACE "_SDL_DreamcastSurface"

int SDL_DREAMCAST_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    SDL_Surface *surface;
    const Uint32 surface_format = SDL_PIXELFORMAT_RGB888;
    int w, h;

    /* Free the old framebuffer surface */
    SDL_DREAMCAST_DestroyWindowFramebuffer(_this, window);

    /* Create a new one */
    SDL_GetWindowSizeInPixels(window, &w, &h);
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, surface_format);
    if (!surface) {
        return -1;
    }

    /* Save the info and return! */
    SDL_SetWindowData(window, DREAMCAST_SURFACE, surface);
    *format = surface_format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;
    return 0;
}

#include <string.h> // For memcpy

int SDL_DREAMCAST_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_Surface *surface;
    Uint32 *dst;
    int w, h, pitch, i;
    Uint32 *src_row;

    surface = (SDL_Surface *)SDL_GetWindowData(window, DREAMCAST_SURFACE);
    if (!surface) {
        return SDL_SetError("Couldn't find framebuffer surface for window");
    }

    // Retrieve dimensions and pitch of the SDL surface
    w = surface->w;
    h = surface->h;
    pitch = surface->pitch;

    // Assuming vram_l points to the start of the framebuffer
    dst = (Uint32 *)vram_l;
    src_row = (Uint32 *)surface->pixels;
        
    // Update entire framebuffer if no rects are provided
    if (numrects == 0) {
        // Copy the entire surface to the framebuffer
        for (int y = 0; y < h; ++y) {
            memcpy(dst + (y * (pitch / sizeof(Uint32))), src_row + (y * (pitch / sizeof(Uint32))), w * sizeof(Uint32));
        }
    } else {
        // Update only specified rectangles
        for (i = 0; i < numrects; ++i) {
            const SDL_Rect *rect = &rects[i];
            for (int y = rect->y; y < rect->y + rect->h; ++y) {
                memcpy(dst + ((y * (pitch / sizeof(Uint32))) + rect->x), src_row + ((y * (pitch / sizeof(Uint32))) + rect->x), rect->w * sizeof(Uint32));
            }
        }
    }

    return 0;
}


void SDL_DREAMCAST_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_Surface *surface;

    surface = (SDL_Surface *)SDL_SetWindowData(window, DREAMCAST_SURFACE, NULL);
    SDL_FreeSurface(surface);
}


#endif /* SDL_VIDEO_DRIVER_DREAMCAST */

/* vi: set ts=4 sw=4 expandtab: */
