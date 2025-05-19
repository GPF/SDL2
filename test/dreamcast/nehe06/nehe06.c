/*  DREAMCAST
 *IAN MICHEAL Ported SDL+OPENGL USING SDL[DREAMHAL][GLDC][KOS2.0]2021
 * Cleaned and tested on dreamcast hardware by Ianmicheal
 *		This Code Was Created By Pet & Commented/Cleaned Up By Jeff Molofee
 *		If You've Found This Code Useful, Please Let Me Know.
 *		Visit NeHe Productions At http://nehe.gamedev.net
 */
//Troy Davis GPF ported to dreamcast SDL3+opengl 
#include <math.h>            // Header File For Windows Math Library
#include <stdio.h>           // Header File For Standard Input/Output
#include <stdlib.h>          // Header File For Standard Library
#include "GL/gl.h"
#include "GL/glu.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_surface.h> 
// #include <SDL3/SDL_opengl.h>

#define FPS 60
Uint32 waittime = 1000.0f / FPS;
Uint32 framestarttime = 0;
Sint32 delaytime;

float xrot = 0.0f, yrot = 0.0f, zrot = 0.0f;
float xspeed = 0.0f, yspeed = 0.0f, zspeed = 0.0f;
const float ROTATION_SPEED = 90.0f;

GLuint texture[1]; 

// Load BMP and convert to texture
SDL_Surface *LoadBMP(char *filename) {
    Uint8 *rowhi, *rowlo;
    Uint8 *tmpbuf, tmpch;
    SDL_Surface *image = SDL_LoadBMP(filename);
    if (!image) {
        fprintf(stderr, "Unable to load %s: %s\n", filename, SDL_GetError());
        return NULL;
    }
const char* format_name = SDL_GetPixelFormatName(image->format);
printf("Image surface format: %s\n", format_name);  

    printf("SDL_LoadBMP_RW\n"); 
    SDL_SetSurfaceColorKey(image, 1, *((Uint8 *)image->pixels));
 
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
    printf("SDL_LoadBMP_RW2\n"); 
    return image;
}

// Load textures
void LoadGLTextures() {
    SDL_Surface *image1 = LoadBMP("/rd/NeHe.bmp");
    if (!image1) {
        SDL_Quit();
        exit(1);
    }
    printf("LoadGLTextures\n");
    glGenTextures(1, &texture[0]); 
    printf("LoadGLTextures2\n");
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // Create an empty texture first
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image1->w, image1->h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

    // Update the texture with the image data
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image1->w, image1->h, GL_RGB, GL_UNSIGNED_BYTE, image1->pixels);    

    SDL_DestroySurface(image1);
   
}


/* A general OpenGL initialization function.  Sets all of the initial parameters. */
void InitGL(int Width, int Height)            // We call this right after our OpenGL window is created.
{
    glViewport(0, 0, Width, Height);
    LoadGLTextures();
    glEnable(GL_TEXTURE_2D);
    glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
    glClearDepth(1.0);
    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (GLfloat)Width / (GLfloat)Height, 0.1f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
}

void DrawGLScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glTranslatef(0.0f,0.0f,-5.0f);
    
    glRotatef(xrot,1.0f,0.0f,0.0f);
    glRotatef(yrot,0.0f,1.0f,0.0f);
    glRotatef(zrot,0.0f,0.0f,1.0f);

    glBindTexture(GL_TEXTURE_2D, texture[0]);

    glBegin(GL_QUADS);
    
    // Front Face
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
    
    // Back Face
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
    
    // Top Face
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
    
    // Bottom Face
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
    
    // Right face
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
    
    // Left Face
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
    
    glEnd();

    SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
}

#define MAX_ROTATION_SPEED 3.0f

void HandleJoystickInput(SDL_Joystick *joystick, float *xspeed, float *yspeed, const float ROTATION_SPEED) {
    if (!joystick) return;

    Uint8 hat = SDL_GetJoystickHat(joystick, 0);

    if (hat & SDL_HAT_UP) {
        *xspeed -= ROTATION_SPEED * 0.101f;
    } else if (hat & SDL_HAT_DOWN) {
        *xspeed += ROTATION_SPEED * 0.101f;
    }

    if (hat & SDL_HAT_LEFT) {
        *yspeed -= ROTATION_SPEED * 0.101f;
    } else if (hat & SDL_HAT_RIGHT) {
        *yspeed += ROTATION_SPEED * 0.101f;
    }

    // Clamp values
    if (*xspeed > MAX_ROTATION_SPEED) *xspeed = MAX_ROTATION_SPEED;
    if (*xspeed < -MAX_ROTATION_SPEED) *xspeed = -MAX_ROTATION_SPEED;
    if (*yspeed > MAX_ROTATION_SPEED) *yspeed = MAX_ROTATION_SPEED;
    if (*yspeed < -MAX_ROTATION_SPEED) *yspeed = -MAX_ROTATION_SPEED;

    if (SDL_GetJoystickButton(joystick, 0)) {
        *xspeed = 0.0f;
        *yspeed = 0.0f;
    }
}



void HandleGameControllerInput(SDL_Gamepad *controller, float *xspeed, float *yspeed, const float ROTATION_SPEED) {
    if (!controller) return;

    // Axes: -1 to +1
    float axis_x = SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_LEFTX);
    float axis_y = SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_LEFTY);

    // Dead zone filtering
    const float DEAD_ZONE = 0.2f;
    *xspeed = fabsf(axis_y) > DEAD_ZONE ? axis_y * ROTATION_SPEED : 0.0f;
    *yspeed = fabsf(axis_x) > DEAD_ZONE ? axis_x * ROTATION_SPEED : 0.0f;

    if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_SOUTH)) {
        *xspeed = 0.0f;
        *yspeed = 0.0f;
    }
}


int main(int argc, char **argv) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD);
    SDL_Window *window = SDL_CreateWindow("Dreamcast SDL3 OpenGL", 640, 480, SDL_WINDOW_OPENGL);
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext); 

int joystick_count = 0;
SDL_Joystick *joy=NULL;
SDL_JoystickID *joysticks = SDL_GetJoysticks(&joystick_count);

if (joysticks == NULL) {
    SDL_Log("SDL_GetJoysticks failed: %s", SDL_GetError());
} else {
    SDL_Log("Number of joysticks detected: %d", joystick_count);

    for (int i = 0; i < joystick_count; i++) {
        SDL_JoystickID instance_id = joysticks[i];
        SDL_Log("Joystick instance ID: %ld", (long)instance_id);

        joy = SDL_OpenJoystick(instance_id);
        if (joy) {
            SDL_Log("Opened joystick: %s", SDL_GetJoystickName(joy));
            // Use joystick `joy` as needed...
        } else {
            SDL_Log("Failed to open joystick %ld: %s", (long)instance_id, SDL_GetError());
        }
    }

    SDL_free(joysticks);
}

       // Get gamepads
    // SDL_Gamepad *controller = NULL;
    // int count = 0;
    // SDL_JoystickID *gamepads = SDL_GetGamepads(&count);
    // SDL_Log("Number of gamepads detected: %d", count);

    // if (count > 0) {
    //     SDL_JoystickID instance_id = gamepads[0];
    //     SDL_GUID guid = SDL_GetGamepadGUIDForID(instance_id);
    //     char guid_str[33];
    //     SDL_GUIDToString(guid, guid_str, sizeof(guid_str));
    //     SDL_Log("Gamepad instance ID %ld GUID: %s", (long)instance_id, guid_str);

    //     controller = SDL_OpenGamepad(instance_id);
    //     if (!controller) {
    //         SDL_Log("Failed to open gamepad (instance ID %ld): %s", (long)instance_id, SDL_GetError());
    //     } else {
    //         SDL_Log("Opened gamepad: %s", SDL_GetGamepadName(controller));
    //     }
    // } else {
    //     SDL_Log("No compatible SDL3 gamepads found.");
    // }
    // SDL_free(gamepads);

    InitGL(640, 480);
    int done = 0;
    Uint32 lastTime = SDL_GetTicksNS();

    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            HandleJoystickInput(joy, &xspeed, &yspeed, ROTATION_SPEED);

            switch (event.type) {
                case SDL_EVENT_QUIT:
                    break;

                case SDL_EVENT_JOYSTICK_AXIS_MOTION:
                    SDL_Log("JoyAxis: instance_id=%ld, axis=%d, value=%d",
                            (long)event.jaxis.which, event.jaxis.axis, event.jaxis.value);
                    break;

                case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
                    SDL_Log("JoyButtonDown: instance_id=%ld, button=%d",
                            (long)event.jbutton.which, event.jbutton.button);
                    break;

                case SDL_EVENT_JOYSTICK_BUTTON_UP:
                    SDL_Log("JoyButtonUp: instance_id=%ld, button=%d",
                            (long)event.jbutton.which, event.jbutton.button);
                    break;

                default:
                    // SDL_Log("Event type: %d", event.type);
                    break;
            }
        }

    // HandleGameControllerInput(controller, &xspeed, &yspeed, ROTATION_SPEED);

            // printf("xspeed: %f, yspeed: %f\n", xspeed, yspeed);
        Uint32 frameStart  = SDL_GetTicksNS();
        float deltaTime = (frameStart - lastTime) / 1000000.0f;
        lastTime = frameStart;

        xrot += xspeed * deltaTime;
        yrot += yspeed * deltaTime;
        zrot += zspeed * deltaTime;

        DrawGLScene();

        Uint32 frameTime = SDL_GetTicksNS() - frameStart;
        if (frameTime < waittime) {
            SDL_Delay(waittime - frameTime);
        }
    }

    // SDL_CloseGamepad(controller);
    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
