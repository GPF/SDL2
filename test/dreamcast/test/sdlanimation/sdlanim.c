#include <SDL2/SDL.h>
#include <stdio.h>
#include <kos.h>

/* Size of the window */
#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480
/* Size of the grass texture picture */
#define GRASS_SIZE    32

/* In the sprite, we have 8 images of a 32x32 picture,
 * 2 images for each direction. */
/* Size of one image: */
#define SPRITE_SIZE     32
/* Order of the different directions in the picture: */
#define DIR_UP          0
#define DIR_RIGHT       1
#define DIR_DOWN        2
#define DIR_LEFT        3

/* Number of pixels for one step of the sprite */
#define SPRITE_STEP     5

void HandleJoystickInput(SDL_Joystick *joystick, int *currDirection, SDL_Rect *position, int *gameover) {
    // Check the D-pad hat position
    Sint16 hat = SDL_JoystickGetHat(joystick, 0); // Assuming using the first hat

    if (hat & SDL_HAT_UP) {
        *currDirection = DIR_UP;
        position->y -= SPRITE_STEP;
    } else if (hat & SDL_HAT_DOWN) {
        *currDirection = DIR_DOWN;
        position->y += SPRITE_STEP;
    } else if (hat & SDL_HAT_LEFT) {
        *currDirection = DIR_LEFT;
        position->x -= SPRITE_STEP;
    } else if (hat & SDL_HAT_RIGHT) {
        *currDirection = DIR_RIGHT;
        position->x += SPRITE_STEP;
    }

    // Check for button press
    if (SDL_JoystickGetButton(joystick, 0)) {
        *gameover = 1;
    }
}

KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MALLOCSTATS);

int main(int argc, char* argv[])
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *spriteTexture;
    SDL_Texture *grassTexture;
    SDL_Surface *temp;

    /* Information about the current situation of the sprite: */
    int currentDirection = DIR_RIGHT;
    int animationFlip = 0;
    SDL_Rect spritePosition;

    /* initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    /* initialize joystick */
    SDL_Joystick *joystick = NULL;
    if (SDL_NumJoysticks() > 0) {
        joystick = SDL_JoystickOpen(0);
        if (!joystick) {
            SDL_Log("Failed to open joystick: %s", SDL_GetError());
            SDL_Quit();
            return 1;
        }
    } else {
        SDL_Log("No joystick connected!");
        SDL_Quit();
        return 1;
    }

    /* create window and renderer */
    window = SDL_CreateWindow("SDL Animation", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    // Set SDL hint for the renderer
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "opengl");    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* load sprite and create texture */
    temp = SDL_LoadBMP("/rd/sprite2.bmp");
    if (!temp) {
        SDL_Log("Failed to load sprite image: %s", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    // SDL_SetColorKey(temp, SDL_TRUE, SDL_MapRGB(temp->format, 255, 0, 255));
    // spriteTexture = SDL_CreateTextureFromSurface(renderer, temp);
    SDL_Surface* converted_surface1 = SDL_ConvertSurfaceFormat(temp, SDL_PIXELFORMAT_ARGB8888, 0); 
    spriteTexture = SDL_CreateTextureFromSurface(renderer, converted_surface1);
    SDL_FreeSurface(temp);

    if (!spriteTexture) {
        SDL_Log("Failed to create sprite texture: %s", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* load grass and create texture */
    temp = SDL_LoadBMP("/rd/grass.bmp");
    if (!temp) {
        SDL_Log("Failed to load grass image: %s", SDL_GetError());
        SDL_DestroyTexture(spriteTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    // grassTexture = SDL_CreateTextureFromSurface(renderer, temp);
    SDL_Surface* converted_surface = SDL_ConvertSurfaceFormat(temp, SDL_PIXELFORMAT_ARGB1555, 0);    
    grassTexture = SDL_CreateTextureFromSurface(renderer, converted_surface);
    SDL_FreeSurface(temp);

    if (!grassTexture) {
        SDL_Log("Failed to create grass texture: %s", SDL_GetError());
        SDL_DestroyTexture(spriteTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* set sprite position in the middle of the window */
    spritePosition.x = (SCREEN_WIDTH - SPRITE_SIZE) / 2;
    spritePosition.y = (SCREEN_HEIGHT - SPRITE_SIZE) / 2;

    int gameover = 0;

    /* main loop: check events and re-draw the window until the end */
    while (!gameover)
    {
        SDL_Event event;

        /* look for an event; possibly update the position and the shape
         * of the sprite. */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                gameover = 1;
            } else if (event.type == SDL_KEYDOWN) {
                // Handle keyboard events, e.g., check for escape key to quit
        if (event.key.keysym.sym == SDLK_ESCAPE) {
                    gameover = 1;
                }
            } else if (event.type == SDL_JOYBUTTONDOWN) {
                // Handle joystick button press events
        if (event.jbutton.button == 1 && event.jbutton.which == 0) {
                    // Button 1 on joystick 0 was pressed, exit the program immediately
                    SDL_Quit();
                    exit(0);
                }
                if (event.jbutton.button == 0 && event.jbutton.which == 0) {
                    // Button 0 on joystick 0 was pressed, set gameover
                    gameover = 1;
                }
            }
        }


        /* handle joystick input */
        HandleJoystickInput(joystick, &currentDirection, &spritePosition, &gameover);

        /* collide with edges of screen */
        if (spritePosition.x <= 0)
            spritePosition.x = 0;
        if (spritePosition.x >= SCREEN_WIDTH - SPRITE_SIZE) 
            spritePosition.x = SCREEN_WIDTH - SPRITE_SIZE;

        if (spritePosition.y <= 0)
            spritePosition.y = 0;
        if (spritePosition.y >= SCREEN_HEIGHT - SPRITE_SIZE) 
            spritePosition.y = SCREEN_HEIGHT - SPRITE_SIZE;

        /* clear the screen */
        SDL_RenderClear(renderer);

        /* draw the background */
        for (int x = 0; x < SCREEN_WIDTH / GRASS_SIZE; x++) {
            for (int y = 0; y < SCREEN_HEIGHT / GRASS_SIZE; y++) {
                SDL_Rect position = {x * GRASS_SIZE, y * GRASS_SIZE, GRASS_SIZE, GRASS_SIZE};
                SDL_RenderCopy(renderer, grassTexture, NULL, &position);
            }
        }
    // SDL_SetTextureBlendMode(spriteTexture, SDL_BLENDMODE_BLEND);
        /* Draw the selected image of the sprite at the right position */
        {
            SDL_Rect spriteImage;
            spriteImage.y = 0;
            spriteImage.w = SPRITE_SIZE;
            spriteImage.h = SPRITE_SIZE;
            spriteImage.x = SPRITE_SIZE * (2 * currentDirection + animationFlip);

            SDL_Rect destRect = {spritePosition.x, spritePosition.y, SPRITE_SIZE, SPRITE_SIZE};
            SDL_RenderCopy(renderer, spriteTexture, &spriteImage, &destRect);
        }
        animationFlip = !animationFlip; // Toggle animation state
        /* update the screen */
        SDL_RenderPresent(renderer);
    }

    /* clean up */
    if (joystick) {
        SDL_JoystickClose(joystick);
    }
    SDL_DestroyTexture(spriteTexture);
    SDL_DestroyTexture(grassTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
