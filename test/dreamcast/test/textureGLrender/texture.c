    #include <SDL2/SDL.h>
    #include <GL/gl.h>
    #include <math.h>
    #include <stdio.h>

    #define FPS 60
    #define WINDOW_WIDTH 640
    #define WINDOW_HEIGHT 480

    const Uint32 waittime = 1000 / FPS;

    float xrot = 0.0f, yrot = 0.0f, zrot = 0.0f;
    const float ROTATION_SPEED = 0.1f;

    SDL_Texture* texture = NULL;

SDL_Texture* LoadTexture(SDL_Renderer* renderer, const char* filename) {
    Uint8 *rowhi, *rowlo;
    Uint8 *tmpbuf, tmpch;    
    SDL_Surface* image = SDL_LoadBMP(filename);
    if (!image) {
        fprintf(stderr, "Unable to load %s: %s\n", filename, SDL_GetError());
        return NULL;
    }
    tmpbuf = (Uint8 *)malloc(image->pitch);
    if (!tmpbuf) {
        fprintf(stderr, "Out of memory\n");
        return NULL;
    }
    rowhi = (Uint8 *)image->pixels;
    rowlo = rowhi + (image->h * image->pitch) - image->pitch;
    for (int i = 0; i < image->h / 2; ++i) {
        for (int j = 0; j < image->w; ++j) { 
            tmpch = rowhi[j * 3];
            rowhi[j * 3] = rowhi[j * 3 + 2];
            rowhi[j * 3 + 2] = tmpch;
            tmpch = rowlo[j * 3];
            rowlo[j * 3] = rowlo[j * 3 + 2];
            rowlo[j * 3 + 2] = tmpch;
        }
        memcpy(tmpbuf, rowhi, image->pitch);
        memcpy(rowhi, rowlo, image->pitch);
        memcpy(rowlo, tmpbuf, image->pitch);
        rowhi += image->pitch;
        rowlo -= image->pitch;
    }
    free(tmpbuf);

    // Set transparent pixel as the pixel at (0,0) for BMPs with no alpha channel
    if (image->format->BitsPerPixel == 24 || image->format->BitsPerPixel == 32) {
        Uint32 transparentColor = *(Uint32 *)image->pixels;
        SDL_SetColorKey(image, SDL_TRUE, transparentColor);
    }

    // Convert the surface to a format compatible with the renderer
    SDL_Surface* converted_surface = SDL_ConvertSurfaceFormat(image, SDL_PIXELFORMAT_ARGB1555, 0);
    if (!converted_surface) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to convert surface: %s\n", SDL_GetError());
        SDL_FreeSurface(image);
        return NULL; 
    }

    // Create texture from the converted surface
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, converted_surface);
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to create texture: %s\n", SDL_GetError());
    }

    SDL_FreeSurface(image);          // Free the original surface
    SDL_FreeSurface(converted_surface); // Free the converted surface
    return texture;
}



void DrawScene(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_RenderClear(renderer);

    // Calculate the center of the screen
    int centerX = WINDOW_WIDTH / 2;
    int centerY = WINDOW_HEIGHT / 2;

    // Define the center of the texture
    int textureWidth = 256; // Width of your texture
    int textureHeight = 256; // Height of your texture

    // Define the corners of the quad
    SDL_Point corners[4] = {
        {-textureWidth / 2, -textureHeight / 2}, // Top-left corner
        {textureWidth / 2, -textureHeight / 2}, // Top-right corner
        {textureWidth / 2, textureHeight / 2}, // Bottom-right corner
        {-textureWidth / 2, textureHeight / 2} // Bottom-left corner
    };

    // Apply rotation around the center
    for (int i = 0; i < 4; i++) {
        float x = corners[i].x;
        float y = corners[i].y;

        // Rotate around Z axis
        corners[i].x = (int)(x * cos(zrot) - y * sin(zrot));
        corners[i].y = (int)(x * sin(zrot) + y * cos(zrot));
    }

    // Calculate the destination rectangle's position (centered)
    SDL_Rect dstRect = {
        centerX - textureWidth / 2,  // Center the rectangle on the screen
        centerY - textureHeight / 2,  // Center the rectangle on the screen
        textureWidth,                  // Texture width
        textureHeight                  // Texture height
    };

    // Draw the rotated quad with vertical flip
    SDL_RenderCopyEx(renderer, texture, NULL, &dstRect, zrot * (180.0 / M_PI), NULL, SDL_FLIP_VERTICAL);

    SDL_RenderPresent(renderer);
}


    void HandleJoystickInput(SDL_Joystick* joystick) {
        Uint8 hat = SDL_JoystickGetHat(joystick, 0);

        if (hat & SDL_HAT_UP) xrot += ROTATION_SPEED;
        if (hat & SDL_HAT_DOWN) xrot -= ROTATION_SPEED;
        if (hat & SDL_HAT_LEFT) yrot -= ROTATION_SPEED;
        if (hat & SDL_HAT_RIGHT) yrot += ROTATION_SPEED;

        // Add Z-axis rotation with joystick buttons
        if (SDL_JoystickGetButton(joystick, 0)) zrot += ROTATION_SPEED;
        if (SDL_JoystickGetButton(joystick, 1)) zrot -= ROTATION_SPEED;
    }

    int main(int argc, char** argv) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL could not initialize! SDL Error: %s\n", SDL_GetError());
            return 1;
        }

        SDL_Window* window = SDL_CreateWindow("SDL2 Texture Demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN|SDL_VIDEO_OPENGL);
        if (!window) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window could not be created! SDL Error: %s\n", SDL_GetError());
            return 1;
        }

        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Renderer could not be created! SDL Error: %s\n", SDL_GetError());
            return 1;
        }
    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer, &info) == 0) {
        SDL_Log("Renderer Name: %s", info.name);
        SDL_Log("Texture Formats: ");
        for (int i = 0; i < info.num_texture_formats; i++) {
            SDL_Log("  %s", SDL_GetPixelFormatName(info.texture_formats[i]));
        }
    }
        texture = LoadTexture(renderer, "/rd/Troy2024.bmp");
        if (!texture) {
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        SDL_Event event;
        int running = 1;

        SDL_Joystick* joystick = SDL_JoystickOpen(0);
        if (!joystick) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to open joystick: %s\n", SDL_GetError());
        }

        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = 0;
                }
            }

            if (joystick) {
                HandleJoystickInput(joystick);
            }

            DrawScene(renderer);

            SDL_Delay(waittime); // Wait for the remaining frame time
        }

        if (joystick) {
            SDL_JoystickClose(joystick);
        }
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }
