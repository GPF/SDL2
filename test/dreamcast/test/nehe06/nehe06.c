/*  DREAMCAST
 *IAN MICHEAL Ported SDL+OPENGL USING SDL[DREAMHAL][GLDC][KOS2.0]2021
 * Cleaned and tested on dreamcast hardware by Ianmicheal
 *		This Code Was Created By Pet & Commented/Cleaned Up By Jeff Molofee
 *		If You've Found This Code Useful, Please Let Me Know.
 *		Visit NeHe Productions At http://nehe.gamedev.net
 */
//Troy Davis GPF ported to dreamcast SDL2+opengl 
#include <math.h>            // Header File For Windows Math Library
#include <stdio.h>           // Header File For Standard Input/Output
#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glkos.h"
#include "SDL2/SDL.h"
#include <kos.h>
#define FPS 60
Uint32 waittime = 1000.0f/FPS;
Uint32 framestarttime = 0;
Sint32 delaytime;
#define BMP_PATH "/rd/NeHe.bmp"

/* floats for x rotation, y rotation, z rotation */
float xrot, yrot, zrot;

/* storage for one texture  */
GLuint texture[1];
SDL_Window *window;

SDL_Surface* LoadBMP(char *filename) {
    Uint8 *rowhi, *rowlo;
    Uint8 *tmpbuf, tmpch;
    SDL_Surface *image;
    int i, j;

    image = SDL_LoadBMP(filename);
    if (image == NULL) {
        fprintf(stderr, "Unable to load %s: %s\n", filename, SDL_GetError());
        return NULL;
    }

    tmpbuf = (Uint8 *)malloc(image->pitch);
    if (tmpbuf == NULL) {
        fprintf(stderr, "Out of memory\n");
        return NULL;
    }
    rowhi = (Uint8 *)image->pixels;
    rowlo = rowhi + (image->h * image->pitch) - image->pitch;
    for (i = 0; i < image->h / 2; ++i) {
        for (j = 0; j < image->w; ++j) {
            tmpch = rowhi[j * 3];
            rowhi[j * 3] = rowhi[j * 3 + 2];
            rowhi[j * 3 + 2] = tmpch;
            tmpch = rowlo[j * 3];
            rowlo[j * 3 + 2] = tmpch;
        }
        memcpy(tmpbuf, rowhi, image->pitch);
        memcpy(rowhi, rowlo, image->pitch);
        memcpy(rowlo, tmpbuf, image->pitch);
        rowhi += image->pitch;
        rowlo -= image->pitch;
    }
    free(tmpbuf);
    return image;
}

void LoadGLTextures(void) {
    SDL_Surface *image1;

    image1 = LoadBMP(BMP_PATH);
    if (!image1) {
        SDL_Quit();
        exit(1);
    }

    glGenTextures(1, &texture[0]);
    glBindTexture(GL_TEXTURE_2D, texture[0]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, 3, image1->w, image1->h, 0, GL_RGB, GL_UNSIGNED_BYTE, image1->pixels);
    SDL_FreeSurface(image1);
}


/* A general OpenGL initialization function.  Sets all of the initial parameters. */
void InitGL(int Width, int Height)            // We call this right after our OpenGL window is created.
{
    glViewport(0, 0, Width, Height);
    LoadGLTextures();                // Load The Texture(s) 
    glEnable(GL_TEXTURE_2D);            // Enable Texture Mapping
    glClearColor(0.0f, 0.0f, 1.0f, 0.0f);    // Clear The Background Color To Blue 
    glClearDepth(1.0);                // Enables Clearing Of The Depth Buffer
    glDepthFunc(GL_LESS);            // The Type Of Depth Test To Do
    glEnable(GL_DEPTH_TEST);            // Enables Depth Testing
    glShadeModel(GL_SMOOTH);            // Enables Smooth Color Shading
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();                // Reset The Projection Matrix
    
    gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,100.0f);    // Calculate The Aspect Ratio Of The Window
    
    glMatrixMode(GL_MODELVIEW);
}

/* The main drawing function. */
void DrawGLScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);        // Clear The Screen And The Depth Buffer
    glLoadIdentity();                // Reset The View

    glTranslatef(0.0f,0.0f,-5.0f);              // move 5 units into the screen.
    
    glRotatef(xrot,1.0f,0.0f,0.0f);        // Rotate On The X Axis
    glRotatef(yrot,0.0f,1.0f,0.0f);        // Rotate On The Y Axis
    glRotatef(zrot,0.0f,0.0f,1.0f);        // Rotate On The Z Axis

    glBindTexture(GL_TEXTURE_2D, texture[0]);   // choose the texture to use.

    glBegin(GL_QUADS);                        // begin drawing a cube
    
    // Front Face (note that the texture's corners have to match the quad's corners)
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);    // Bottom Left Of The Texture and Quad
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);    // Bottom Right Of The Texture and Quad
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);    // Top Right Of The Texture and Quad
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);    // Top Left Of The Texture and Quad
    
    // Back Face
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);    // Bottom Right Of The Texture and Quad
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);    // Top Right Of The Texture and Quad
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);    // Top Left Of The Texture and Quad
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);    // Bottom Left Of The Texture and Quad
    
    // Top Face
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);    // Top Left Of The Texture and Quad
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,  1.0f,  1.0f);    // Bottom Left Of The Texture and Quad
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,  1.0f,  1.0f);    // Bottom Right Of The Texture and Quad
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);    // Top Right Of The Texture and Quad
    
    // Bottom Face       
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, -1.0f, -1.0f);    // Top Right Of The Texture and Quad
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f, -1.0f, -1.0f);    // Top Left Of The Texture and Quad
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);    // Bottom Left Of The Texture and Quad
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);    // Bottom Right Of The Texture and Quad
    
    // Right face
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);    // Bottom Right Of The Texture and Quad
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);    // Top Right Of The Texture and Quad
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);    // Top Left Of The Texture and Quad
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);    // Bottom Left Of The Texture and Quad
    
    // Left Face
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);    // Bottom Left Of The Texture and Quad
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);    // Bottom Right Of The Texture and Quad
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);    // Top Right Of The Texture and Quad
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);    // Top Left Of The Texture and Quad
    
    glEnd();                                    // done with the polygon.

    // swap buffers to display, since we're double buffered.
    SDL_GL_SwapWindow(window);

    // increase the rotation angles for the next frame.
    xrot += 0.3f;
    yrot += 0.2f;
    zrot += 0.4f;
}

/* Main function: GLUT runs as a console application starting at main()  */
int main(int argc, char **argv)
{  
    int done;
    SDL_Event event;

    SDL_GLContext glcontext;
    // Initialize KOS and GLdc
    // glKosInit();
    /* Initialize SDL for video output */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    /* Create a 640x480 OpenGL screen */
    window = SDL_CreateWindow("NeHe's OpenGL Lesson 6",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              640, 480,
                              SDL_WINDOW_OPENGL);
    
    if (window == NULL) {
        fprintf(stderr, "Unable to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    /* Create an OpenGL context associated with the window */
    glcontext = SDL_GL_CreateContext(window);
    if (glcontext == NULL) {
        fprintf(stderr, "Unable to create OpenGL context: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* Initialize OpenGL */
    InitGL(640, 480);

    /* Loop, drawing and processing events */
    done = 0;
    while (!done) {
        /* Process incoming events */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                done = 1;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    done = 1;
                }
            }
        }
        
        /* Draw the scene */
        DrawGLScene();
        
        /* Frame limiting */
        delaytime = waittime + framestarttime - SDL_GetTicks();
        if (delaytime > 0) {
            SDL_Delay(delaytime);
        }
        framestarttime = SDL_GetTicks();
    }

    /* Clean up */
    SDL_GL_DeleteContext(glcontext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
