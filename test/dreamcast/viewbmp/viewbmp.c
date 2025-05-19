#include <kos.h>
#define BMP_PATH "/rd/Troy2024.bmp"

#include <SDL3/SDL.h>

int main(int argc, char *argv[]) {
    // Initialize SDL
//    SDL_SetHint(SDL_HINT_VIDEO_DOUBLE_BUFFER, "1");
    SDL_SetHint(SDL_HINT_DC_VIDEO_MODE, "SDL_DC_DIRECT_VIDEO");    
    // SDL_SetHint(SDL_HINT_DC_VIDEO_MODE, "SDL_DC_TEXTURED_VIDEO");  
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // Create window
    SDL_Window *window = SDL_CreateWindow("Test", 640, 480, SDL_WINDOW_FULLSCREEN);
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Load BMP image
    SDL_Log("Trying to load: %s", BMP_PATH);
    SDL_Surface *image = SDL_LoadBMP(BMP_PATH);
    if (!image) {
        SDL_Log("Failed to load BMP: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_Log("Loaded BMP successfully (%dx%d)", image->w, image->h);

    
    // Main loop
    int running = 1;
    SDL_Event e;
    
    while (running) {
        // Process events
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                running = 0;
            }
            
        }

        // Get window surface and blit image
        SDL_Surface *window_surface = SDL_GetWindowSurface(window);
        if (window_surface) {
            SDL_BlitSurface(image, NULL, window_surface, NULL);
            SDL_UpdateWindowSurface(window);
        }
        SDL_Log("Blitted image to window surface");
    }

    // Cleanup
    SDL_DestroySurface(image);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}