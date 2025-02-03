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
#include <dc/sound/sound.h>
#include <kos/thread.h>
#include "../SDL_sysaudio.h"
#include "SDL_timer.h"
#include "SDL_hints.h"
#include "kos.h"

static SDL_AudioDevice *audioDevice = NULL;   // Pointer to the active audio output device
static SDL_AudioDevice *captureDevice = NULL; // Pointer to the active audio capture device

static Uint8 * DREAMCASTAUD_GetDeviceBuf(_THIS) {
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)_this->hidden;
    return hidden->mixbuf[hidden->buffer];
}

// Stream callback function
static void *stream_callback(snd_stream_hnd_t hnd, int req, int *done) {
    SDL_AudioDevice *device = audioDevice;
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)device->hidden;
    int buffer_size = hidden->buffer_size;
    *done = 0;
    Uint8 *buffer;
    // Only fill the buffer if it's marked as "ready"
    if (hidden->buffer_ready) {
        buffer = (Uint8 *)DREAMCASTAUD_GetDeviceBuf(device);
        *done = SDL_min(req, buffer_size);
        hidden->buffer_ready = 0; // Mark buffer as consumed
        SDL_Log("stream_callback - filled %d bytes", *done);
    }

   return buffer;
}

static void DREAMCASTAUD_WaitDevice(_THIS) {
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)_this->hidden;
    if (SDL_AtomicGet(&_this->paused)) return;

    // Wait until the current buffer is no longer marked as ready
    while (hidden->buffer_ready) {
        snd_stream_poll(hidden->stream_handle);
        SDL_Delay(1);
    }

    
}
static void DREAMCASTAUD_PlayDevice(_THIS) {
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)_this->hidden;
    if (SDL_AtomicGet(&_this->paused)) return;

    DREAMCASTAUD_WaitDevice(_this);
    hidden->buffer_ready = 1; // Signal that the buffer is ready for filling
    hidden->buffer ^= 1; // Toggle buffer index
    SDL_Log("Switched to buffer %d", hidden->buffer);
}


static void DREAMCASTAUD_ThreadInit(_THIS) {
    // SDL_Log("DREAMCASTAUD_ThreadInit");
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
}

// Mono-to-stereo filter function
void mono_to_stereo_filter(snd_stream_hnd_t hnd, void *obj, int hz, int channels, void **buffer, int *samplecnt) {
    // Only perform conversion if the audio is mono (1 channel)
    // if (channels == 1) {
    SDL_Log("mono_to_stereo_filter");
        int samples = *samplecnt;
        Uint8 *input_buffer = (Uint8 *)(*buffer);
        
        // Allocate a buffer for stereo output (twice the size of the input buffer)
        Uint8 *stereo_buffer = malloc(samples);
        if (!stereo_buffer) {
            SDL_SetError("Failed to allocate stereo buffer");
            return;
        }

        // Duplicate mono samples to both left and right channels
        for (int i = 0; i < samples; i++) {
            stereo_buffer[i * 2] = input_buffer[i];      // Left channel
            stereo_buffer[i * 2 + 1] = input_buffer[i];  // Right channel
        }

        // Update buffer and sample count to reflect stereo data
        *buffer = stereo_buffer;
        *samplecnt = samples ;  // Update to reflect stereo sample count
    // }
}

// Open the audio device
int DREAMCASTAUD_OpenDevice(_THIS, const char *devname) {
    SDL_PrivateAudioData *hidden;
    SDL_AudioFormat test_format;
    int channels,frequency;
    char *hint_value;
    audioDevice = _this;
    

    hidden = (SDL_PrivateAudioData *)SDL_malloc(sizeof(*hidden));
    if (!hidden) {
        return SDL_OutOfMemory();
    }
    SDL_zerop(hidden);
    _this->hidden = (struct SDL_PrivateAudioData*)hidden;

    // // Read the hint for direct buffer access -- Not supported yet
    // hint_value = SDL_GetHint(SDL_HINT_AUDIO_DIRECT_BUFFER_ACCESS_DC);
    // if (hint_value && SDL_strcasecmp(hint_value, "1") == 0) {
    //     hidden->direct_buffer_access = SDL_TRUE;
    //     SDL_Log("Direct buffer access enabled for Dreamcast audio driver.");
    // } else {
    //     hidden->direct_buffer_access = SDL_FALSE;
    //     SDL_Log("Direct buffer access disabled for Dreamcast audio driver.");
    // }

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

    hidden->buffer_size = _this->spec.size* ((_this->spec.channels == 1) ? 2 : 1);

    // Allocate the stream with the data callback
    hidden->stream_handle = snd_stream_alloc(NULL, hidden->buffer_size);
    if (hidden->stream_handle == SND_STREAM_INVALID) {
        SDL_free(hidden);
        snd_stream_shutdown();
        return SDL_SetError("Failed to allocate sound stream");
    }

    hidden->mixbuf[0] = (Uint8 *)memalign(32, hidden->buffer_size);
    hidden->mixbuf[1] = (Uint8 *)memalign(32, hidden->buffer_size);
    if (!hidden->mixbuf[0] || !hidden->mixbuf[1]) {
        SDL_free(hidden);
        snd_stream_shutdown();
        return SDL_OutOfMemory();
    }

    SDL_memset(hidden->mixbuf[0], _this->spec.silence, hidden->buffer_size);
    SDL_memset(hidden->mixbuf[1], _this->spec.silence, hidden->buffer_size);

    // Set stream callback and start playback
    snd_stream_reinit(hidden->stream_handle, stream_callback);
    // snd_stream_volume(hidden->stream_handle, 128); // half volume

    channels = _this->spec.channels;
    frequency = _this->spec.freq;

    // Set panning if it's mono
    // if (channels == 1) {
    //     snd_stream_pan(hidden->stream_handle, 128, 128); // Centered mono panning
    //     // Add the mono-to-stereo filter to the stream
    //     snd_stream_filter_add(hidden->stream_handle, mono_to_stereo_filter, NULL);
    //     channels =2 ;
    // }
    
    // Check the audio format and initialize the stream accordingly
    const char *adpcm_hint = SDL_GetHint("SDL_AUDIO_ADPCM_STREAM_DC");
    if (adpcm_hint && SDL_strcmp(adpcm_hint, "1") == 0) {
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, "4-bit ADPCM audio format enabled\n");
        fflush(stdout);
        // hidden->use_adpcm = SDL_TRUE;
        snd_stream_start_adpcm(hidden->stream_handle, frequency, (channels == 2) ? 1 : 0);
    } else if (_this->spec.format == AUDIO_S16LSB) {
        snd_stream_start(hidden->stream_handle, frequency, (channels == 2) ? 1 : 0);
    } else if (_this->spec.format == AUDIO_S8) {
        snd_stream_start_pcm8(hidden->stream_handle, frequency, (channels == 2) ? 1 : 0);
    } else {
        SDL_SetError("Unsupported audio format: %d", _this->spec.format);
        return -1;
    }
    // // Start the playback thread
    // SDL_AtomicSet(&_this->enabled, 1); // Enable audio
    // SDL_AtomicSet(&_this->paused, 0); // Enable audio
    // _this->thread = SDL_CreateThread(DREAMCASTAUD_Thread, "DreamcastAudioThread", _this);
    // if (!_this->thread) {
    //     SDL_AtomicSet(&_this->enabled, 0);
    //     SDL_free(hidden);
    //     snd_stream_shutdown();
    //     return SDL_SetError("Failed to create audio thread");
    // }

    // Store the KOS sound stream handle in SDL's userdata field - so can be used to call sound stream api functions directly for volume control etc.
    _this->spec.userdata = (void *)(intptr_t)hidden->stream_handle;
    hidden->buffer_ready = 0; 
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
            snd_stream_stop(hidden->stream_handle);
            snd_stream_destroy(hidden->stream_handle);
        }
        if (hidden->mixbuf[0]) SDL_free(hidden->mixbuf[0]);
        if (hidden->mixbuf[1]) SDL_free(hidden->mixbuf[1]);
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
    impl->PlayDevice = DREAMCASTAUD_PlayDevice;
    impl->WaitDevice = DREAMCASTAUD_WaitDevice;
    impl->GetDeviceBuf = DREAMCASTAUD_GetDeviceBuf;
    impl->CloseDevice = DREAMCASTAUD_CloseDevice;
    impl->ThreadInit = DREAMCASTAUD_ThreadInit;    
    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;    

    return SDL_TRUE;
}

AudioBootStrap DREAMCASTAUD_bootstrap = {
    "dcstreamingaudio", "SDL Dreamcast Streaming Audio Driver",
    DREAMCASTAUD_Init, SDL_FALSE
};

#endif /* SDL_AUDIO_DRIVER_DREAMCAST */
