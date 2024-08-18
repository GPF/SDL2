#include "SDL_config.h"
#include <stdio.h>
#include <stdlib.h>
#include "SDL2/SDL.h"

#define WAV_PATH "/cd/data/sample.wav"
// extern uint8 romdisk[];
// KOS_INIT_ROMDISK(romdisk);

static struct {
    SDL_AudioSpec spec;
    Uint8 *sound;    /* Pointer to wave data */
    Uint32 soundlen; /* Length of wave data */
    int soundpos;    /* Current play position */
} wave;

static SDL_AudioDeviceID device;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void quit(int rc) {
    SDL_Quit();
    exit(rc);
}

static void close_audio(void) {
    if (device != 0) {
        SDL_CloseAudioDevice(device);
        device = 0;
    }
}

static void open_audio(void) {
    device = SDL_OpenAudioDevice(NULL, SDL_FALSE, &wave.spec, NULL, 0);
    if (!device) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open audio: %s\n", SDL_GetError());
        SDL_FreeWAV(wave.sound);
        quit(2);
    }
    SDL_PauseAudioDevice(device, SDL_FALSE);
}

#ifndef __EMSCRIPTEN__
static void reopen_audio(void) {
    close_audio();
    open_audio();
}
#endif

void SDLCALL fillerup(void *unused, Uint8 *stream, int len) {
    Uint8 *waveptr;
    int waveleft;

    /* Set up the pointers */
    waveptr = wave.sound + wave.soundpos;
    waveleft = wave.soundlen - wave.soundpos;

    /* Go! */
    while (waveleft <= len) {
        SDL_memcpy(stream, waveptr, waveleft);
        stream += waveleft;
        len -= waveleft;
        waveptr = wave.sound;
        waveleft = wave.soundlen;
        wave.soundpos = 0;
    }
    SDL_memcpy(stream, waveptr, len);
    wave.soundpos += len;

    /* Log the callback call */
    // SDL_Log("fillerup called, pos: %d, len: %d", wave.soundpos, len);
}

static int done = 0;


int main(int argc, char *argv[]) {
    int i;
    char *filename = NULL;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Load the SDL library */
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    filename = WAV_PATH;

    if (!filename) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s\n", SDL_GetError());
        quit(1);
    }

    /* Load the wave file into memory */
    if (SDL_LoadWAV(filename, &wave.spec, &wave.sound, &wave.soundlen) == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load %s: %s\n", filename, SDL_GetError());
        quit(1);
    }
// Example configuration
// wave.spec.freq = 22050;    // Sample rate
// wave.spec.format = AUDIO_S16LSB;  // Audio format
// wave.spec.channels = 1;    // Stereo
// wave.spec.samples = 16384;  // Buffer size

    wave.spec.callback = fillerup;

    /* Show the list of available drivers */
    SDL_Log("Available audio drivers:");
    for (i = 0; i < SDL_GetNumAudioDrivers(); ++i) {
        SDL_Log("%i: %s", i, SDL_GetAudioDriver(i));
    }

    SDL_Log("Using audio driver: %s\n", SDL_GetCurrentAudioDriver());

    open_audio();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        SDL_Event event;

        while (SDL_PollEvent(&event) > 0) {
            if (event.type == SDL_QUIT) {
                done = 1;
            }
        }
        SDL_Delay(100);
        // SDL_Log("Main loop running");
    }
#endif

    /* Clean up on signal */
    close_audio();
    SDL_FreeWAV(wave.sound);
    SDL_free(filename);
    SDL_Quit();
    return 0;
}
