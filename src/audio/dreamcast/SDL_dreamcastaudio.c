#include "../../SDL_internal.h"

#ifdef SDL_AUDIO_DRIVER_DREAMCAST

#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "SDL_dreamcastaudio.h" // Defines SDL_PrivateAudioData struct
#include <dc/sound/stream.h> // Include KOS sound stream library
#include <kos/thread.h>
#include "../../SDL_internal.h"
#include "../SDL_sysaudio.h"
#include "kos.h"

static SDL_AudioDevice *audioDevice = NULL;   // Pointer to the active audio output device
static SDL_AudioDevice *captureDevice = NULL; // Pointer to the active audio capture device

static void DREAMCASTAUD_WaitDevices(_THIS);

// // Function to get the device buffer
static Uint8 *DREAMCASTAUD_GetDeviceBuf(_THIS) {
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)_this->hidden;

    // Check if the buffer is valid
    if (hidden && hidden->buffer) {
        return hidden->buffer;
    }

    return NULL;
}

static void *stream_callback(snd_stream_hnd_t hnd, int req, int *done) {
    SDL_AudioDevice *device = (SDL_AudioDevice *)audioDevice;
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)device->hidden;
    void *buffer = DREAMCASTAUD_GetDeviceBuf(device);
    int buffer_size = device->spec.size;
    int available_data = buffer_size - hidden->buffer_offset;
    int bytes_to_copy = SDL_min(req, available_data);

    SDL_Log("stream_callback called with %d samples requested", req);
    SDL_Log("Buffer size: %ld bytes, Write offset: %d bytes", (Uint32)device->spec.size, hidden->buffer_offset);

    if (!buffer) {
        SDL_Log("Buffer is NULL.");
        *done = 0;
        return NULL;
    }



    if (available_data >= req) {
        // Copy available data to buffer
        memcpy(buffer, hidden->buffer + hidden->buffer_offset, bytes_to_copy);
        hidden->buffer_offset += bytes_to_copy;
    } else {
        // Handle buffer wrap-around
        memcpy(buffer, hidden->buffer + hidden->buffer_offset, available_data);
        SDL_memset((Uint8 *)buffer + available_data, device->spec.silence, req - available_data);
        hidden->buffer_offset = req - available_data; // Wrap around and update offset
    }

    if (hidden->buffer_offset >= buffer_size) {
        hidden->buffer_offset = 0; // Ensure offset wraps around
    }

    *done = req;
    return buffer;
}



// Open the audio device
static int DREAMCASTAUD_OpenDevice(_THIS, const char *devname) {
    SDL_PrivateAudioData *hidden;
    SDL_AudioFormat test_format;
    SDL_bool iscapture = SDL_FALSE;

    SDL_assert((audioDevice == NULL) || iscapture);

    if (iscapture) {
        captureDevice = _this;
    } else {
        audioDevice = _this;
    }

    hidden = (SDL_PrivateAudioData *)SDL_malloc(sizeof(*hidden));
    if (!hidden) {
        return SDL_OutOfMemory();
    }
    SDL_zerop(hidden);
    _this->hidden = (struct SDL_PrivateAudioData *)hidden;

    // Initialize the sound stream system
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
    hidden->buffer_offset = 0; // Initialize buffer offset
    snd_stream_queue_enable(hidden->stream_handle);
    SDL_Log("Dreamcast audio driver initialized: %d\n", hidden->stream_handle);

    return 0;
}


// Start playback of the audio device
static void DREAMCASTAUD_PlayDevice(_THIS) {
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)_this->hidden;
    int channels = _this->spec.channels;
    int frequency = _this->spec.freq;

    if (hidden->stream_handle == SND_STREAM_INVALID) {
        SDL_SetError("Invalid stream handle");
        return;
    }

    if (!hidden->playing) {
        
        SDL_Log("Format: %d, Channels: %d, Frequency: %d", _this->spec.format, channels, frequency);
        snd_stream_reinit(hidden->stream_handle, stream_callback);
        snd_stream_volume(hidden->stream_handle, 255); // Max volume

        // Check the audio format and initialize the stream accordingly
        if (_this->spec.format == AUDIO_S16LSB) {
            // Initialize stream for 16-bit PCM
            snd_stream_start(hidden->stream_handle, frequency, (channels == 2) ? 1 : 0);
        } else if (_this->spec.format == AUDIO_S8) {
            // Initialize stream for 8-bit PCM
            snd_stream_start(hidden->stream_handle, frequency, (channels == 2) ? 1 : 0);
        } else {
            SDL_SetError("Unsupported audio format: %d", _this->spec.format);
            return;
        }

        // Start the audio stream
        snd_stream_queue_go(hidden->stream_handle);

        hidden->playing = SDL_TRUE;
    } else {
            SDL_Log("DREAMCASTAUD_PlayDevice called");
            // while (snd_stream_poll(hidden->stream_handle) < 0) {
            //     thd_pass();
            // }

    }
}

// Wait for the audio devices
static void DREAMCASTAUD_WaitDevices(_THIS) {
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)_this->hidden;
    if (hidden->playing) {
        SDL_Log("DREAMCASTAUD_WaitDevices called");
        while (snd_stream_poll(hidden->stream_handle) < 0) {
            thd_sleep(50);
        }
		
    } 
//     else {
//         thd_pass(); // This will make the thread yield until audio is playing
//     }
}


// Close the audio device
static void DREAMCASTAUD_CloseDevice(_THIS) {
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)_this->hidden;

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
        SDL_assert(captureDevice == _this);
        captureDevice = NULL;
    } else {
        SDL_assert(audioDevice == _this);
        audioDevice = NULL;
    }

    snd_stream_shutdown();
}

// Initialize the audio driver
static void DREAMCASTAUD_ThreadInit(_THIS) {
    kthread_t *thid = thd_get_current(); // Get the current thread ID
    thd_set_prio(thid, PRIO_DEFAULT + 1); // Set a higher priority
    SDL_Log("Audio thread priority set to higher.");
}

// Initialize the SDL2 Dreamcast audio driver
static SDL_bool DREAMCASTAUD_Init(SDL_AudioDriverImpl *impl) {
    /* Set the function pointers */
    impl->ThreadInit = DREAMCASTAUD_ThreadInit;
    impl->ThreadDeinit = NULL; // Implement if needed
    impl->OpenDevice = DREAMCASTAUD_OpenDevice;
    impl->PlayDevice = DREAMCASTAUD_PlayDevice;
    impl->WaitDevice = DREAMCASTAUD_WaitDevices;
    impl->CloseDevice = DREAMCASTAUD_CloseDevice;
    impl->GetDeviceBuf = DREAMCASTAUD_GetDeviceBuf;
    impl->ProvidesOwnCallbackThread = SDL_FALSE;
    SDL_Log("Dreamcast SDL2 audio driver initialized.");
    return SDL_TRUE;
}

AudioBootStrap DREAMCASTAUD_bootstrap = {
    "dcaudio", "SDL2 Dreamcast audio driver", DREAMCASTAUD_Init, SDL_FALSE
};

#endif // SDL_AUDIO_DRIVER_DREAMCAST
