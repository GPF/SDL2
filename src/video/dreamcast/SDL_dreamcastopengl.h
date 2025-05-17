#ifndef _SDL_dreamcastopengl_h
#define _SDL_dreamcastopengl_h

#if defined(SDL_VIDEO_DRIVER_DREAMCAST) && defined(SDL_VIDEO_OPENGL)

extern void (*DREAMCAST_GL_GetProcAddress(SDL_VideoDevice *, const char *))(void);
extern bool DREAMCAST_GL_LoadLibrary(SDL_VideoDevice *_this, const char *path);
extern bool DREAMCAST_GL_SwapBuffers(SDL_VideoDevice *_this, SDL_Window * window);
extern bool DREAMCAST_GL_Initialize(SDL_VideoDevice *_this);
extern void DREAMCAST_GL_Shutdown(SDL_VideoDevice *_this);
extern SDL_GLContext DREAMCAST_GL_CreateContext(SDL_VideoDevice *_this, SDL_Window *window);
extern bool DREAMCAST_GL_MakeCurrent(SDL_VideoDevice *_this, SDL_Window *window, SDL_GLContext context);
extern void DREAMCAST_GL_DeleteContext(SDL_VideoDevice *_this, SDL_GLContext context);

#endif /* SDL_VIDEO_DRIVER_DREAMCAST && SDL_VIDEO_OPENGL */

#endif /* _SDL_dreamcastopengl_h */
