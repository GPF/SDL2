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

/* Being a null driver, there's no event stream. We just define stubs for
   most of the API. */

#include "SDL_dreamcastevents_c.h"
#include <dc/maple.h>
#include <dc/maple/mouse.h>
#include <dc/maple/keyboard.h>
#include <stdio.h>
#include <arch/arch.h>
#include <arch/timer.h>
#include <arch/irq.h>
#include "SDL_dreamcastvideo.h"
#include "SDL_dreamcastevents_c.h"
#define MIN_FRAME_UPDATE 16

static __inline__ Uint32 myGetTicks(void)
{
	return ((timer_us_gettime64()>>10));
}

void DREAMCAST_PumpEvents(_THIS)
{
	// static Uint32 last_time=0;
	// Uint32 now=myGetTicks();
	// if (now-last_time>MIN_FRAME_UPDATE)
	// {
	// 	keyboard_update();
	// 	mouse_update();
	// 	last_time=now;
	// }
}

#endif /* SDL_VIDEO_DRIVER_DREAMCAST */

/* vi: set ts=4 sw=4 expandtab: */
