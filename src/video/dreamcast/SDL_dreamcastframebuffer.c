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

#ifdef SDL_VIDEO_DRIVER_DREAMCAST
#include <kos.h> 
#include "../SDL_sysvideo.h"
#include "SDL_dreamcastframebuffer_c.h"
#include "SDL_hints.h"
#include "../../SDL_utils_c.h"

int sdl_dc_pvr_inited =0;
#define DREAMCAST_SURFACE "_SDL_DreamcastSurface"
static int sdl_dc_textured=0;
static int sdl_dc_width=0;
static int sdl_dc_height=0;
static int sdl_dc_bpp=0;
static int sdl_dc_wtex=0;
static int sdl_dc_htex=0;
static pvr_ptr_t sdl_dc_memtex;
static unsigned short *sdl_dc_buftex;
static void *sdl_dc_memfreed;
static void *sdl_dc_dblfreed=NULL;
static void *sdl_dc_dblmem=NULL;
static unsigned sdl_dc_dblsize=0;
static float sdl_dc_u1=0.3f;
static float sdl_dc_u2=0.3f;
static float sdl_dc_v1=0.9f;
static float sdl_dc_v2=0.6f;

static void sdl_dc_blit_textured(void)
{
#define DX1 0.0f
#define DY1 0.0f
#define DZ1 1.0f
#define DWI 640.0f
#define DHE 480.0f

    pvr_poly_hdr_t *hdr;
    pvr_vertex_t *vert;
    pvr_poly_cxt_t cxt;
    pvr_dr_state_t dr_state;

    if (SDL_GetHintBoolean(SDL_HINT_RENDER_VSYNC, SDL_TRUE)) {
            vid_waitvbl();
    }
            // SDL_Log("sdl_dc_blit_textured");
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);

    if (sdl_dc_buftex)
    {
        dcache_flush_range((unsigned)sdl_dc_buftex, sdl_dc_wtex*sdl_dc_htex*2);
        while (!pvr_dma_ready());
        pvr_txr_load_dma(sdl_dc_buftex, sdl_dc_memtex, sdl_dc_wtex*sdl_dc_htex*2, -1, NULL, 0);
        // pvr_txr_load(sdl_dc_buftex, sdl_dc_memtex, sdl_dc_wtex*sdl_dc_htex*2);
    }

    pvr_dr_init(&dr_state);
    pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565|PVR_TXRFMT_NONTWIDDLED, sdl_dc_wtex, sdl_dc_htex, sdl_dc_memtex, PVR_FILTER_NEAREST);

    hdr = (pvr_poly_hdr_t *)pvr_dr_target(dr_state);
    pvr_poly_compile(hdr, &cxt);
    pvr_dr_commit(hdr);

    vert = pvr_dr_target(dr_state);
    vert->argb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
    vert->oargb = 0;
    vert->flags = PVR_CMD_VERTEX;
    vert->x = DX1; vert->y = DY1; vert->z = DZ1; vert->u = sdl_dc_u1; vert->v = sdl_dc_v1;
    pvr_dr_commit(vert);

    vert = pvr_dr_target(dr_state);
    vert->argb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
    vert->oargb = 0;
    vert->flags = PVR_CMD_VERTEX;
    vert->x = DX1+DWI; vert->y = DY1; vert->z = DZ1; vert->u = sdl_dc_u2; vert->v = sdl_dc_v1;
    pvr_dr_commit(vert);

    vert = pvr_dr_target(dr_state);
    vert->argb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
    vert->oargb = 0;
    vert->flags = PVR_CMD_VERTEX;
    vert->x = DX1; vert->y = DY1+DHE; vert->z = DZ1; vert->u = sdl_dc_u1; vert->v = sdl_dc_v2;
    pvr_dr_commit(vert);

    vert = pvr_dr_target(dr_state);
    vert->argb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
    vert->oargb = 0;
    vert->flags = PVR_CMD_VERTEX_EOL;
    vert->x = DX1+DWI; vert->y = DY1+DHE; vert->z = DZ1; vert->u = sdl_dc_u2; vert->v = sdl_dc_v2;
    pvr_dr_commit(vert);

    pvr_dr_finish();
    pvr_list_finish();
    pvr_scene_finish();

#undef DX1
#undef DY1
#undef DZ1
#undef DWI
#undef DHE
}

int SDL_DREAMCAST_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch) 
{
    SDL_Surface *surface;
    int w, h;
    SDL_DisplayMode mode;
    Uint32 surface_format = mode.format;
    int target_bpp;
    const char *video_mode_hint = SDL_GetHint(SDL_HINT_DC_VIDEO_MODE);
    const char *double_buffer_hint = SDL_GetHint(SDL_HINT_VIDEO_DOUBLE_BUFFER);

    /* Free the old framebuffer surface */
    SDL_DREAMCAST_DestroyWindowFramebuffer(_this, window);

    SDL_GetWindowSizeInPixels(window, &w, &h);

    // Retrieve the display mode's format to ensure consistency with the surface creation

    if (SDL_GetWindowDisplayMode(window, &mode) != 0) {
        return SDL_SetError("Failed to get window display mode");
    }

    // Use the format from the display mode or fallback to the default

    if (surface_format != SDL_PIXELFORMAT_RGB565 &&
        surface_format != SDL_PIXELFORMAT_ARGB1555 &&
        surface_format != SDL_PIXELFORMAT_RGB888 &&
        surface_format != SDL_PIXELFORMAT_ARGB8888) {
        surface_format = SDL_PIXELFORMAT_RGB888;  // Default format if unsupported
    }

    // Determine color depth (bit depth) based on video mode
    if (video_mode_hint != NULL && strcmp(video_mode_hint, "SDL_DC_TEXTURED_VIDEO") == 0) {
        // Use 16-bit for textured mode, keep the format but change bit depth
        target_bpp = 16;  // Adjust bit depth to 16-bit for textured mode
        surface_format = SDL_PIXELFORMAT_RGB565;
        sdl_dc_width =w;
        sdl_dc_height=h;
        w = SDL_powerof2(w);  
        h = SDL_powerof2(h);         
    } else {
        // Default to the original bit depth from the display mode
        target_bpp = SDL_BITSPERPIXEL(surface_format);  // Maintain the original depth if not textured mode
    }    
    surface->pitch = w * (target_bpp / 8);

    // Create the surface with the appropriate format
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 
                                             target_bpp, 
                                             surface_format);
    if (!surface) {
        return -1;
    }

    if (sdl_dc_pvr_inited !=1 && (video_mode_hint != NULL && strcmp(video_mode_hint, "SDL_DC_DIRECT_VIDEO") == 0)) {
        surface->pixels = (void *)vram_l;  // Default for SDL_DC_DIRECT_VIDEO
        SDL_Log("Inited SDL_DC_DIRECT_VIDEO"); 
    }

    if (sdl_dc_pvr_inited !=1 && (video_mode_hint != NULL && strcmp(video_mode_hint, "SDL_DC_DMA_VIDEO") == 0)) {
        pvr_dma_init();
        sdl_dc_pvr_inited =1;
        SDL_Log("Inited SDL_DC_DMA_VIDEO");   
		sdl_dc_dblsize=(w*h*(SDL_BITSPERPIXEL(surface_format)));
		sdl_dc_dblfreed = calloc(128+sdl_dc_dblsize,1);
		sdl_dc_dblmem = (unsigned short *)(((((unsigned)sdl_dc_dblfreed)+64)/64)*64);        
        surface->pixels = (void *)sdl_dc_dblmem;
    }

    if (sdl_dc_textured !=1 && (video_mode_hint != NULL && strcmp(video_mode_hint, "SDL_DC_TEXTURED_VIDEO") == 0)) {
		pvr_init_defaults();
		pvr_dma_init();
		sdl_dc_pvr_inited = 1;
        sdl_dc_textured =1;
        sdl_dc_bpp=SDL_BITSPERPIXEL(surface_format);
        sdl_dc_wtex = w;  
        sdl_dc_htex = h;   
        SDL_Log("sdl_dc_wtex: %d",sdl_dc_wtex);
        SDL_Log("sdl_dc_htex: %d",sdl_dc_htex);
        sdl_dc_memtex = pvr_mem_malloc(sdl_dc_wtex*sdl_dc_htex*2);        
        sdl_dc_u1 = 0.0f;
        sdl_dc_v1 = 0.0f;
        sdl_dc_u2 = (float)sdl_dc_width / (float)sdl_dc_wtex;
        sdl_dc_v2 = (float)sdl_dc_height / (float)sdl_dc_htex;
        SDL_Log("Inited SDL_DC_TEXTURED_VIDEO");
        if (double_buffer_hint && SDL_GetHintBoolean(SDL_HINT_VIDEO_DOUBLE_BUFFER, SDL_TRUE)) {
			sdl_dc_memfreed = calloc(64+(sdl_dc_wtex*sdl_dc_htex*(sdl_dc_bpp>>3)),1);
			sdl_dc_buftex = (unsigned short *)(((((unsigned)sdl_dc_memfreed)+32)/32)*32);            
            surface->pixels = (void *)sdl_dc_buftex;
        }
        else {
			sdl_dc_buftex = 0;
			surface->pixels = sdl_dc_memtex;
		}        
    }

    /* Save the info and return! */
    SDL_SetWindowData(window, DREAMCAST_SURFACE, surface);
    *format = surface_format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;
    return 0;
}

int SDL_DREAMCAST_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_Surface *surface;
    Uint32 *dst;
    int w, h, pitch, i;
    Uint32 *src_row;
    const char *video_mode_hint = SDL_GetHint(SDL_HINT_DC_VIDEO_MODE);
    surface = (SDL_Surface *)SDL_GetWindowData(window, DREAMCAST_SURFACE);
    if (!surface) {
        return SDL_SetError("Couldn't find framebuffer surface for window");
    }

    // Retrieve dimensions and pitch of the SDL surface
    w = surface->w;
    h = surface->h;
    pitch = surface->pitch;

    // Assuming vram_l points to the start of the framebuffer
    dst = (Uint32 *)vram_l;
    src_row = (Uint32 *)surface->pixels;

    // Update the entire framebuffer in one go if no specific rectangles are provided
    if (numrects == 1) {
    if (video_mode_hint != NULL && strcmp(video_mode_hint, "SDL_DC_TEXTURED_VIDEO") == 0) {
        sdl_dc_blit_textured();
    }
    else if (video_mode_hint != NULL && strcmp(video_mode_hint, "SDL_DC_DMA_VIDEO") == 0) {
        //   SDL_Log("Using dma video mode");    
        // // Ensure the data cache is flushed before DMA transfer
        dcache_flush_range((uintptr_t)surface->pixels, h * pitch);
        while (!pvr_dma_ready());  // Wait for any prior DMA transfer to complete
        // Perform the DMA transfer from surface->pixels to vram_l
        pvr_dma_transfer((void*)surface->pixels, (uintptr_t)vram_l, h * pitch, PVR_DMA_VRAM32, -1, NULL, NULL); 
    }
    else{
        // SDL_Log("Using direct video mode");        
        sq_cpy(vram_l, surface->pixels, h * pitch);  // Copies the entire framebuffer at once 
    }} else {
        SDL_Log("numrects: %d",numrects);       
        // Update only specified rectangles if provided
        for (i = 0; i < numrects; ++i) {
            const SDL_Rect *rect = &rects[i];
            for (int y = rect->y; y < rect->y + rect->h; ++y) {
                sq_cpy(dst + ((y * (pitch / sizeof(Uint32))) + rect->x),
                    src_row + ((y * (pitch / sizeof(Uint32))) + rect->x),
                    rect->w * sizeof(Uint32));
            }
        }
    }

    if (video_mode_hint != NULL && strcmp(video_mode_hint, "SDL_DC_TEXTURED_VIDEO") != 0) {
        // Check SDL hints before synchronization
        const char *double_buffer_hint = SDL_GetHint(SDL_HINT_VIDEO_DOUBLE_BUFFER);
        if (double_buffer_hint && SDL_strcasecmp(double_buffer_hint, "1") == 0) {
            if (SDL_GetHintBoolean(SDL_HINT_RENDER_VSYNC, SDL_TRUE)) {
                vid_waitvbl();
                // SDL_Log("SDL_HINT_RENDER_VSYNC");
            }
            if (video_mode_hint != NULL && strcmp(video_mode_hint, "SDL_DC_TEXTURED_VIDEO") != 0) {
                vid_flip(-1);
                // SDL_Log("SDL_HINT_VIDEO_DOUBLE_BUFFER");
            }
        }
    // fb = !fb;
    }

    return 0;
}

void SDL_DREAMCAST_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_Surface *surface;

    // Retrieve and clear the framebuffer surface data associated with the window
    surface = (SDL_Surface *)SDL_SetWindowData(window, DREAMCAST_SURFACE, NULL);
    if (surface) {
        SDL_FreeSurface(surface);
    }

    // Free any additional allocated memory for DMA and textured modes
    if (sdl_dc_dblfreed) {
        free(sdl_dc_dblfreed);
        sdl_dc_dblfreed = NULL;
        sdl_dc_dblmem = NULL;
        sdl_dc_dblsize = 0;
    }

    if (sdl_dc_memfreed) {
        free(sdl_dc_memfreed);
        sdl_dc_memfreed = NULL;
        sdl_dc_buftex = NULL;
    }

    // Free PVR allocated texture memory for textured mode
    if (sdl_dc_memtex) {
        pvr_mem_free(sdl_dc_memtex);
        sdl_dc_memtex = NULL;
    }

    // Reset mode-specific flags and states
    sdl_dc_pvr_inited = 0;
    sdl_dc_textured = 0;
    sdl_dc_width = 0;
    sdl_dc_height = 0;
    sdl_dc_bpp = 0;
    sdl_dc_wtex = 0;
    sdl_dc_htex = 0;
}

#endif /* SDL_VIDEO_DRIVER_DREAMCAST */

/* vi: set ts=4 sw=4 expandtab: */
