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
     claim that you wrote the original software. If you use this software in a product,
     an acknowledgment in the product documentation would be appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#ifdef SDL_AUDIO_DRIVER_DREAMCAST

#include <SDL3/SDL_audio.h>
#include "../SDL_audio_c.h"
#include "../SDL_sysaudio.h"
#include "SDL_dreamcastaudio.h"  /* defines SDL_PrivateAudioData */
#include <dc/sound/stream.h>
#include <dc/sound/sound.h>
#include <kos/thread.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_hints.h>
#include "kos.h"



/*
 * Return pointer to the current active buffer.
 * The SDL mixing thread writes raw 4-bit ADPCM data here.
 */
static Uint8 *DREAMCASTAUD_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    struct SDL_PrivateAudioData *hidden = (struct SDL_PrivateAudioData *)device->hidden;
    return hidden->mixbuf[ SDL_GetAtomicInt(&hidden->active_buffer) ];
}

/* Global device references */
static SDL_AudioDevice *audioDevice = NULL;   /* Active output device */
/*
 * Stream callback invoked by the KOS sound stream system.
 * We lock the mutex to safely access our atomic flags.
 */
static void *stream_callback(snd_stream_hnd_t hnd, int req, int *done)
{
    SDL_AudioDevice *device = audioDevice;
    struct SDL_PrivateAudioData *hidden;

    *done = 0;
    if (!device || !device->hidden || req <= 0) {
        return NULL;
    }

    hidden = (struct SDL_PrivateAudioData *)device->hidden;
    const int buffer_size = hidden->buffer_size;

    if (SDL_GetAtomicInt(&hidden->buffer_ready)) {
        const int current_active = SDL_GetAtomicInt(&hidden->active_buffer);
        const int next_buf = current_active ^ 1;

        *done = SDL_min(req, buffer_size);
        SDL_SetAtomicInt(&hidden->active_buffer, next_buf);
        SDL_SetAtomicInt(&hidden->buffer_ready, 0);

        return hidden->mixbuf[next_buf];
    }

    // Optional: rate-limit or remove in production
    SDL_Log("Stream callback: no data available");
    return hidden->mixbuf[SDL_GetAtomicInt(&hidden->active_buffer)];
}

/*
 * WaitDevice - block until the current buffer has been consumed.
 */
static bool DREAMCASTAUD_WaitDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *hidden = (struct SDL_PrivateAudioData *)device->hidden;

    while (SDL_GetAtomicInt(&hidden->buffer_ready)) {
        snd_stream_poll(hidden->stream_handle);
        SDL_Delay(1);
    }

    return true;
}



/*
 * PlayDevice - mark the current mix buffer as ready.
 * We assume that the SDL mixing thread writes raw 4-bit ADPCM data
 * into the active mix buffer (via SDL_GetDeviceBuf), so here we only need
 * to flag that the buffer is ready and nudge the KOS stream.
 */
static bool DREAMCASTAUD_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buffer_size)
{
    struct SDL_PrivateAudioData *hidden = (struct SDL_PrivateAudioData *)device->hidden;

    // Wait until previous buffer is consumed
    DREAMCASTAUD_WaitDevice(device);

    // Mark buffer as ready
    SDL_SetAtomicInt(&hidden->buffer_ready, 1);

    return true;
}


/*
 * Thread initialization: raise thread priority.
 */
static void DREAMCASTAUD_ThreadInit(SDL_AudioDevice *device)
{
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);
}

static void DREAMCASTAUD_ThreadDeinit(SDL_AudioDevice *device)
{
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW);
}

/*
 * Open the audio device.
 * This function initializes the KOS sound stream, allocates double-buffering,
 * and sets up the stream callback.
 */
static bool DREAMCASTAUD_OpenDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *hidden;
    const SDL_AudioFormat *closefmts;
    SDL_AudioFormat test_format;
    int channels, frequency;

    SDL_Log("Opening Dreamcast audio device");

    hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*hidden));
    if (!hidden) {
        return false;
    }
    device->hidden = hidden;

    // if (snd_stream_init() != 0) {
    //     SDL_free(hidden);
    //     return SDL_SetError("Failed to initialize sound stream system");
    // }

    // Pick supported audio format using SDL_ClosestAudioFormats
    closefmts = SDL_ClosestAudioFormats(device->spec.format);
    while ((test_format = *(closefmts++)) != 0) {
        if (test_format == SDL_AUDIO_S8 || test_format == SDL_AUDIO_S16LE) {
            device->spec.format = test_format;
            break;
        }
    }

    if (!test_format) {
        SDL_free(hidden);
        snd_stream_shutdown();
        return SDL_SetError("Unsupported audio format");
    }

    // Align sample count if necessary (SDL3 manages a lot of this internally)
    device->sample_frames = (device->sample_frames + 31) & ~31;  // Align to 32
    SDL_UpdatedAudioDeviceFormat(device);  // Updates device->buffer_size etc

    hidden->buffer_size = device->buffer_size;

    hidden->stream_handle = snd_stream_alloc(NULL, hidden->buffer_size);
    if (hidden->stream_handle == SND_STREAM_INVALID) {
        SDL_free(hidden);
        snd_stream_shutdown();
        return SDL_SetError("Failed to allocate sound stream");
    }

    hidden->mixbuf[0] = (Uint8 *)memalign(32, hidden->buffer_size);
    hidden->mixbuf[1] = (Uint8 *)memalign(32, hidden->buffer_size);
    if (!hidden->mixbuf[0] || !hidden->mixbuf[1]) {
        SDL_free(hidden->mixbuf[0]);
        SDL_free(hidden->mixbuf[1]);
        SDL_free(hidden);
        snd_stream_shutdown();
        return SDL_OutOfMemory();
    }

    SDL_memset(hidden->mixbuf[0], 0, hidden->buffer_size);
    SDL_memset(hidden->mixbuf[1], 0, hidden->buffer_size);

    snd_stream_reinit(hidden->stream_handle, stream_callback);

    channels = device->spec.channels;
    frequency = device->spec.freq;

    const char *adpcm_hint = SDL_GetHint("SDL_AUDIO_ADPCM_STREAM_DC");
    if (adpcm_hint && SDL_strcmp(adpcm_hint, "1") == 0) {
        SDL_Log("Using 4-bit ADPCM audio");
        snd_stream_start_adpcm(hidden->stream_handle, frequency, (channels == 2) ? 1 : 0);
    } else if (device->spec.format == SDL_AUDIO_S16LE) {
        SDL_Log("Using 16-bit PCM audio");
        snd_stream_start(hidden->stream_handle, frequency, (channels == 2) ? 1 : 0);
    } else if (device->spec.format == SDL_AUDIO_S8) {
        SDL_Log("Using 8-bit PCM audio");
        snd_stream_start_pcm8(hidden->stream_handle, frequency, (channels == 2) ? 1 : 0);
    } else {
        SDL_free(hidden->mixbuf[0]);
        SDL_free(hidden->mixbuf[1]);
        SDL_free(hidden);
        snd_stream_shutdown();
        return SDL_SetError("Unsupported audio format");
    }

    SDL_SetAtomicInt(&hidden->active_buffer, 0);
    SDL_SetAtomicInt(&hidden->buffer_ready, 0);

    audioDevice = device;

    SDL_Log("Dreamcast audio driver initialized successfully");
    return true;
}

/*
 * Close the audio device.
 */
static void DREAMCASTAUD_CloseDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *hidden = (struct SDL_PrivateAudioData *)device->hidden;

    SDL_Log("Closing audio device\n");

    if (hidden) {
        if (hidden->stream_handle != SND_STREAM_INVALID) {
            SDL_Log("Stopping and destroying sound stream\n");
            snd_stream_stop(hidden->stream_handle);
            snd_stream_destroy(hidden->stream_handle);
            hidden->stream_handle = SND_STREAM_INVALID;
        }

        if (hidden->mixbuf[0]) {
            SDL_free(hidden->mixbuf[0]);
            hidden->mixbuf[0] = NULL;
        }
        if (hidden->mixbuf[1]) {
            SDL_free(hidden->mixbuf[1]);
            hidden->mixbuf[1] = NULL;
        }

        SDL_free(hidden);
        device->hidden = NULL;
    }

    audioDevice = NULL;
    SDL_Log("Audio device closed\n");

    snd_stream_shutdown();
}


/*
 * Initialize the SDL3 Dreamcast audio driver.
 */
static bool DREAMCASTAUD_Init(SDL_AudioDriverImpl *impl)
{

        /* Initialize the sound stream system */
    if (snd_stream_init() != 0) {
        return SDL_SetError("Failed to initialize sound stream system");
    }

    impl->OpenDevice  = DREAMCASTAUD_OpenDevice;
    impl->CloseDevice = DREAMCASTAUD_CloseDevice;
    impl->PlayDevice  = DREAMCASTAUD_PlayDevice;
    impl->WaitDevice  = DREAMCASTAUD_WaitDevice;
    impl->GetDeviceBuf = DREAMCASTAUD_GetDeviceBuf;
    impl->ThreadInit  = DREAMCASTAUD_ThreadInit;
    impl->ThreadDeinit = DREAMCASTAUD_ThreadDeinit;
    impl->OnlyHasDefaultPlaybackDevice = true;
    return true;
}

AudioBootStrap DREAMCASTAUDIO_bootstrap = {
    "dcstreamingaudio", "SDL Dreamcast Streaming Audio Driver",
    DREAMCASTAUD_Init, false, false
};


#endif /* SDL_AUDIO_DRIVER_DREAMCAST */
