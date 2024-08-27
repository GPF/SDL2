
#include "../../SDL_internal.h"
#include "SDL_render_dreamcast_pvr.h"
#if SDL_VIDEO_RENDER_DREAMCAST_PVR
#include "../SDL_sysrender.h"
#include "SDL_hints.h"
#include <dc/video.h>
#include <dc/pvr.h>
#include <kos.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations for the renderer functions */
void DC_DestroyRenderer(SDL_Renderer *renderer);

int DC_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture);

void DC_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture);

int DC_RenderPresent(SDL_Renderer *renderer);

int DC_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, void **pixels, int *pitch);

void DC_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture);

/* Forward declarations for the renderer stubs */
static int DC_QueueSetViewport(SDL_Renderer *renderer, SDL_RenderCommand *cmd);

static int DC_QueueSetDrawColor(SDL_Renderer *renderer, SDL_RenderCommand *cmd);

static int DC_QueueDrawPoints(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count);

static int DC_QueueDrawLines(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count);

static int DC_QueueFillRects(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FRect *rects, int count);

static int DC_QueueCopy(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_FRect *dstrect);

static int DC_QueueCopyEx(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_FRect *dstrect, double angle, const SDL_FPoint *center, SDL_RendererFlip flip);

static int DC_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const float *xy, int xy_stride, const SDL_Color *color, int color_stride, const float *uv, int uv_stride, int num_vertices, const void *indices, int num_indices, int size_indices);

static void DC_RenderBegin(SDL_Renderer *renderer);

static void DC_RenderEnd(SDL_Renderer *renderer);

int DC_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch);

int DC_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture, SDL_ScaleMode scaleMode);

int DC_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture);


/* Define the PVR renderer */
static int DC_PixelFormatToPVR(Uint32 format);
static void DC_RenderBegin(SDL_Renderer *renderer);
static void DC_RenderEnd(SDL_Renderer *renderer);

typedef struct
{
    void *frontbuffer;
    void *backbuffer;
    SDL_Texture *boundTarget;
    SDL_bool initialized;
    unsigned int bpp;
    SDL_bool vsync;
    pvr_ptr_t texture_mem;     /**< Texture memory in PVR */
    unsigned short *buffer;    /**< Software rendering buffer */
} DC_RenderData;

int DC_CreateRenderer(SDL_Renderer *renderer, SDL_Window *window, Uint32 flags) {
    DC_RenderData *data;
    int pixelformat;
    
    data = (DC_RenderData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return SDL_OutOfMemory();
    }

    renderer->driverdata = data;
    
    /* Initialize PVR */
    pvr_init_params_t params = {
        .opb_sizes = { PVR_BINSIZE_16, PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0 },
        .vertex_buf_size = 512 * 1024,
        .dma_enabled = 1
    };
    pvr_init(&params);

    /* Create front and back buffers */
    data->frontbuffer = vram_l;
    data->backbuffer = vram_l + (640 * 480 * 2); // Double buffer with 16-bit mode

    /* Setup renderer function pointers */
    renderer->info = (SDL_RendererInfo){
        .name = "Dreamcast PVR",
        .flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE,
        .num_texture_formats = 4,
        .texture_formats = { SDL_PIXELFORMAT_BGR565, SDL_PIXELFORMAT_ABGR1555, SDL_PIXELFORMAT_ABGR4444, SDL_PIXELFORMAT_ABGR8888 },
        .max_texture_width = 1024,
        .max_texture_height = 1024
    };
    
    renderer->CreateTexture = DC_CreateTexture;
    renderer->UpdateTexture = DC_UpdateTexture;
    renderer->LockTexture = DC_LockTexture;
    renderer->UnlockTexture = DC_UnlockTexture;
    renderer->SetTextureScaleMode = DC_SetTextureScaleMode;
    renderer->SetRenderTarget = DC_SetRenderTarget;
    renderer->QueueSetViewport = DC_QueueSetViewport;
    renderer->QueueSetDrawColor = DC_QueueSetDrawColor;
    renderer->QueueDrawPoints = DC_QueueDrawPoints;
    renderer->QueueDrawLines = DC_QueueDrawLines;
    renderer->QueueFillRects = DC_QueueFillRects;
    renderer->QueueCopy = DC_QueueCopy;
    renderer->QueueCopyEx = DC_QueueCopyEx;
    renderer->QueueGeometry = DC_QueueGeometry;
    renderer->RenderPresent = DC_RenderPresent;
    renderer->DestroyTexture = DC_DestroyTexture;
    renderer->DestroyRenderer = DC_DestroyRenderer;
    
    data->initialized = SDL_TRUE;
    
    return 0;
}

void DC_DestroyRenderer(SDL_Renderer *renderer) {
    DC_RenderData *data = (DC_RenderData *)renderer->driverdata;
    
    if (data->initialized) {
        pvr_shutdown();  // Shutdown PVR
        SDL_free(data);  // Free allocated memory
    }
}

int DC_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture) {
    DC_RenderData *data = (DC_RenderData *)renderer->driverdata;
    
    texture->driverdata = SDL_calloc(1, sizeof(pvr_ptr_t));
    if (!texture->driverdata) {
        return SDL_OutOfMemory();
    }
    
    /* Allocate texture memory in VRAM */
    pvr_ptr_t texture_mem = pvr_mem_malloc(texture->w * texture->h * SDL_BYTESPERPIXEL(texture->format));
    if (!texture_mem) {
        SDL_free(texture->driverdata);
        return SDL_OutOfMemory();
    }
    
    *((pvr_ptr_t *)texture->driverdata) = texture_mem;
    
    return 0;
}

void DC_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture) {
    if (texture->driverdata) {
        pvr_mem_free(*((pvr_ptr_t *)texture->driverdata));
        SDL_free(texture->driverdata);
        texture->driverdata = NULL;
    }
}

int DC_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch) {
    // Stub: Implement updating texture data
    (void)renderer; // Avoid unused parameter warning
    (void)texture;
    (void)rect;
    (void)pixels;
    (void)pitch;
    return SDL_Unsupported(); // Return an appropriate error code or handle it as needed
}

int DC_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture, SDL_ScaleMode scaleMode) {
    // Stub: Implement setting the texture scaling mode
    (void)renderer; // Avoid unused parameter warning
    (void)texture;
    (void)scaleMode;
    return SDL_Unsupported(); // Return an appropriate error code or handle it as needed
}

int DC_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture) {
    // Stub: Implement setting the render target
    (void)renderer; // Avoid unused parameter warning
    (void)texture;
    return SDL_Unsupported(); // Return an appropriate error code or handle it as needed
}

int DC_RenderPresent(SDL_Renderer *renderer) {
    DC_RenderData *data = (DC_RenderData *)renderer->driverdata;
    
    DC_RenderEnd(renderer);
    
    /* Swap buffers */
    pvr_scene_finish();
    
    if (data->vsync) {
        pvr_wait_ready();
    }
    
    /* Swap front and back buffers */
    void *tmp = data->frontbuffer;
    data->frontbuffer = data->backbuffer;
    data->backbuffer = tmp;
    
    pvr_scene_begin();
    
    return 0;
}

int DC_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, void **pixels, int *pitch) {
    *pixels = (void *)((char *)*((pvr_ptr_t *)texture->driverdata) + rect->y * texture->pitch + rect->x * SDL_BYTESPERPIXEL(texture->format));
    *pitch = texture->pitch;
    return 0;
}

void DC_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture) {
    pvr_ptr_t texture_mem = *((pvr_ptr_t *)texture->driverdata);
    pvr_txr_load(texture_mem, texture_mem, texture->w * texture->h * SDL_BYTESPERPIXEL(texture->format));
}

/* Stub functions for rendering commands */

static int DC_QueueSetViewport(SDL_Renderer *renderer, SDL_RenderCommand *cmd) {
    // Stub: Implement setting the viewport using PVR
    return 0;
}

static int DC_QueueSetDrawColor(SDL_Renderer *renderer, SDL_RenderCommand *cmd) {
    // Stub: Implement setting the draw color in PVR
    return 0;
}

static int DC_QueueDrawPoints(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count) {
    // Stub: Implement drawing points using PVR
    return 0;
}

static int DC_QueueDrawLines(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count) {
    // Stub: Implement drawing lines using PVR
    return 0;
}

static int DC_QueueFillRects(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FRect *rects, int count) {
    // Stub: Implement filling rectangles using PVR
    return 0;
}

static int DC_QueueCopy(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_FRect *dstrect) {
    // Stub: Implement copying textures using PVR
    return 0;
}

static int DC_QueueCopyEx(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_FRect *dstrect, double angle, const SDL_FPoint *center, SDL_RendererFlip flip) {
    // Stub: Implement copying textures with transformations using PVR
    return 0;
}

static int DC_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const float *xy, int xy_stride, const SDL_Color *color, int color_stride, const float *uv, int uv_stride, int num_vertices, const void *indices, int num_indices, int size_indices) {
    // Stub: Implement geometry rendering using PVR
    return 0;
}

/* Placeholder functions */

static void DC_RenderBegin(SDL_Renderer *renderer) {
    pvr_wait_ready();
    pvr_scene_begin();
}

static void DC_RenderEnd(SDL_Renderer *renderer) {
    pvr_scene_finish();
}

SDL_RenderDriver DC_RenderDriver = {
    .CreateRenderer = DC_CreateRenderer,
    .info = {
        .name = "DREAMCAST",
        .flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE,
        .num_texture_formats = 4,
        .texture_formats = {
            [0] = SDL_PIXELFORMAT_BGR565,
            [1] = SDL_PIXELFORMAT_ABGR1555,
            [2] = SDL_PIXELFORMAT_ABGR4444,
            [3] = SDL_PIXELFORMAT_ABGR8888,
        },
        .max_texture_width = 1024,
        .max_texture_height = 1024,
    }
};

#endif /* SDL_VIDEO_RENDER_DREAMCAST_PVR */
