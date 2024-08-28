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

#define LOG_FUNCTION_CALL() SDL_Log("%s called", __func__)
#define LOG_FUNCTION_COMPLETE() SDL_Log("%s completed", __func__)

// Forward declarations of functions
void DC_DestroyRenderer(SDL_Renderer *renderer);
int DC_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture);
void DC_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture);
int DC_RenderPresent(SDL_Renderer *renderer);
int DC_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, void **pixels, int *pitch);
void DC_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture);

// Command queue functions
static int DC_QueueSetDrawColor(SDL_Renderer *renderer, SDL_RenderCommand *cmd);
static int DC_QueueSetViewport(SDL_Renderer *renderer, SDL_RenderCommand *cmd);
static int DC_QueueSetClipRect(SDL_Renderer *renderer, SDL_RenderCommand *cmd);
static int DC_QueueClear(SDL_Renderer *renderer, SDL_RenderCommand *cmd);
static int DC_QueueDrawPoints(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count);
static int DC_QueueDrawLines(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count);
static int DC_QueueFillRects(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FRect *rects, int count);
static int DC_QueueCopy(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_FRect *dstrect);
static int DC_QueueCopyEx(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_FRect *dstrect, double angle, const SDL_FPoint *center, SDL_RendererFlip flip);
static int DC_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const float *xy, int xy_stride, const SDL_Color *color, int color_stride, const float *uv, int uv_stride, int num_vertices, const void *indices, int num_indices, int size_indices);

// Render lifecycle functions
static void DC_RenderBegin(SDL_Renderer *renderer);
static void DC_RenderEnd(SDL_Renderer *renderer);
static int DC_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize);

// Texture scaling and render target functions
int DC_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch);
int DC_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture, SDL_ScaleMode scaleMode);
int DC_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture);

// Structure to hold renderer data
typedef struct {
    void *frontbuffer;
    void *backbuffer;
    SDL_Texture *boundTarget;
    SDL_bool initialized;
    unsigned int bpp;
    SDL_bool vsync;
    pvr_ptr_t texture_mem;
    unsigned short *buffer;
} DC_RenderData;

typedef struct {
    SDL_Color color;
} SDL_RenderCommand_SetDrawColor;

typedef struct {
    SDL_Rect viewport;
} SDL_RenderCommand_SetViewport;

typedef struct {
    SDL_Rect clip_rect;
} SDL_RenderCommand_SetClipRect;

typedef struct {
    SDL_Color color;
} SDL_RenderCommand_Clear;

typedef struct {
    SDL_Texture *texture;
    SDL_Rect srcrect;
    SDL_FRect dstrect;
} SDL_RenderCommand_Copy;

typedef struct {
    SDL_Texture *texture;
    SDL_Rect srcrect;
    SDL_FRect dstrect;
    double angle;
    SDL_FPoint center;
    SDL_RendererFlip flip;
} SDL_RenderCommand_CopyEx;

typedef struct {
    SDL_Texture *texture;
    float *xy;
    int xy_stride;
    SDL_Color *color;
    int color_stride;
    float *uv;
    int uv_stride;
    int num_vertices;
    void *indices;
    int num_indices;
    int size_indices;
} SDL_RenderCommand_Geometry;



// Create renderer function
int DC_CreateRenderer(SDL_Renderer *renderer, SDL_Window *window, Uint32 flags) {
    LOG_FUNCTION_CALL();
    DC_RenderData *data = (DC_RenderData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return SDL_OutOfMemory();
    }

    renderer->driverdata = data;

    // Initialize PVR
    pvr_init_params_t params = {
        .opb_sizes = { PVR_BINSIZE_16, PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0 },
        .vertex_buf_size = 512 * 1024,
        .dma_enabled = 1
    };
    pvr_init(&params);

    data->frontbuffer = vram_l; // 16-bit color depth
    data->backbuffer = malloc(640 * 480 * 2); // Allocate backbuffer

    // Initialize renderer info
    renderer->info = (SDL_RendererInfo){
        .name = "Dreamcast PVR",
        .flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE,
        .num_texture_formats = 4,
        .texture_formats = { SDL_PIXELFORMAT_BGR565, SDL_PIXELFORMAT_ABGR1555, SDL_PIXELFORMAT_ABGR4444, SDL_PIXELFORMAT_ABGR8888 },
        .max_texture_width = 1024,
        .max_texture_height = 1024
    };

    // Assign function pointers
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
    renderer->RunCommandQueue = DC_RunCommandQueue;
    renderer->RenderPresent = DC_RenderPresent;
    renderer->DestroyTexture = DC_DestroyTexture;
    renderer->DestroyRenderer = DC_DestroyRenderer;

    data->initialized = SDL_TRUE;

    LOG_FUNCTION_COMPLETE();
    return 0;
}

static int DC_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize) {
    while (cmd) {
        switch (cmd->command) {
            case SDL_RENDERCMD_SETDRAWCOLOR:
                DC_QueueSetDrawColor(renderer, cmd);
                break;
            case SDL_RENDERCMD_SETVIEWPORT:
                DC_QueueSetViewport(renderer, cmd);
                break;
            case SDL_RENDERCMD_SETCLIPRECT:
                DC_QueueSetClipRect(renderer, cmd);
                break;
            case SDL_RENDERCMD_CLEAR:
                DC_QueueClear(renderer, cmd);
                break;
            case SDL_RENDERCMD_DRAW_POINTS:
                DC_QueueDrawPoints(renderer, cmd, (const SDL_FPoint *)vertices, cmd->data.draw.count);
                break;
            case SDL_RENDERCMD_DRAW_LINES:
                DC_QueueDrawLines(renderer, cmd, (const SDL_FPoint *)vertices, cmd->data.draw.count);
                break;
            case SDL_RENDERCMD_FILL_RECTS:
                DC_QueueFillRects(renderer, cmd, (const SDL_FRect *)vertices, cmd->data.draw.count);
                break;
            case SDL_RENDERCMD_COPY:
                // DC_QueueCopy(renderer, cmd, cmd->data.copy.texture, &cmd->data.copy.srcrect, &cmd->data.copy.dstrect);
                break;
            case SDL_RENDERCMD_COPY_EX:
                // DC_QueueCopyEx(renderer, cmd, cmd->data.copy_ex.texture, &cmd->data.copy_ex.srcrect, &cmd->data.copy_ex.dstrect, cmd->data.copy_ex.angle, &cmd->data.copy_ex.center, cmd->data.copy_ex.flip);
                break;
            case SDL_RENDERCMD_GEOMETRY:
                // DC_QueueGeometry(renderer, cmd, cmd->data.geometry.texture, (const float *)vertices, cmd->data.geometry.xy_stride, (const SDL_Color *)(vertices + cmd->data.geometry.xy_stride * cmd->data.geometry.num_vertices * sizeof(float)), cmd->data.geometry.color_stride, (const float *)(vertices + cmd->data.geometry.xy_stride * cmd->data.geometry.num_vertices * sizeof(float) + cmd->data.geometry.num_vertices * sizeof(SDL_Color)), cmd->data.geometry.uv_stride, cmd->data.geometry.num_vertices, (const void *)(vertices + cmd->data.geometry.xy_stride * cmd->data.geometry.num_vertices * sizeof(float) + cmd->data.geometry.num_vertices * sizeof(SDL_Color) + cmd->data.geometry.num_vertices * sizeof(float) + cmd->data.geometry.num_indices * sizeof(float)), cmd->data.geometry.num_indices, cmd->data.geometry.size_indices);
                break;
            default:
                // SDL_assert_unreachable();
                break;
        }
        cmd = cmd->next;
    }
    return 0;
}


// Destroy renderer function
void DC_DestroyRenderer(SDL_Renderer *renderer) {
    LOG_FUNCTION_CALL();
    DC_RenderData *data = (DC_RenderData *)renderer->driverdata;
    
    if (data->initialized) {
        pvr_shutdown();
        SDL_free(data->backbuffer); // Free allocated backbuffer
        SDL_free(data);
    }
    LOG_FUNCTION_COMPLETE();
}

// Create texture function
int DC_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture) {
    LOG_FUNCTION_CALL();
    texture->driverdata = SDL_calloc(1, sizeof(pvr_ptr_t));
    if (!texture->driverdata) {
        return SDL_OutOfMemory();
    }
    
    pvr_ptr_t texture_mem = pvr_mem_malloc(texture->w * texture->h * 2);
    if (!texture_mem) {
        SDL_free(texture->driverdata);
        return SDL_OutOfMemory();
    }
    
    *(pvr_ptr_t *)texture->driverdata = texture_mem;
    LOG_FUNCTION_COMPLETE();
    return 0;
}

// Destroy texture function
void DC_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture) {
    LOG_FUNCTION_CALL();
    pvr_ptr_t texture_mem = *(pvr_ptr_t *)texture->driverdata;
    
    if (texture_mem) {
        pvr_mem_free(texture_mem);
    }
    SDL_free(texture->driverdata);
    LOG_FUNCTION_COMPLETE();
}

// Update texture function
int DC_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch) {
    LOG_FUNCTION_CALL();
    // This function would need to handle texture updates. Implementation depends on specifics of texture management.
    LOG_FUNCTION_COMPLETE();
    return 0;
}

// Set texture scaling mode function
int DC_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture, SDL_ScaleMode scaleMode) {
    LOG_FUNCTION_CALL();
    // Handle scaling mode changes here if applicable
    LOG_FUNCTION_COMPLETE();
    return 0;
}

// Set render target function
int DC_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture) {
    LOG_FUNCTION_CALL();
    DC_RenderData *data = (DC_RenderData *)renderer->driverdata;
    data->boundTarget = texture;
    LOG_FUNCTION_COMPLETE();
    return 0;
}

// Render present function
int DC_RenderPresent(SDL_Renderer *renderer) {
    LOG_FUNCTION_CALL();
    DC_RenderData *data = (DC_RenderData *)renderer->driverdata;

    if (!data->initialized) {
        return SDL_SetError("Renderer not initialized");
    }

    // Swap front and back buffers
    memcpy(data->frontbuffer, data->backbuffer, 640 * 480 * 2);
    memset(data->backbuffer, 0, 640 * 480 * 2); // Clear backbuffer
    
    // Present the backbuffer
    pvr_list_begin(PVR_LIST_TR_POLY);
    // pvr_sprite32(0, 0, 0, 640, 480, 0, 0, 0, 0);
    pvr_list_finish();
    
    LOG_FUNCTION_COMPLETE();
    return 0;
}

// Lock texture function
int DC_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, void **pixels, int *pitch) {
    LOG_FUNCTION_CALL();
    *pixels = (void *)*(pvr_ptr_t *)texture->driverdata;
    *pitch = texture->w * 2; // Assuming 16-bit color depth
    LOG_FUNCTION_COMPLETE();
    return 0;
}

// Unlock texture function
void DC_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture) {
    LOG_FUNCTION_CALL();
    // Nothing to do here for PVR renderer
    LOG_FUNCTION_COMPLETE();
}

// Additional command queue functions would be similar to the above


static int DC_QueueSetViewport(SDL_Renderer *renderer, SDL_RenderCommand *cmd) {
    LOG_FUNCTION_CALL();
    // Stub: Implement viewport logic
    LOG_FUNCTION_COMPLETE();
    return 0;
}

static int DC_QueueSetDrawColor(SDL_Renderer *renderer, SDL_RenderCommand *cmd) {
    LOG_FUNCTION_CALL();
    // Stub: Implement draw color logic
    LOG_FUNCTION_COMPLETE();
    return 0;
}

static int DC_QueueDrawPoints(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count) {
    LOG_FUNCTION_CALL();
    // Stub: Implement draw points logic
    LOG_FUNCTION_COMPLETE();
    return 0;
}

static int DC_QueueDrawLines(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count) {
    LOG_FUNCTION_CALL();
    // Stub: Implement draw lines logic
    LOG_FUNCTION_COMPLETE();
    return 0;
}

static int DC_QueueFillRects(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FRect *rects, int count) {
    LOG_FUNCTION_CALL();
    // Stub: Implement fill rects logic
    LOG_FUNCTION_COMPLETE();
    return 0;
}

static int DC_QueueCopy(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_FRect *dstrect) {
    LOG_FUNCTION_CALL();
    // Stub: Implement texture copy logic
    LOG_FUNCTION_COMPLETE();
    return 0;
}

static int DC_QueueCopyEx(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_FRect *dstrect, double angle, const SDL_FPoint *center, SDL_RendererFlip flip) {
    LOG_FUNCTION_CALL();
    // Stub: Implement texture copy with rotation and flipping
    LOG_FUNCTION_COMPLETE();
    return 0;
}

static int DC_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const float *xy, int xy_stride, const SDL_Color *color, int color_stride, const float *uv, int uv_stride, int num_vertices, const void *indices, int num_indices, int size_indices) {
    LOG_FUNCTION_CALL();
    // Stub: Implement geometry queueing
    LOG_FUNCTION_COMPLETE();
    return 0;
}

static int DC_QueueClear(SDL_Renderer *renderer, SDL_RenderCommand *cmd){
        LOG_FUNCTION_CALL();
    // Stub: Implement geometry queueing
    LOG_FUNCTION_COMPLETE();
    return 0;
}

static int DC_QueueSetClipRect(SDL_Renderer *renderer, SDL_RenderCommand *cmd){
        LOG_FUNCTION_CALL();
    // Stub: Implement geometry queueing
    LOG_FUNCTION_COMPLETE();
    return 0;
}
static void DC_RenderBegin(SDL_Renderer *renderer) {
    LOG_FUNCTION_CALL();
    DC_RenderData *data = (DC_RenderData *)renderer->driverdata;

    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_poly_hdr_t hdr;
    pvr_poly_cxt_t cxt;

    pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));

    pvr_list_begin(PVR_LIST_TR_POLY);
    LOG_FUNCTION_COMPLETE();
}

static void DC_RenderEnd(SDL_Renderer *renderer) {
    LOG_FUNCTION_CALL();
    pvr_list_finish();
    pvr_scene_finish();
    LOG_FUNCTION_COMPLETE();
}

SDL_RenderDriver DC_RenderDriver = {
    .CreateRenderer = DC_CreateRenderer, 
    .info = {
        .name = "Dreamcast PVR",
        .flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE,
        .num_texture_formats = 4,
        .texture_formats = {
            SDL_PIXELFORMAT_BGR565,
            SDL_PIXELFORMAT_ABGR1555,
            SDL_PIXELFORMAT_ABGR4444,
            SDL_PIXELFORMAT_ABGR8888
        },
        .max_texture_width = 1024,
        .max_texture_height = 1024
    }
};


#endif /* SDL_VIDEO_RENDER_DREAMCAST_PVR */
