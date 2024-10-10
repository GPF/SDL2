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

#ifdef SDL_AUDIO_DRIVER_DREAMCAST

#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "SDL_dreamcastaudio.h" // Defines SDL_PrivateAudioData struct
#include <dc/sound/stream.h> // Include KOS sound stream library
#include <kos/thread.h>
#include "../SDL_sysaudio.h"
#include "SDL_timer.h"
#include "kos.h"

static SDL_AudioDevice *audioDevice = NULL;   // Pointer to the active audio output device
static SDL_AudioDevice *captureDevice = NULL; // Pointer to the active audio capture device

// Stream callback function
static void *stream_callback(snd_stream_hnd_t hnd, int req, int *done) {
    SDL_AudioDevice *device = audioDevice;
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)device->hidden;
    void *buffer = hidden->buffer; // Use hidden->buffer for interleaving
    int buffer_size = device->spec.size;
    int bytes_to_copy = SDL_min(req, buffer_size);
    int got = 0;
    SDL_AudioCallback callback;

    SDL_LockMutex(device->mixer_lock);

    if (!buffer) {
        SDL_Log("Buffer is NULL.");
        *done = 0;
        SDL_UnlockMutex(device->mixer_lock);
        return NULL;
    }

    callback = device->callbackspec.callback;

    if (!SDL_AtomicGet(&device->enabled) || SDL_AtomicGet(&device->paused)) {
        // Audio is not enabled or is paused, so clear the stream and set silence
        if (device->stream) {
            SDL_AudioStreamClear(device->stream);
        }
        SDL_memset(buffer, device->callbackspec.silence, bytes_to_copy);
    } else {
        SDL_assert(device->spec.size == bytes_to_copy);

        // No stream conversion needed
        if (!device->stream) {
            callback(device->callbackspec.userdata, buffer, bytes_to_copy);
        } else {
            // Stream conversion is needed
            while (SDL_AudioStreamAvailable(device->stream) < bytes_to_copy) {
                callback(device->callbackspec.userdata, hidden->buffer, buffer_size);
                if (SDL_AudioStreamPut(device->stream, hidden->buffer, buffer_size) == -1) {
                    SDL_AudioStreamClear(device->stream);
                    SDL_AtomicSet(&device->enabled, 0);
                    break;
                }
            }

            got = SDL_AudioStreamGet(device->stream, buffer, bytes_to_copy);
            if (got != bytes_to_copy) {
                SDL_memset(buffer, device->callbackspec.silence, bytes_to_copy);
            }
        }
    }

    SDL_UnlockMutex(device->mixer_lock);

    *done = req;
    return buffer;  // Return the interleaved buffer
}

// Thread function for audio playback
static int SDLCALL DREAMCASTAUD_Thread(void *data) {
    SDL_AudioDevice *device = (SDL_AudioDevice *)data;
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)device->hidden;

    SDL_Log("Audio thread started\n");

    while (SDL_AtomicGet(&device->enabled)) {
        // Call snd_stream_poll to check if more audio data is needed
        int result = snd_stream_poll(hidden->stream_handle);
        if (result < 0) {
            SDL_Log("snd_stream_poll failed: %d\n", result);
            SDL_AtomicSet(&device->enabled, 0);
            break;
        }
        SDL_Delay(10); // Avoid busy looping
    }

    SDL_Log("Audio thread exiting\n");

    return 0;
}

// Open the audio device
int DREAMCASTAUD_OpenDevice(_THIS, const char *devname) {
    SDL_PrivateAudioData *hidden;
    SDL_AudioFormat test_format;
    int channels,frequency;
    audioDevice = _this;

    hidden = (SDL_PrivateAudioData *)SDL_malloc(sizeof(*hidden));
    if (!hidden) {
        return SDL_OutOfMemory();
    }
    SDL_zerop(hidden);
    _this->hidden = (struct SDL_PrivateAudioData*)hidden;

    // Initialize sound stream system
    if (snd_stream_init() != 0) {
        SDL_free(hidden);
        return SDL_SetError("Failed to initialize sound stream system");
    }

    // Find a compatible format
    for (test_format = SDL_FirstAudioFormat(_this->spec.format); test_format; test_format = SDL_NextAudioFormat()) {
        if ((test_format == AUDIO_S8) || (test_format == AUDIO_S16LSB)) {
            _this->spec.format = test_format;
            break;
        }
    }

    if (!test_format) {
        /* Didn't find a compatible format :( */
        snd_stream_shutdown();
        SDL_free(hidden);
        return SDL_SetError("Dreamcast unsupported audio format: 0x%x", _this->spec.format);
    }

    SDL_CalculateAudioSpec(&_this->spec);

    // Allocate the stream with the data callback
    hidden->stream_handle = snd_stream_alloc(NULL, _this->spec.size);
    if (hidden->stream_handle == SND_STREAM_INVALID) {
        SDL_free(hidden);
        snd_stream_shutdown();
        return SDL_SetError("Failed to allocate sound stream");
    }

    // Allocate a single buffer for reuse
    hidden->buffer = (Uint8 *)memalign(32, _this->spec.size); // Ensure 32-byte alignment
    if (!hidden->buffer) {
        SDL_free(hidden);
        snd_stream_shutdown();
        return SDL_OutOfMemory();
    }

    // Set stream callback and start playback
    snd_stream_reinit(hidden->stream_handle, stream_callback);
    snd_stream_volume(hidden->stream_handle, 255); // Max volume

    channels = _this->spec.channels;
    frequency = _this->spec.freq;

    // Check the audio format and initialize the stream accordingly
    if (_this->spec.format == AUDIO_S16LSB) {
        // Initialize stream for 16-bit PCM
        snd_stream_start(hidden->stream_handle, frequency, (channels == 2) ? 1 : 0);
    } else if (_this->spec.format == AUDIO_S8) {
        // Initialize stream for 8-bit PCM
        snd_stream_start_pcm8(hidden->stream_handle, frequency, (channels == 2) ? 1 : 0);
    } else {
        SDL_SetError("Unsupported audio format: %d", _this->spec.format);
        return -1;
    }
    // Start the playback thread
    SDL_AtomicSet(&_this->enabled, 1); // Enable audio
    _this->thread = SDL_CreateThread(DREAMCASTAUD_Thread, "DreamcastAudioThread", _this);
    if (!_this->thread) {
        SDL_AtomicSet(&_this->enabled, 0);
        SDL_free(hidden);
        snd_stream_shutdown();
        return SDL_SetError("Failed to create audio thread");
    }

    SDL_Log("Dreamcast audio driver initialized\n");
    return 0;
}

// Close the audio device
static void DREAMCASTAUD_CloseDevice(_THIS) {
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)_this->hidden;

    SDL_Log("Closing audio device\n");

    // Disable the audio device and wait for the thread to finish
    SDL_AtomicSet(&_this->enabled, 0);
    if (_this->thread) {
        SDL_WaitThread(_this->thread, NULL);
        _this->thread = NULL;
    }

    if (hidden) {
        if (hidden->stream_handle != SND_STREAM_INVALID) {
            snd_stream_destroy(hidden->stream_handle);
        }
        if (hidden->buffer) {
            SDL_free(hidden->buffer);
        }
        SDL_free(hidden);
        _this->hidden = NULL;
    }

    if (_this->iscapture) {
        captureDevice = NULL;
    } else {
        audioDevice = NULL;
    }

    snd_stream_shutdown();
}

// Initialize the SDL2 Dreamcast audio driver
static SDL_bool DREAMCASTAUD_Init(SDL_AudioDriverImpl *impl) {
    impl->OpenDevice  = DREAMCASTAUD_OpenDevice;
    impl->CloseDevice = DREAMCASTAUD_CloseDevice;
    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;    
    impl->ProvidesOwnCallbackThread = SDL_TRUE;
    return SDL_TRUE;
}

AudioBootStrap DREAMCASTAUD_bootstrap = {
    "dcstreamingaudio", "SDL Dreamcast Streaming Audio Driver",
    DREAMCASTAUD_Init, SDL_FALSE
};

#endif /* SDL_AUDIO_DRIVER_DREAMCAST */
