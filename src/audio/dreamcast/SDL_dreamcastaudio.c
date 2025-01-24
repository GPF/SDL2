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

// // Stream callback function
// static void *stream_callback(snd_stream_hnd_t hnd, int req, int *done) {
//     SDL_AudioDevice *device = audioDevice;
//     SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)device->hidden;
//     Uint8 *buffer = (Uint8 *)DREAMCASTAUD_GetDeviceBuf(device);
//     int buffer_size = hidden->buffer_size;
//     int bytes_to_copy = SDL_min(req, buffer_size);
//     int remaining = bytes_to_copy;
//     SDL_AudioCallback callback;

//     SDL_LockMutex(device->mixer_lock);

//     if (!buffer) {
//         SDL_Log("Buffer is NULL.");
//         *done = 0;
//         SDL_UnlockMutex(device->mixer_lock);
//         return NULL;
//     }

//     callback = device->callbackspec.callback;

//     // Handle paused or disabled audio
//     if (!SDL_AtomicGet(&device->enabled) || SDL_AtomicGet(&device->paused)) {
//         if (device->stream) {
//             SDL_AudioStreamClear(device->stream);
//         }
//         SDL_memset(buffer, device->callbackspec.silence, bytes_to_copy);
//     } else 
//     // {
//     //     if (hidden->direct_buffer_access) {
//     //         Uint8 *writable_buffer = NULL;
//     //         int writable_size = 0;

//     //         SDL_DC_SetSoundBuffer(&writable_buffer, &writable_size);
//     //         int bytes_to_copy = SDL_min(req, writable_size);

//     //         // Call the client callback directly to fill the buffer
//     //         device->callbackspec.callback(device->callbackspec.userdata, writable_buffer, bytes_to_copy);
//     //         hidden->write_index = (hidden->write_index + bytes_to_copy) % hidden->buffer_size;

            
//     //     }
//     //      else
//           {
//             // Fall back to the existing behavior with memcpy
//             if (!device->stream) {
//                 // callback(device->callbackspec.userdata, buffer, bytes_to_copy);
//             } else {
//                 while (SDL_AudioStreamAvailable(device->stream) < bytes_to_copy) {
//                     // callback(device->callbackspec.userdata, hidden->buffer, buffer_size);
//                     if (SDL_AudioStreamPut(device->stream, buffer, buffer_size) == -1) {
//                         SDL_AudioStreamClear(device->stream);
//                         SDL_AtomicSet(&device->enabled, 0);
//                         break;
//                     }
//                 }

//                 int got = SDL_AudioStreamGet(device->stream, buffer, bytes_to_copy);
//                 if (got != bytes_to_copy) {
//                     SDL_memset(buffer, device->callbackspec.silence, bytes_to_copy);
//                 }
//             }
//         }
    

//     SDL_UnlockMutex(device->mixer_lock);

//     *done = req;
//     return buffer;
// }

// Stream callback function
static void *stream_callback(snd_stream_hnd_t hnd, int req, int *done) {
    SDL_AudioDevice *device = audioDevice;
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)device->hidden;
    int buffer_size = hidden->buffer_size;
    int bytes_to_copy = SDL_min(req, buffer_size);
    // SDL_Log("stream_callback");
    SDL_LockMutex(device->mixer_lock);
    // Obtain the buffer from SDL's device buffer
    Uint8 *buffer = (Uint8 *)DREAMCASTAUD_GetDeviceBuf(device);
    SDL_UnlockMutex(device->mixer_lock);
    *done = bytes_to_copy;
    return buffer;
    
}


// Thread function for audio playback
// static int SDLCALL DREAMCASTAUD_Thread(void *data) {
//     SDL_AudioDevice *device = (SDL_AudioDevice *)data;
//     SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)device->hidden;

//     SDL_Log("Audio thread started\n");

//     while (SDL_AtomicGet(&device->enabled)) {
//         // Call snd_stream_poll to check if more audio data is needed
//         SDL_Log("snd_stream_poll");
//         int result = snd_stream_poll(hidden->stream_handle);
//         if (result < 0) {
//             SDL_Log("snd_stream_poll failed: %d\n", result);
//             SDL_AtomicSet(&device->enabled, 0);
//             break;
//         }
//         SDL_Delay(10); // Avoid busy looping
//     }

//     SDL_Log("Audio thread exiting\n");

//     return 0;
// }

static void DREAMCASTAUD_WaitDevice(_THIS) {
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)_this->hidden;

    if (SDL_AtomicGet(&_this->paused)) {
        // SDL_Log("Audio is not enabled or is paused, skipping buffer switch.");
        return;
    }
    // Get the current buffer pointer
    Uint8 *current_buffer = DREAMCASTAUD_GetDeviceBuf(_this);
    
    // Wait until the position moves to the next buffer
    while (snd_get_pos(hidden->stream_handle) /  (_this->spec.samples )  == (current_buffer == hidden->mixbuf[0] ? 0 : 1)) {
        snd_stream_poll(hidden->stream_handle);
        SDL_Delay(5);
        // SDL_Log("DREAMCASTAUD_WaitDevice - waiting for buffer switch %d",snd_get_pos(hidden->stream_handle));
    }
}
static void DREAMCASTAUD_PlayDevice(_THIS) {
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)_this->hidden;
    
    // Check if audio is enabled and not paused
    if (SDL_AtomicGet(&_this->paused)) {
        // SDL_Log("Audio is not enabled or is paused, skipping buffer switch.");
        return;
    }

    // SDL_Log("DREAMCASTAUD_PlayDevice");
    DREAMCASTAUD_WaitDevice(_this);  // Wait for the current buffer to be played

    // Switch buffer for the next fill by SDL2
    hidden->buffer ^= 1;  // Toggle between 0 and 1 for buffer index
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

    // Read the hint for direct buffer access
    hint_value = SDL_GetHint(SDL_HINT_AUDIO_DIRECT_BUFFER_ACCESS_DC);
    if (hint_value && SDL_strcasecmp(hint_value, "1") == 0) {
        hidden->direct_buffer_access = SDL_TRUE;
        SDL_Log("Direct buffer access enabled for Dreamcast audio driver.");
    } else {
        hidden->direct_buffer_access = SDL_FALSE;
        SDL_Log("Direct buffer access disabled for Dreamcast audio driver.");
    }

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
    snd_stream_volume(hidden->stream_handle, 255); // Max volume

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
