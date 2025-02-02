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

    Sam Lantinga
    slouken@libsdl.org
*/

#ifndef _SDL_config_dreamcast_h
#define _SDL_config_dreamcast_h

#include "SDL_platform.h"

#define HAVE_O_CLOEXEC 1
/* Enable the DREAMCAST thread support (src/thread/dreamcast/\*.c) */
#define SDL_THREAD_DREAMCAST 0
#define SDL_PTHREADS 1

/* Enable the DREAMCAST timer support (src/timer/dreamcast/\*.c) */
#define SDL_TIMER_DREAMCAST 0
#define SDL_TIMER_UNIX 1

/* Enable the DREAMCAST video driver (src/video/dreamcast/\*.c) */
#define SDL_VIDEO_DRIVER_DREAMCAST 1
#define SDL_VIDEO_RENDER_DREAMCAST_PVR 0
#define SDL_VIDEO_DRIVER_DUMMY	0
#define SDL_VIDEO_DRIVER_OFFSCREEN 0

/* Enable the dummy audio driver (src/audio/dreamcast/\*.c) */
#define SDL_AUDIO_DRIVER_DREAMCAST 1

/* Enable the stub joystick driver (src/joystick/dreamcast/\*.c) */
#define SDL_JOYSTICK_DREAMCAST   1

/* Enable the dreamcast haptic driver (src/haptic/dreamcast/\*.c) */
#define SDL_HAPTIC 1
#define SDL_HAPTIC_DREAMCAST 1

/* Enable the stub HIDAPI */
#define SDL_HIDAPI_DISABLED 1

/* Enable the stub sensor driver (src/sensor/dummy/\*.c) */
#define SDL_SENSOR_DISABLED 1

/* Enable the stub shared object loader (src/loadso/dummy/\*.c) */
#define SDL_LOADSO_DISABLED 1

/* Enable the DREAMCAST filesystem driver (src/filesystem/dreamcast/\*.c) */
#define SDL_FILESYSTEM_DREAMCAST 1

#endif /* _SDL_config_dreamcast_h */
