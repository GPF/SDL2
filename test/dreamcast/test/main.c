#ifdef DREAMCAST
#include <kos.h>
// Path to the BMP file on the Dreamcast's CD disk
#define BMP_PATH "/cd/data/NeHe.bmp"
#else
#define BMP_PATH "cd/data/NeHe.bmp"
#endif
#include <SDL2/SDL.h>


int main(int argc, char *argv[]) {
    SDL_Window *window;
    SDL_Surface *image_surface;
    SDL_Texture *texture;
    SDL_Renderer *renderer;
    SDL_Event event;
    int running = 1;

    printf("SDL2_INIT_VIDEO\n");
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL2 could not initialize! SDL_Error: %s\n", SDL_GetError());
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
 
    printf("SDL_CreateRenderer\n"); 
    // Create a renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    // renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) { 
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit(); 
        return 1;  
    } 

    // Load BMP file
    SDL_RWops *rw = SDL_RWFromFile(BMP_PATH, "rb");
    if (!rw) {  
        printf("Unable to open BMP file! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("SDL_RWFromFile - %s \n", BMP_PATH);

    image_surface = SDL_LoadBMP_RW(rw, 1);
    if (!image_surface) {
        printf("Unable to load BMP file! SDL_Error: %s\n", SDL_GetError());
        SDL_RWclose(rw);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("SDL_LoadBMP_RW\n"); 

    // Create texture from surface
    texture = SDL_CreateTextureFromSurface(renderer, image_surface);
    SDL_FreeSurface(image_surface); // Free the surface after creating the texture
    if (!texture) {
        printf("Unable to create texture from surface! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_RWclose(rw);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("SDL_CreateTextureFromSurface\n"); 
    // Main loop
    printf("running\n");
    while (running) { 
        while (SDL_PollEvent(&event)) { 
            if (event.type == SDL_QUIT) { 
                running = 0;
            } 
        }

        // Clear the screen 
        SDL_RenderClear(renderer);
        // Copy the texture to the renderer
        SDL_RenderCopy(renderer, texture, NULL, NULL); 
        // Present the renderer
        SDL_RenderPresent(renderer);

        SDL_UpdateWindowSurface(window);
        // Sleep for a short while to prevent excessive CPU usage
        SDL_Delay(16);  // approximately 60 FPS
    }

    // Clean up
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_RWclose(rw);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
