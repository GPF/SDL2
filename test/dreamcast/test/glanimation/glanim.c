#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>

#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480
#define SPRITE_SIZE 32
#define GRASS_SIZE 32
#define DIR_UP 0
#define DIR_RIGHT 1
#define DIR_DOWN 2
#define DIR_LEFT 3
#define SPRITE_STEP 5

GLuint textures[2]; // To store both textures

void LoadGLTextures() {
    const char *filenames[] = {"/rd/grass.bmp", "/rd/sprite.bmp"};
    SDL_Surface *image;
    GLenum format;

    glGenTextures(2, textures);

    for(int i = 0; i < 2; ++i) {
        image = SDL_LoadBMP(filenames[i]);
        if (!image) {
            fprintf(stderr, "Unable to load %s: %s\n", filenames[i], SDL_GetError());
            continue;
        }

        // Determine the format of the image data
        if (image->format->BytesPerPixel == 3) { 
            format = GL_RGB;
        } else if (image->format->BytesPerPixel == 4) { 
            format = GL_RGBA;
        } else {
            SDL_FreeSurface(image);
            continue;
        }

        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, format, image->w, image->h, 0, format, GL_UNSIGNED_BYTE, image->pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        SDL_FreeSurface(image);
    }
}

void HandleJoystickInput(SDL_Joystick *joystick, int *currDirection, SDL_Rect *position, int *gameover) {
    Uint8 hat = SDL_JoystickGetHat(joystick, 0);

    if (hat & SDL_HAT_UP) {
        *currDirection = 0; position->y -= 5;
    } else if (hat & SDL_HAT_DOWN) {
        *currDirection = 2; position->y += 5; 
    }   

    if (hat & SDL_HAT_LEFT) {
        *currDirection = 3; position->x -= 5;
    } else if (hat & SDL_HAT_RIGHT) {
         *currDirection = 1; position->x += 5;
    }    
}

void drawScene(SDL_Rect spritePosition, int currentDirection, int animationFlip) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glTranslatef(0.0f, 0.0f, -5.0f);

    // Draw grass tiles
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    for(int x = 0; x < SCREEN_WIDTH / GRASS_SIZE; x++) {
        for(int y = 0; y < SCREEN_HEIGHT / GRASS_SIZE; y++) {
            glBegin(GL_QUADS);
                glTexCoord2f(0, 1); glVertex2i(x * GRASS_SIZE, y * GRASS_SIZE);
                glTexCoord2f(1, 1); glVertex2i((x + 1) * GRASS_SIZE, y * GRASS_SIZE);
                glTexCoord2f(1, 0); glVertex2i((x + 1) * GRASS_SIZE, (y + 1) * GRASS_SIZE);
                glTexCoord2f(0, 0); glVertex2i(x * GRASS_SIZE, (y + 1) * GRASS_SIZE);
            glEnd();
        }
    }

    // Draw Sprite
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    float texX = (animationFlip % 2) * SPRITE_SIZE / 256.0; // Assuming sprite sheet is 128px wide
    float texY = currentDirection * SPRITE_SIZE / 256.0;

    glBegin(GL_QUADS);
        glTexCoord2f(texX, texY + SPRITE_SIZE / 256.0); glVertex2i(spritePosition.x, spritePosition.y);
        glTexCoord2f(texX + SPRITE_SIZE / 256.0, texY + SPRITE_SIZE / 256.0); glVertex2i(spritePosition.x + SPRITE_SIZE, spritePosition.y);
        glTexCoord2f(texX + SPRITE_SIZE / 256.0, texY); glVertex2i(spritePosition.x + SPRITE_SIZE, spritePosition.y + SPRITE_SIZE);
        glTexCoord2f(texX, texY); glVertex2i(spritePosition.x, spritePosition.y + SPRITE_SIZE);
    glEnd();
}

void setupOpenGL(int width, int height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_TEXTURE_2D);
    LoadGLTextures();
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
    SDL_Window *window = SDL_CreateWindow("OpenGL with SDL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    setupOpenGL(SCREEN_WIDTH, SCREEN_HEIGHT);

    SDL_Joystick *joystick = SDL_JoystickOpen(0);
    if(!joystick) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open joystick: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int currentDirection = DIR_RIGHT, animationFlip = 0;
    SDL_Rect spritePosition = {(SCREEN_WIDTH - SPRITE_SIZE) / 2, (SCREEN_HEIGHT - SPRITE_SIZE) / 2, SPRITE_SIZE, SPRITE_SIZE};
    int gameover = 0;

    while(!gameover) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if(event.type == SDL_QUIT) gameover = 1;
            // Add any other event handling here
        }

        HandleJoystickInput(joystick, &currentDirection, &spritePosition, &gameover);
        drawScene(spritePosition, currentDirection, animationFlip);
        SDL_GL_SwapWindow(window);

        animationFlip = !animationFlip; // Toggle animation state

        SDL_Delay(50); // Control speed
    }

    glDeleteTextures(2, textures);
    SDL_JoystickClose(joystick);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}