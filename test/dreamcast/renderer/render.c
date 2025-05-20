#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "opengl");
    SDL_Window *win = SDL_CreateWindow("SDL3 Render Test", 640, 480, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
    SDL_Renderer *renderer = SDL_CreateRenderer(win, "opengl");
    if (!renderer) {
        SDL_Log("Renderer failed: %s", SDL_GetError());
    } else {
const char *name = SDL_GetRendererName(renderer);
if (name) {
    SDL_Log("Renderer selected: %s", name);
}
    }
    SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB1555, SDL_TEXTUREACCESS_STATIC, 256, 256);

    uint16_t *pixels = malloc(256 * 256 * sizeof(uint16_t));
    if (!pixels) {
        printf("Failed to allocate pixel buffer\n");
        return 1;
    }
    uint16_t color = 0xCCCC; 
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            pixels[y * 256 + x] = color;
        }
    }

    SDL_UpdateTexture(tex, NULL, pixels, 256 * sizeof(uint16_t));

    // Destination rect centered at 640x480
    SDL_FRect dst = {
        .x = (640 - 256) / 2,
        .y = (480 - 256) / 2,
        .w = 256,
        .h = 256
    };

    int running = 1;
    while (running) {
        // printf("Running...\n");
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = 0;
        }

        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, tex, NULL, &dst);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    free(pixels);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
