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
#include "SDL_dreamcastaudio.h"
#include "aica.h"
#include <dc/spu.h>
#include <kos/thread.h>

#include "../../SDL_internal.h"
#include "../SDL_sysaudio.h"
#include <stdint.h>

static SDL_AudioDevice *audioDevice = NULL;   // Pointer to the active audio output device
static SDL_AudioDevice *captureDevice = NULL; // Pointer to the active audio capture device
static __inline__ void SDL_DC_spu_memload_mono(uint32 dst, uint32 *__restrict__ src, size_t size);
static __inline__ void SDL_DC_spu_memload_stereo8(int leftpos, int rightpos, void* __restrict__ src0, size_t size);
static __inline__ void SDL_DC_spu_memload_stereo16(int leftpos, int rightpos, void* __restrict__ src0, size_t size);

static int DREAMCASTAUD_OpenDevice(_THIS, const char *devname)
{
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

    // Find a compatible format
    for (test_format = SDL_FirstAudioFormat(_this->spec.format); test_format; test_format = SDL_NextAudioFormat()) {
        if ((test_format == AUDIO_S8) ||
            (test_format == AUDIO_S16LSB)) {
            _this->spec.format = test_format;
            break;
        }
    }

    if (!test_format) {
        /* Didn't find a compatible format :( */
        printf("Dreamcast unsupported audio format: 0x%x", _this->spec.format);
        return 0;
    }

    SDL_CalculateAudioSpec(&_this->spec);

    // Allocate and initialize mixing buffer
    hidden->mixlen = _this->spec.size;
    hidden->mixbuf = (Uint8 *)SDL_malloc(hidden->mixlen);
    if (!hidden->mixbuf) {
        SDL_free(hidden);
        return SDL_OutOfMemory();
    }
    SDL_memset(hidden->mixbuf, _this->spec.silence, _this->spec.size);
    hidden->leftpos = (uint32_t *)0x11000;  // Adjust type if needed
    hidden->rightpos = (uint32_t *)(0x11000 + _this->spec.size);  // Adjust type if needed
    hidden->playing = 0;
    hidden->nextbuf = 0;

    // Initialize the sound device
    irq_enable();

    SDL_LogCritical(SDL_LOG_CATEGORY_AUDIO,
                    "Dreamcast audio driver initialized\n");

    return 0;
}

static void DREAMCASTAUD_WaitDevices(_THIS)
{
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)_this->hidden;
    
    if (hidden->playing) {
        int current_pos = SDL_DC_aica_get_pos(0) / _this->spec.samples;
        printf("Waiting in DREAMCASTAUD_WaitDevices. Current position: %d, Next buffer: %d\n",
               current_pos, hidden->nextbuf);
        while (current_pos == hidden->nextbuf) {
            thd_pass();
            current_pos = SDL_DC_aica_get_pos(0) / _this->spec.samples;
            // printf("Still waiting... Current position: %d, Next buffer: %d\n",
                //    current_pos, hidden->nextbuf);
        }
        printf("Done waiting. Buffer is ready.\n");
    } else {
        printf("No playback to wait for.\n");
    }
}

static void DREAMCASTAUD_PlayDevice(_THIS)
{
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)_this->hidden;
    SDL_AudioSpec *spec = &_this->spec;
    unsigned int offset;
    
    // Log the function entry and current playing state
    printf("DREAMCASTAUD_PlayDevice called. Playing: %d\n", hidden->playing);

    // Wait until it's possible to write
    if (hidden->playing) {
        // printf("Waiting for buffer to be ready. Next buffer: %d\n", hidden->nextbuf);
        while (SDL_DC_aica_get_pos(0) / spec->samples == hidden->nextbuf) {
            thd_pass();
        }
        // printf("Buffer is ready. Proceeding to load data.\n");
    } else {
        printf("Audio is not currently playing. Loading data directly.\n");
    }

    offset = hidden->nextbuf * spec->size;
    hidden->nextbuf ^= 1;

    // Log buffer offset and size
    // printf("Buffer offset: %u, Buffer size: %u\n", offset, spec->size);

    // Load the audio buffer
    if (spec->channels == 1) {
        printf("Loading mono audio data.\n");
        SDL_DC_spu_memload_mono((uint32)(hidden->leftpos + offset), (uint32 *)hidden->mixbuf, spec->size);
    } else {
        offset >>= 1;
        if (spec->format == AUDIO_S8) {
            printf("Loading 8-bit stereo audio data.\n");
            SDL_DC_spu_memload_stereo8((uint32)(hidden->leftpos + offset), (uint32 )(hidden->rightpos + offset), (uint32 *)hidden->mixbuf, spec->size);
        } else {
            printf("Loading 16-bit stereo audio data.\n");
            SDL_DC_spu_memload_stereo16((uint32)(hidden->leftpos + offset), (uint32)(hidden->rightpos + offset), (uint32 *)hidden->mixbuf, spec->size);
        }
    }

    // Start playback if not already playing
    if (!hidden->playing) {
        int mode = (spec->format == AUDIO_S8) ? SDL_DC_SM_8BIT : SDL_DC_SM_16BIT;
        hidden->playing = 1;

        printf("Starting playback. Channels: %d, Mode: %d\n", spec->channels, mode);
        
        if (spec->channels == 1) {
            printf("Playing mono audio.\n");
            SDL_DC_aica_play(0, mode, (unsigned long)hidden->leftpos, 0, spec->samples << 1, spec->freq, 255, 128, 1);
        } else {
            printf("Playing stereo audio.\n");
            SDL_DC_aica_play(0, mode, (unsigned long)hidden->leftpos, 0, spec->samples << 1, spec->freq, 255, 0, 1);
            SDL_DC_aica_play(1, mode, (unsigned long)hidden->rightpos, 0, spec->samples << 1, spec->freq, 255, 255, 1);
        }
    }
}

static Uint8* DREAMCASTAUD_GetDeviceBuf(_THIS)
{
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)_this->hidden;
    return hidden->mixbuf;
}

// static int DREAMCASTAUD_CaptureFromDevice(_THIS, void *buffer, int buflen)
// {
//     /* Implement capture functionality if needed */
//     return 0;  /* Example return value */
// }

// static void DREAMCASTAUD_FlushCapture(_THIS)
// {
//     /* Implement capture flush functionality if needed */
// }

static void DREAMCASTAUD_CloseDevice(_THIS)
{
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *)_this->hidden;

    /* Clean up and close the device */
    spu_shutdown();

    if (hidden) {
        if (hidden->mixbuf) {
            SDL_free(hidden->mixbuf);
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
}



static SDL_bool DREAMCASTAUD_Init(SDL_AudioDriverImpl *impl)
{
    /* Set the function pointers */
    impl->OpenDevice = DREAMCASTAUD_OpenDevice;
    impl->WaitDevice = DREAMCASTAUD_WaitDevices; 
    impl->PlayDevice = DREAMCASTAUD_PlayDevice;
    impl->GetDeviceBuf = DREAMCASTAUD_GetDeviceBuf;
    // impl->CaptureFromDevice = DREAMCASTAUD_CaptureFromDevice;
    // impl->FlushCapture = DREAMCASTAUD_FlushCapture;
    impl->CloseDevice = DREAMCASTAUD_CloseDevice;
    impl->DetectDevices = NULL;  /* Assuming no device detection, set to NULL if not needed */
    impl->AllowsArbitraryDeviceNames = SDL_TRUE;

    /* Set capabilities */
    impl->HasCaptureSupport = SDL_FALSE;  /* Assuming no capture support */
    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;
    impl->OnlyHasDefaultCaptureDevice = SDL_FALSE;

    /* Initialize the sound processing unit */
    spu_init();
    
    return SDL_TRUE;  /* This audio target is available */
}

AudioBootStrap DREAMCASTAUD_bootstrap = {
    "dcaudio", "SDL2 Dreamcast audio driver", DREAMCASTAUD_Init, SDL_FALSE
};


// void DREAMCASTAUD_ResumeDevices(void)
// {
//     /* Handle resuming audio devices */
//     if (audioDevice && audioDevice->hidden) {
//         SDL_UnlockMutex(audioDevice->mixer_lock);
//     }

//     if (captureDevice && captureDevice->hidden) {
//         SDL_UnlockMutex(captureDevice->mixer_lock);
//     }
// }

static __inline__ void SDL_DC_spu_memload_mono(uint32 dst, uint32 *__restrict__ src, size_t size)
{
	register uint32 *dat  = (uint32*)(dst + SDL_DC_SPU_RAM_BASE);

	unsigned old1,old2;
	SDL_DC_G2_LOCK(old1, old2);
	size >>= 5;
	while(size--) {
		SDL_DC_G2_WRITE_32(dat++,*src++);
		SDL_DC_G2_WRITE_32(dat++,*src++);
		SDL_DC_G2_WRITE_32(dat++,*src++);
		SDL_DC_G2_WRITE_32(dat++,*src++);
		SDL_DC_G2_WRITE_32(dat++,*src++);
		SDL_DC_G2_WRITE_32(dat++,*src++);
		SDL_DC_G2_WRITE_32(dat++,*src++);
		SDL_DC_G2_WRITE_32(dat++,*src++);
	}
	SDL_DC_G2_UNLOCK(old1, old2);
	SDL_DC_G2_FIFO_WAIT();
}


static __inline__ void SDL_DC_spu_memload_stereo8(int leftpos, int rightpos, void* __restrict__ src0, size_t size)
{
	uint8 *src = src0;
	uint32 *left  = (uint32*)(leftpos + SDL_DC_SPU_RAM_BASE);
	uint32 *right = (uint32*)(rightpos+ SDL_DC_SPU_RAM_BASE);
	unsigned old1,old2;
	SDL_DC_G2_LOCK(old1, old2);
	size >>= 5;
	while(size--) {
		register unsigned lval,rval;

		lval = *src++; rval = *src++;
		lval|= (*src++)<<8; rval|= (*src++)<<8;
		lval|= (*src++)<<16; rval|= (*src++)<<16;
		lval|= (*src++)<<24; rval|= (*src++)<<24;
		SDL_DC_G2_WRITE_32(left++,lval);
		SDL_DC_G2_WRITE_32(right++,rval);

		lval = *src++; rval = *src++;
		lval|= (*src++)<<8; rval|= (*src++)<<8;
		lval|= (*src++)<<16; rval|= (*src++)<<16;
		lval|= (*src++)<<24; rval|= (*src++)<<24;
		SDL_DC_G2_WRITE_32(left++,lval);
		SDL_DC_G2_WRITE_32(right++,rval);

		lval = *src++; rval = *src++;
		lval|= (*src++)<<8; rval|= (*src++)<<8;
		lval|= (*src++)<<16; rval|= (*src++)<<16;
		lval|= (*src++)<<24; rval|= (*src++)<<24;
		SDL_DC_G2_WRITE_32(left++,lval);
		SDL_DC_G2_WRITE_32(right++,rval);

		lval = *src++; rval = *src++;
		lval|= (*src++)<<8; rval|= (*src++)<<8;
		lval|= (*src++)<<16; rval|= (*src++)<<16;
		lval|= (*src++)<<24; rval|= (*src++)<<24;
		SDL_DC_G2_WRITE_32(left++,lval);
		SDL_DC_G2_WRITE_32(right++,rval);
	}
	SDL_DC_G2_UNLOCK(old1, old2);
	SDL_DC_G2_FIFO_WAIT();
}

static __inline__ void SDL_DC_spu_memload_stereo16(int leftpos, int rightpos, void* __restrict__ src0, size_t size)
{
	uint16 *src = src0;
	uint32 *left  = (uint32*)(leftpos + SDL_DC_SPU_RAM_BASE);
	uint32 *right = (uint32*)(rightpos+ SDL_DC_SPU_RAM_BASE);
	unsigned old1,old2;
	SDL_DC_G2_LOCK(old1, old2);
	size >>= 5;
	while(size--) {
		register unsigned lval,rval;

		lval = (*src++); rval = *src++;
		lval|= (*src++)<<16; rval|= (*src++)<<16;
		SDL_DC_G2_WRITE_32(left++,lval);
		SDL_DC_G2_WRITE_32(right++,rval);

		lval = (*src++); rval = *src++;
		lval|= (*src++)<<16; rval|= (*src++)<<16;
		SDL_DC_G2_WRITE_32(left++,lval);
		SDL_DC_G2_WRITE_32(right++,rval);

		lval = (*src++); rval = *src++;
		lval|= (*src++)<<16; rval|= (*src++)<<16;
		SDL_DC_G2_WRITE_32(left++,lval);
		SDL_DC_G2_WRITE_32(right++,rval);

		lval = (*src++); rval = *src++;
		lval|= (*src++)<<16; rval|= (*src++)<<16;
		SDL_DC_G2_WRITE_32(left++,lval);
		SDL_DC_G2_WRITE_32(right++,rval);
	}
	SDL_DC_G2_UNLOCK(old1, old2);
	SDL_DC_G2_FIFO_WAIT();
}



#else /* SDL_AUDIO_DRIVER_DREAMCAST  not defined */

void DREAMCASTAUD_WaitDevices(void) {}

#endif /* SDL_AUDIO_DRIVER_DREAMCAST*/
/* vi: set ts=4 sw=4 expandtab: */
