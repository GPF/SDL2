#ifndef SDL_RENDER_DREAMCAST_PVR_H
#define SDL_RENDER_DREAMCAST_PVR_H

#include "../SDL_sysrender.h"
#include <dc/pvr.h>



/* Function prototypes */
int DC_CreateRenderer(SDL_Renderer *renderer, SDL_Window *window, Uint32 flags);
void DC_DestroyRenderer(SDL_Renderer *renderer);

int DC_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture);
void DC_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture);
int DC_RenderPresent(SDL_Renderer *renderer);

int DC_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, void **pixels, int *pitch);
void DC_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture);

#endif /* SDL_RENDER_DREAMCAST_PVR_H */
