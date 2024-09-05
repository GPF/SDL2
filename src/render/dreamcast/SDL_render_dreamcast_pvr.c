#include "../../SDL_internal.h"

#if SDL_VIDEO_RENDER_DREAMCAST_PVR
#include "../SDL_sysrender.h"
#include "SDL_hints.h"
#include <dc/video.h>
#include <dc/pvr.h>
#include <kos.h>
#include <stdlib.h>
#include <string.h>


/* Forward declarations */
static int DC_QueueSetViewport(SDL_Renderer *renderer, SDL_RenderCommand *cmd);
static int DC_QueueSetDrawColor(SDL_Renderer *renderer, SDL_RenderCommand *cmd);
static int DC_QueueDrawPoints(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count);
static int DC_QueueDrawLines(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count);
static int DC_QueueFillRects(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FRect *rects, int count);
static int DC_QueueCopy(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_FRect *dstrect);
static int DC_QueueCopyEx(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_FRect *dstrect, double angle, const SDL_FPoint *center, SDL_RendererFlip flip);
static int DC_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const float *xy, int xy_stride, const SDL_Color *color, int color_stride, const float *uv, int uv_stride, int num_vertices, const void *indices, int num_indices, int size_indices);

/* Define the PVR renderer */
static int DC_PixelFormatToPVR(Uint32 format);
static void DC_RenderBegin(SDL_Renderer *renderer);
static void DC_RenderEnd(SDL_Renderer *renderer);

int DC_CreateRenderer(SDL_Renderer *renderer, SDL_Window *window, Uint32 flags);
int DC_RenderPresent(SDL_Renderer *renderer);
int DC_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture);
int DC_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch);
int DC_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, void **pixels, int *pitch);
void DC_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture);
int DC_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture, SDL_ScaleMode scaleMode);
static int DC_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture);
void DC_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture);
void DC_DestroyRenderer(SDL_Renderer *renderer);
static int DC_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize);


// Structure to hold renderer data
typedef struct {
    pvr_ptr_t frontbuffer;
    pvr_ptr_t backbuffer;
    SDL_bool initialized;
    SDL_bool vsync;
    pvr_poly_cxt_t pvr_context;
    SDL_Texture *render_target;  // Track the current render target
} DC_RenderData;

SDL_RenderDriver DC_RenderDriver = {
    .CreateRenderer = DC_CreateRenderer,
    .info = {
        .name = "Dreamcast PVR",
        .flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE,
        .num_texture_formats = 6, // Updated count to include both formats
        .texture_formats = {
            SDL_PIXELFORMAT_RGB888,   // 24-bit RGB format
            SDL_PIXELFORMAT_RGB565,   // 16-bit RGB format
            SDL_PIXELFORMAT_ARGB1555, // 16-bit ARGB format
            SDL_PIXELFORMAT_ARGB4444, // 16-bit ARGB format
            SDL_PIXELFORMAT_BGR888,   // 24-bit BGR format
            SDL_PIXELFORMAT_YUY2      // YUV format if needed
        },
        .max_texture_width = 1024,
        .max_texture_height = 1024
    }
};




#define BUFFER_SIZE (640 * 480 * 2) // Example for 16-bit color depth

int DC_CreateRenderer(SDL_Renderer *renderer, SDL_Window *window, Uint32 flags) {
    DC_RenderData *data;
    
    if (flags & SDL_RENDERER_SOFTWARE) {
        SDL_Log("DC_CreateRenderer: Rejecting software rendering.");
        return -1; 
    }

    data = (DC_RenderData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return SDL_OutOfMemory();
    }

    renderer->driverdata = data;

    // Initialize PVR
    pvr_init_params_t pvr_params = {
        { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0 },
        512 * 1024
    };
    pvr_init(&pvr_params);

    // Set up buffer pointers
    // data->frontbuffer = SDL_malloc(BUFFER_SIZE);
    // data->backbuffer = SDL_malloc(BUFFER_SIZE);

    // if (!data->frontbuffer || !data->backbuffer) {
    //     SDL_free(data->frontbuffer);
    //     SDL_free(data->backbuffer);
    //     SDL_free(data);
    //     return SDL_OutOfMemory();
    // }

    // Configure function pointers
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
    renderer->RunCommandQueue = DC_RunCommandQueue;
    renderer->info = DC_RenderDriver.info;

    data->initialized = SDL_TRUE;

    return 0;
}




void DC_DestroyRenderer(SDL_Renderer *renderer) {
    DC_RenderData *data = renderer->driverdata;
    
    if (data->initialized) {
        pvr_shutdown(); // Shutdown PVR
        SDL_free(data); // Free allocated memory
    }
}

int DC_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture) {
    DC_RenderData *data = renderer->driverdata;
    pvr_ptr_t texture_mem;
    int pixel_mode = 0;

    /* Set PVR format based on SDL texture format */
    switch (texture->format) {
        case SDL_PIXELFORMAT_RGB555:
        case SDL_PIXELFORMAT_RGB888:
            // Fallback to RGB565 for unsupported formats
            pixel_mode = PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED;
            texture->pitch = texture->w * 2;
            break;
        case SDL_PIXELFORMAT_RGBA8888:
            pixel_mode = PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED;
            texture->pitch = texture->w * 4;
            break;
        default:
            return SDL_SetError("Unsupported texture format.");
    }

    // Allocate texture memory in VRAM
    texture_mem = pvr_mem_malloc(texture->h * texture->pitch);
    if (!texture_mem) {
        return SDL_OutOfMemory();
    }

    texture->driverdata = (void *)texture_mem;
    return 0;
}


void DC_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture) {
    if (texture->driverdata) {
        pvr_mem_free((pvr_ptr_t)texture->driverdata);
        SDL_free(texture->driverdata);
        texture->driverdata = NULL;
    }
}

int DC_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch) {
    DC_RenderData *data = renderer->driverdata;
    pvr_ptr_t texture_mem = (pvr_ptr_t)texture->driverdata;

    // Corrected pvr_txr_load usage
    pvr_txr_load(pixels, texture_mem, texture->pitch * texture->h);
    return 0;
}


int DC_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, void **pixels, int *pitch) {
    *pixels = (void *)((char *)texture->driverdata + rect->y * texture->pitch + rect->x * SDL_BYTESPERPIXEL(texture->format));
    *pitch = texture->pitch;
    return 0;
}

void DC_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture) {
    /* No specific operation for unlocking, textures are updated directly in VRAM */
}

int DC_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture, SDL_ScaleMode scaleMode) {
    DC_RenderData *data = renderer->driverdata;
    pvr_poly_cxt_t *pvr_context = &data->pvr_context;  // Use a temporary context in the render data

    /* Set PVR texture filtering based on scale mode */
    switch (scaleMode) {
        case SDL_ScaleModeNearest:
            pvr_poly_cxt_txr(pvr_context, PVR_LIST_OP_POLY, PVR_TXRFMT_NONTWIDDLED, texture->w, texture->h, texture->driverdata, PVR_FILTER_NEAREST);
            break;
        case SDL_ScaleModeLinear:
            pvr_poly_cxt_txr(pvr_context, PVR_LIST_OP_POLY, PVR_TXRFMT_NONTWIDDLED, texture->w, texture->h, texture->driverdata, PVR_FILTER_BILINEAR);
            break;
        default:
            return SDL_SetError("Unsupported scale mode.");
    }

    // This will set the current scale mode for the texture, but we don’t need to store it in the texture object.
    // You can apply it during rendering as needed.
    
    return 0;
}


static int DC_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture) {
    DC_RenderData *data = renderer->driverdata;

    if (texture) {
        // Redirect rendering to a texture
        pvr_scene_begin_txr((pvr_ptr_t)texture->driverdata, &texture->w, &texture->h);
        data->render_target = texture;
    } else {
        // Reset to screen rendering
        pvr_scene_begin();
        data->render_target = NULL;
    }

    return 0;
}

int DC_RenderPresent(SDL_Renderer *renderer) {
    DC_RenderData *data = (DC_RenderData *)renderer->driverdata;

    /* Finish the scene rendering */
    pvr_scene_finish();

    /* VSync handling */
    if (data->vsync) {
        pvr_wait_ready();
    }

    /* Begin a new scene */
    pvr_scene_begin();

    return 0;
}

/* Stubbed functions for rendering commands */

static int DC_QueueSetViewport(SDL_Renderer *renderer, SDL_RenderCommand *cmd) {
    // pvr_poly_cxt_t *pvr_context = &((DC_RenderData *)renderer->driverdata)->pvr_context;
    
    // const SDL_Rect *viewport = &cmd->data.viewport.rect;
    
    // // Set the clipping mode to inside the viewport area
    // pvr_context->gen.clip_mode = PVR_USERCLIP_INSIDE; // Assuming clipping inside the viewport
    
    // // Set horizontal clipping area
    // pvr_set_clip_area(PVR_PCLIP_X, viewport->x, viewport->x + viewport->w);
    
    // // Set vertical clipping area
    // pvr_set_clip_area(PVR_PCLIP_Y, viewport->y, viewport->y + viewport->h);

    // // Apply the updated context settings
    // pvr_poly_compile(pvr_context, PVR_LIST_TR_POLY);

    return 0;
}


static int DC_QueueSetDrawColor(SDL_Renderer *renderer, SDL_RenderCommand *cmd) {
    // Check that the command type is SDL_RENDERCMD_SETDRAWCOLOR
    if (cmd->command != SDL_RENDERCMD_SETDRAWCOLOR) {
        return -1;  // Not the correct command type, handle error if needed
    }
    // pvr_set_pal_format(PVR_PAL_ARGB8888);
    // Extract the color values from the command data
    Uint8 r = cmd->data.draw.r;
    Uint8 g = cmd->data.draw.g;
    Uint8 b = cmd->data.draw.b;
    Uint8 a = cmd->data.draw.a;  // Alpha value if needed, otherwise can ignore

    // Access PVR context from renderer data
    pvr_poly_cxt_t *pvr_context = &((DC_RenderData *)renderer->driverdata)->pvr_context;

    // Convert SDL color to PVR format (assumed RGB565 for example)
    // pvr_context->txr2.enable = PVR_TXRENA;  // Enable texturing if needed
    // pvr_context->txr2.rgb = (r & 0xF8) | ((g & 0xFC) >> 3) | ((b & 0xF8) << 8);  // Example for RGB565

    return 0;
}



static int DC_QueueDrawPoints(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count) {
    int i;
    pvr_vertex_t vert;
    
    pvr_list_begin(PVR_LIST_OP_POLY); // Begin vertex list

    for (i = 0; i < count; ++i) {
        memset(&vert, 0, sizeof(vert));
        vert.x = points[i].x;
        vert.y = points[i].y;
        vert.z = 1.0f;
        vert.argb = PVR_PACK_COLOR(255, 255, 255, 255); // White point for now

        pvr_prim(&vert, sizeof(vert));
    }
    
    pvr_list_finish(); // Finish the list
    return 0;
}


static int DC_QueueDrawLines(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count) {
    int i;
    pvr_vertex_t vert;

    pvr_list_begin(PVR_LIST_OP_POLY); // Begin vertex list

    for (i = 0; i < count - 1; i += 2) {
        memset(&vert, 0, sizeof(vert));

        vert.x = points[i].x;
        vert.y = points[i].y;
        vert.z = 1.0f;
        vert.argb = PVR_PACK_COLOR(255, 255, 255, 255); // White color for now
        pvr_prim(&vert, sizeof(vert));

        vert.x = points[i + 1].x;
        vert.y = points[i + 1].y;
        pvr_prim(&vert, sizeof(vert));
    }

    pvr_list_finish(); // Finish the list
    return 0;
}

static int DC_QueueFillRects(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FRect *rects, int count) {
    int i;
    pvr_vertex_t vert;

    pvr_list_begin(PVR_LIST_OP_POLY); // Begin vertex list

    for (i = 0; i < count; ++i) {
        float x = rects[i].x;
        float y = rects[i].y;
        float w = rects[i].w;
        float h = rects[i].h;

        // Define the 4 vertices of the rectangle
        memset(&vert, 0, sizeof(vert));

        vert.x = x;
        vert.y = y;
        vert.z = 1.0f;
        vert.argb = PVR_PACK_COLOR(255, 255, 255, 255); // White color for now
        pvr_prim(&vert, sizeof(vert));

        vert.x = x + w;
        vert.y = y;
        pvr_prim(&vert, sizeof(vert));

        vert.x = x + w;
        vert.y = y + h;
        pvr_prim(&vert, sizeof(vert));

        vert.x = x;
        vert.y = y + h;
        pvr_prim(&vert, sizeof(vert));
    }

    pvr_list_finish(); // Finish the list
    return 0;
}


static int DC_QueueCopy(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_FRect *dstrect) {
    DC_RenderData *data = renderer->driverdata;
    pvr_vertex_t vert;

    pvr_list_begin(PVR_LIST_TR_POLY); // Begin transparent poly list

    // Setup the textured quad based on the source and destination rectangles
    // Assume `texture->driverdata` points to VRAM location of the texture
    float u0 = (float)srcrect->x / texture->w;
    float v0 = (float)srcrect->y / texture->h;
    float u1 = (float)(srcrect->x + srcrect->w) / texture->w;
    float v1 = (float)(srcrect->y + srcrect->h) / texture->h;

    float x0 = dstrect->x;
    float y0 = dstrect->y;
    float x1 = dstrect->x + dstrect->w;
    float y1 = dstrect->y + dstrect->h;

    memset(&vert, 0, sizeof(vert));
    
    // Vertex 1
    vert.x = x0;
    vert.y = y0;
    vert.z = 1.0f;
    vert.u = u0;
    vert.v = v0;
    vert.argb = PVR_PACK_COLOR(255, 255, 255, 255);  // White color for now
    pvr_prim(&vert, sizeof(vert));

    // Vertex 2
    vert.x = x1;
    vert.y = y0;
    vert.u = u1;
    vert.v = v0;
    pvr_prim(&vert, sizeof(vert));

    // Vertex 3
    vert.x = x1;
    vert.y = y1;
    vert.u = u1;
    vert.v = v1;
    pvr_prim(&vert, sizeof(vert));

    // Vertex 4
    vert.x = x0;
    vert.y = y1;
    vert.u = u0;
    vert.v = v1;
    pvr_prim(&vert, sizeof(vert));

    pvr_list_finish(); // Finish the list
    return 0;
}


static int DC_QueueCopyEx(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_FRect *dstrect, double angle, const SDL_FPoint *center, SDL_RendererFlip flip) {
    /* Copy textures with transformations (stubbed) */
    return 0;
}

static int DC_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const float *xy, int xy_stride, const SDL_Color *color, int color_stride, const float *uv, int uv_stride, int num_vertices, const void *indices, int num_indices, int size_indices) {
    /* Geometry rendering (stubbed) */
    return 0;
}

static int DC_RenderSetClipRect(SDL_Renderer *renderer, SDL_RenderCommand *cmd){
    return 0;
}

static int DC_RenderClear(SDL_Renderer *renderer, SDL_RenderCommand *cmd){
    return 0;
}

static int DC_RenderSetViewport(SDL_Renderer *renderer, SDL_RenderCommand *cmd){
    return 0;
}

static int DC_RenderGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void* vertices){
    return 0;
}

static int DC_RenderPrimitive(SDL_Renderer *renderer, void* V, void *vertices, SDL_RenderCommand *cmd, void* P){
    return 0;
}

static int DC_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize)
{
    while (cmd) {
        switch (cmd->command) {
            case SDL_RENDERCMD_SETVIEWPORT:
                DC_RenderSetViewport(renderer, cmd);
                break;
                
            case SDL_RENDERCMD_SETCLIPRECT:
                DC_RenderSetClipRect(renderer, cmd);
                break;
                
            case SDL_RENDERCMD_SETDRAWCOLOR:
                /* This is a no-op in the Dreamcast driver, since the color is set with every primitive. */
                break;
                
            case SDL_RENDERCMD_CLEAR:
                DC_RenderClear(renderer, cmd);
                break;
                
            case SDL_RENDERCMD_DRAW_POINTS:
                DC_RenderPrimitive(renderer, PVR_CMD_VERTEX, vertices, cmd, PVR_LIST_OP_POLY);
                break;
                
            case SDL_RENDERCMD_DRAW_LINES:
                DC_RenderPrimitive(renderer, PVR_CMD_VERTEX, vertices, cmd, PVR_LIST_OP_POLY);
                break;
                
            case SDL_RENDERCMD_FILL_RECTS:
                DC_RenderPrimitive(renderer, PVR_CMD_VERTEX, vertices, cmd, PVR_LIST_OP_POLY);
                break;
                
            case SDL_RENDERCMD_COPY:
                /* Not used for hardware-accelerated renderers */
                break;
                
            case SDL_RENDERCMD_COPY_EX:
                /* Not used for hardware-accelerated renderers */
                break;
                
            case SDL_RENDERCMD_GEOMETRY:
                DC_RenderGeometry(renderer, vertices, cmd);
                break;
                
            case SDL_RENDERCMD_NO_OP:
                /* No operation command; just skip it */
                break;
                
            default:
                /* In case of an unhandled command, we can log or handle it as needed. */
                SDL_Log("Unhandled render command: %d", cmd->command);
                break;
        }
        
        /* Move to the next command */
        cmd = cmd->next;
    }

    /* Complete the rendering (synchronize with the GPU) */
    // pvr_scene_finish();
    return 0;
}

#endif /* SDL_VIDEO_RENDER_DREAMCAST_PVR */
