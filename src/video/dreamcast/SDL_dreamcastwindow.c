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

#include "../SDL_sysvideo.h"

#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "SDL_dreamcastvideo.h"
#include "SDL_dreamcastwindow.h"

int DREAMCAST_CreateWindow(SDL_VideoDevice *_this, SDL_Window * window)
{
    SDL_WindowData *dreamcast_window = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));

    if (!dreamcast_window) {
        return SDL_OutOfMemory();
    }
    window->internal  = dreamcast_window;

    /* Adjust the window data to match the screen */
    if (window->x == SDL_WINDOWPOS_UNDEFINED) {
        window->x = 0;
    }

    if (window->y == SDL_WINDOWPOS_UNDEFINED) {
        window->y = 0;
    }

    dreamcast_window->sdl_window = window;

    window->flags &= ~SDL_WINDOW_RESIZABLE;     /* window is NEVER resizeable */
    window->flags &= ~SDL_WINDOW_HIDDEN;
    window->flags |= SDL_WINDOW_INPUT_FOCUS;    /* always has input focus */
    window->flags |= SDL_WINDOW_OPENGL;

    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);

    return 0;
}

void DREAMCAST_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    /* TODO */
}

void DREAMCAST_DestroyWindow(SDL_VideoDevice *_this, SDL_Window * window)
{
    SDL_WindowData *dreamcast_window = window->internal;
    if (dreamcast_window) {
        SDL_free(dreamcast_window);
    }

    window->internal = NULL;
}

#endif /* SDL_VIDEO_DRIVER_DREAMCAST */

/* vi: set ts=4 sw=4 expandtab: */
