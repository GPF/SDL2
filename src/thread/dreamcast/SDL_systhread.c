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

    based on generic/SDL_thread.c

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

/* Thread management routines for SDL */
#include "SDL_error.h"
#include "SDL_thread.h"
#include "../SDL_thread_c.h"
#include "../SDL_systhread.h"

#include <kos/thread.h>
 
#ifdef SDL_PASSED_BEGINTHREAD_ENDTHREAD
static int thdfunc(void *args)
{
    SDL_RunThread(args);
    return 0;  // Ensure return type matches SDL_ThreadFunction signature
}

int SDL_SYS_CreateThread(SDL_Thread *thread,
                         pfnSDL_CurrentBeginThread pfnBeginThread,
                         pfnSDL_CurrentEndThread pfnEndThread)
{
    thread->handle = thd_create(true, (void *(*)(void *))thdfunc, thread);
    if (thread->handle == NULL) {
        SDL_SetError("Not enough resources to create thread");
        return -1;
    }
    return 0;
}
#else
int SDL_SYS_CreateThread(SDL_Thread *thread)
{
    thread->handle = thd_create(true, (void *(*)(void *))SDL_RunThread, thread);
    if (thread->handle == NULL) {
        SDL_SetError("Not enough resources to create thread");
        return -1;
    }
    return 0;
}
#endif

void SDL_SYS_SetupThread(const char *name)
{
    kthread_t * thid = thd_get_current(); // Get the current thread ID
    thd_set_label(thid, name);
}

Uint32 SDL_ThreadID(void)
{
    return (Uint32)thd_get_current();
}

void SDL_SYS_WaitThread(SDL_Thread *thread)
{
    thd_join(thread->handle, NULL);
}

void SDL_SYS_KillThread(SDL_Thread *thread)
{
    thd_destroy(thread->handle);
}

int SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority) {
    kthread_t * thid = thd_get_current(); // Get the current thread ID
    thd_set_prio(thid, priority); // Set a higher priority

    return 0;
}
