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

    Falco Girgis
    gyrovorbis@gmail.com
*/
#define HAVE_O_CLOEXEC 1
#include "SDL_config.h"

#ifdef SDL_TIMER_DREAMCAST

#include <kos.h>
#include "SDL_thread.h"
#include "SDL_timer.h"
#include "SDL_error.h"
#include "../SDL_timer_c.h"

static Uint64 start;

/* Cache the value of KOS's monotonic ms timer when SDL is initialized. */
void SDL_TicksInit(void)
{
    if (start) {
        return;
    }
	start = timer_ms_gettime64();
}

void SDL_TicksQuit(void)
{
    start = 0;
}

/* SDL Ticks are using KOS's Timer API, which maintains monotonic ticks,
   using one of the TMUs on the SH4 CPU.
*/
Uint64 SDL_GetTicks64(void)
{ 
    if (!start) {
        SDL_TicksInit();
    }

	return timer_ms_gettime64() - start;
}

void SDL_Delay(Uint32 ms)
{
    // Implement delay functionality
    thd_sleep(ms);
}

/* Retrieve real 5ns resolution performance counter ticks.
   NOTE: Like any real performance counter, they cease to increment
         while the processor is asleep ("SLEEP" instruction on SH4,
         happens in the KOS idle thread).
   
   NOTE: If the user were to manually take control over the performance
         counter KOS uses for maintaining these ticks, the driver is smart
         enough to fall back to TMU-ticks from the timer.h driver, which has
         80ns maximum resolution. 
*/
Uint64 SDL_GetPerformanceCounter(void)
{
    return perf_cntr_timer_ns();
}

Uint64 SDL_GetPerformanceFrequency(void)
{
    return 1000000000;
}


#endif /* SDL_TIMER_DREAMCAST */


