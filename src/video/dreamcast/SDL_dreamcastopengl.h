#ifndef _SDL_dreamcastopengl_h
#define _SDL_dreamcastopengl_h

#if defined(SDL_VIDEO_DRIVER_DREAMCAST) && defined(SDL_VIDEO_OPENGL)

extern void *DREAMCAST_GL_GetProcAddress(_THIS, const char *proc);
extern int DREAMCAST_GL_LoadLibrary(_THIS, const char *path);
extern int DREAMCAST_GL_SwapBuffers(_THIS, SDL_Window * window);
extern int DREAMCAST_GL_Initialize(_THIS);
extern void DREAMCAST_GL_Shutdown(_THIS);
extern SDL_GLContext DREAMCAST_GL_CreateContext(_THIS, SDL_Window *window);
extern int DREAMCAST_GL_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context);
extern void DREAMCAST_GL_DeleteContext(_THIS, SDL_GLContext context);

#endif /* SDL_VIDEO_DRIVER_DREAMCAST && SDL_VIDEO_OPENGL */

#endif /* _SDL_dreamcastopengl_h */
