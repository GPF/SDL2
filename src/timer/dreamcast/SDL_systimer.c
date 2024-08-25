/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    BERO
    bero@geocities.co.jp

    based on win32/SDL_systimer.c

    Sam Lantinga
    slouken@libsdl.org
*/
#define HAVE_O_CLOEXEC 1
#include "SDL_config.h"

#ifdef SDL_TIMER_DREAMCAST

#include <kos.h>
#include "SDL_thread.h"
#include "SDL_timer.h"
#include "SDL_error.h"
#include "../SDL_timer_c.h"

static SDL_bool ticks_started = SDL_FALSE;
static Uint64 start;

void SDL_TicksInit(void)
{
    uint32 s, ms;
	uint64 msec;
    if (ticks_started) {
        return;
    }
    ticks_started = SDL_TRUE;
	timer_ms_gettime(&s, &ms);
	msec = (((uint64)s) * ((uint64)1000)) + ((uint64)ms);

	start = (Uint32)msec;
}

void SDL_TicksQuit(void)
{
    ticks_started = SDL_FALSE;
}

Uint64 SDL_GetTicks64(void)
{
    uint32 s, ms;
	uint64 msec;    
    if (!ticks_started) {
        SDL_TicksInit();
    }

    // Implement time retrieval logic
	timer_ms_gettime(&s, &ms);
	msec = (((uint64)s) * ((uint64)1000)) + ((uint64)ms);

	return (Uint32)msec - start;
}

void SDL_Delay(Uint32 ms)
{
    // Implement delay functionality
    thd_sleep(ms);
}

Uint64 SDL_GetPerformanceCounter(void)
{
    uint32 s, ms;
	uint64 msec;       
	timer_ms_gettime(&s, &ms);
	msec = (((uint64)s) * ((uint64)1000)) + ((uint64)ms);

	return (Uint32)msec;
}

Uint64 SDL_GetPerformanceFrequency(void)
{
    return 1000000;
}


#endif /* SDL_TIMER_DREAMCAST */


