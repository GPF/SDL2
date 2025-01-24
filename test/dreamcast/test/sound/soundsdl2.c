#include "SDL_config.h"
#include <stdio.h>
#include <stdlib.h>
#include "SDL2/SDL.h"

#ifdef DREAMCAST
#include "kos.h"
#include "SDL_hints.h"
#define WAV_PATH "/rd/StarWars60.wav"
extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);
#else
#define WAV_PATH "data/sample.wav"
#endif

static struct {
    SDL_AudioSpec spec;
    Uint8 *sound;    /* Pointer to wave data */
    Uint32 soundlen; /* Length of wave data */
    Uint32 soundpos; /* Current play position */
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
    SDL_Log("Attempting to open audio device with spec:");
    SDL_Log("  Frequency: %d", wave.spec.freq);
    SDL_Log("  Format: %d", wave.spec.format);
    SDL_Log("  Channels: %d", wave.spec.channels);
    SDL_Log("  Samples: %d", wave.spec.samples);

    device = SDL_OpenAudioDevice(NULL, SDL_FALSE, &wave.spec, NULL, 0);
    if (!device) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open audio: %s\n", SDL_GetError());
        SDL_FreeWAV(wave.sound);
        quit(2);
    }

 // Double-check if it's really unpaused
    SDL_PauseAudioDevice(device, 0);
    if (SDL_GetAudioDeviceStatus(device) != SDL_AUDIO_PLAYING) {
        SDL_Log("Audio device did not start playing! %s", SDL_GetError());
    }
    printf("SDL_OpenAudioDevice successful");

}

#ifndef __EMSCRIPTEN__
static void reopen_audio(void) {
    close_audio();
    open_audio();
}
#endif
// void SDL_DC_SetSoundBuffer(Uint8 **buffer_ptr, int *available_size);
void SDLCALL fillerup(void *userdata, Uint8 *stream, int len) {
    // Fill the stream directly
    // userdata can contain any necessary state information
    // SDL_Log("fillerup\n");
    Uint8 *waveptr = wave.sound + wave.soundpos;
    int waveleft = wave.soundlen - wave.soundpos;

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
}

static int done = 0;

#ifdef __EMSCRIPTEN__
void loop(void) {
    if (done || (SDL_GetAudioDeviceStatus(device) != SDL_AUDIO_PLAYING)) {
        emscripten_cancel_main_loop();
    }
}
#endif

int main(int argc, char *argv[]) {
    char *filename = WAV_PATH;
    SDL_Window *window;
    SDL_Surface *image_surface;
    SDL_Texture *texture;
    SDL_Renderer *renderer;
    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
    // SDL_SetHint(SDL_HINT_AUDIO_DIRECT_BUFFER_ACCESS_DC, "0");
    /* Load the SDL library */
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL_CreateWindow\n"); 
    // Create a window
    window = SDL_CreateWindow("SDL2 Displaying Image", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_SHOWN);
    if (!window) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;  
    } 
     // Set SDL hint for the renderer
    // SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "software");
    printf("SDL_CreateRenderer\n"); 
    // Create a renderer
    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) { 
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit(); 
        return 1;  
    } 

    /* Load the wave file into memory */
    if (SDL_LoadWAV(filename, &wave.spec, &wave.sound, &wave.soundlen) == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load %s: %s\n", filename, SDL_GetError());
        quit(1);
    }
    // wave.spec.freq= 44100;
    // wave.spec.format    = AUDIO_S16LSB;
    // wave.spec.channels = 1;
    // wave.spec.samples = 4096;
    wave.spec.callback = fillerup;

    /* Show the list of available drivers */
    SDL_Log("Available audio drivers:");
    for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i) {
        SDL_Log("%i: %s", i, SDL_GetAudioDriver(i)); 
    }

    SDL_Log("Using audio driver: %s\n", SDL_GetCurrentAudioDriver());

    open_audio();

    SDL_FlushEvents(SDL_AUDIODEVICEADDED, SDL_AUDIODEVICEREMOVED);
       
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        SDL_Event event;

        while (SDL_PollEvent(&event) > 0) {
            if (event.type == SDL_QUIT) {
                done = 1;
            }
            if ((event.type == SDL_AUDIODEVICEADDED && !event.adevice.iscapture) ||
                (event.type == SDL_AUDIODEVICEREMOVED && !event.adevice.iscapture && event.adevice.which == device)) {
                reopen_audio();
            }
        }
        SDL_Delay(100);
    }
#endif

    /* Clean up on signal */
    close_audio();
    SDL_FreeWAV(wave.sound);
    SDL_Quit();
    return 0;
}
