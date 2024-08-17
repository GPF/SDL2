/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"
#include "SDL_dreamcastvideo.h"
#include "SDL_dreamcastopengl.h"
#include "SDL_video.h"

#if defined(SDL_VIDEO_DRIVER_DREAMCAST) && defined(SDL_VIDEO_OPENGL)
#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glkos.h"

typedef void (*funcptr)();
const static struct {
    char *name;
    funcptr addr;
} glfuncs[] = {
    #define DEF(func) { #func, (funcptr) &func }
    DEF(glBegin),
    DEF(glBindTexture),
    DEF(glBlendFunc),
    DEF(glClear),
    DEF(glClearColor),
    DEF(glCullFace),
    DEF(glDepthFunc),
    DEF(glDepthMask),
    DEF(glScissor),
    DEF(glColor3f),
    DEF(glColor3fv),
    DEF(glColor4f),
    DEF(glColor4fv),
    DEF(glColor4ub),
    DEF(glNormal3f),
    DEF(glDisable),
    DEF(glEnable),
    DEF(glFrontFace),
    DEF(glEnd),
    DEF(glFlush),
    DEF(glHint),
    DEF(glGenTextures),
    DEF(glGetString),
    DEF(glLoadIdentity),
    DEF(glLoadMatrixf),
    DEF(glLoadTransposeMatrixf),
    DEF(glMatrixMode),
    DEF(glMultMatrixf),
    DEF(glMultTransposeMatrixf),
    DEF(glOrtho),
    DEF(glPixelStorei),
    DEF(glPopMatrix),
    DEF(glPushMatrix),
    DEF(glRotatef),
    DEF(glScalef),
    DEF(glTranslatef),
    DEF(glTexCoord2f),
    DEF(glTexCoord2fv),
    DEF(glTexEnvi),
    DEF(glTexImage2D),
    DEF(glTexParameteri),
    DEF(glTexSubImage2D),
    DEF(glViewport),
    DEF(glShadeModel),
    DEF(glTexEnvf),
    DEF(glVertex2i)
    #undef DEF
};

typedef struct {
    int dummy; // Placeholder for any necessary context-specific information
} DreamcastGLContext;

void *DREAMCAST_GL_GetProcAddress(_THIS, const char *proc) {
    for (int i = 0; i < sizeof(glfuncs) / sizeof(glfuncs[0]); i++) {
        if (SDL_strcmp(proc, glfuncs[i].name) == 0) {
            return glfuncs[i].addr;
        }
    }
    return NULL;
}

int DREAMCAST_GL_LoadLibrary(_THIS, const char *path) {
    _this->gl_config.driver_loaded = 1;
    return 0;
}

void DREAMCAST_GL_SwapBuffers(_THIS) {
    glKosSwapBuffers();
}

int DREAMCAST_GL_Initialize(_THIS) {
    printf("Initializing SDL2 GLdc...\n");
    glKosInit();
    if (DREAMCAST_GL_LoadLibrary(_this, NULL) < 0) {
        return -1;
    }
    return 0;
}

void DREAMCAST_GL_Shutdown(_THIS) {
    printf("Shutting down GLdc...\n");
    glKosShutdown();
}

SDL_GLContext DREAMCAST_GL_CreateContext(_THIS, SDL_Window *window) {
    DreamcastGLContext *context;

    // Initialize the OpenGL driver if it hasn't been initialized
    if (DREAMCAST_GL_Initialize(_this) < 0) {
        SDL_SetError("Failed to initialize OpenGL");
        return NULL;
    }

    printf("Creating Dreamcast SDL2 OpenGL context...\n");

    context = (DreamcastGLContext *) SDL_calloc(1, sizeof(DreamcastGLContext));
    if (!context) {
        SDL_OutOfMemory();
        return NULL;
    }

    return (SDL_GLContext) context;
}

int DREAMCAST_GL_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context) {
    DreamcastGLContext *glcontext = (DreamcastGLContext *) context;
    if (!glcontext) {
        return -1;
    }
    printf("Making context current...\n");
    return 0;
}

void DREAMCAST_GL_DeleteContext(_THIS, SDL_GLContext context) {
    DreamcastGLContext *glcontext = (DreamcastGLContext *) context;
    if (glcontext) {
        SDL_free(glcontext);
    }
}

void SDL_DREAMCAST_InitGL(_THIS) { 
    if (DREAMCAST_GL_Initialize(_this) < 0) {
        SDL_SetError("Failed to initialize OpenGL");
    }
}

#endif /* SDL_VIDEO_DRIVER_DREAMCAST */
/* vi: set ts=4 sw=4 expandtab: */
