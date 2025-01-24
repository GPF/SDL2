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

#include "SDL_dreamcastevents_c.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "SDL_events.h"
#include "SDL_timer.h"
#include "SDL_mouse.h"
#include <kos.h>
#include "SDL_dreamcastvideo.h"
#include "SDL_dreamcastevents_c.h"

#define MIN_FRAME_UPDATE 16

extern unsigned int __sdl_dc_mouse_shift;

const static unsigned short sdl_key[] = {
    /*0*/    0, 0, 0, 0, SDL_SCANCODE_A, SDL_SCANCODE_B, SDL_SCANCODE_C, SDL_SCANCODE_D, SDL_SCANCODE_E, SDL_SCANCODE_F, 
             SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_I,
             SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_O, 
             SDL_SCANCODE_P, SDL_SCANCODE_Q, SDL_SCANCODE_R, SDL_SCANCODE_S, SDL_SCANCODE_T,
             SDL_SCANCODE_U, SDL_SCANCODE_V, SDL_SCANCODE_W, SDL_SCANCODE_X, SDL_SCANCODE_Y, SDL_SCANCODE_Z,
    /*1e*/   SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6, 
             SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_0,
    /*28*/   SDL_SCANCODE_RETURN, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_TAB, SDL_SCANCODE_SPACE, 
             SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS, SDL_SCANCODE_LEFTBRACKET, 
             SDL_SCANCODE_RIGHTBRACKET, SDL_SCANCODE_BACKSLASH, 0, SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_APOSTROPHE,
    /*35*/   SDL_SCANCODE_GRAVE, SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH, SDL_SCANCODE_CAPSLOCK, 
             SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4, SDL_SCANCODE_F5, SDL_SCANCODE_F6, 
             SDL_SCANCODE_F7, SDL_SCANCODE_F8, SDL_SCANCODE_F9, SDL_SCANCODE_F10, SDL_SCANCODE_F11, SDL_SCANCODE_F12,
    /*46*/   SDL_SCANCODE_PRINTSCREEN, SDL_SCANCODE_SCROLLLOCK, SDL_SCANCODE_PAUSE, SDL_SCANCODE_INSERT, 
             SDL_SCANCODE_HOME, SDL_SCANCODE_PAGEUP, SDL_SCANCODE_DELETE, SDL_SCANCODE_END, SDL_SCANCODE_PAGEDOWN, 
             SDL_SCANCODE_RIGHT, SDL_SCANCODE_LEFT, SDL_SCANCODE_DOWN, SDL_SCANCODE_UP,
    /*53*/   SDL_SCANCODE_NUMLOCKCLEAR, SDL_SCANCODE_KP_DIVIDE, SDL_SCANCODE_KP_MULTIPLY, SDL_SCANCODE_KP_MINUS, 
             SDL_SCANCODE_KP_PLUS, SDL_SCANCODE_KP_ENTER, 
             SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_3, SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_6,
    /*5f*/   SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_9, SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_PERIOD, 0 /* S3 */
};

const static unsigned short sdl_shift[] = {
    SDL_SCANCODE_LCTRL, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_LALT, 0 /* S1 */,
    SDL_SCANCODE_RCTRL, SDL_SCANCODE_RSHIFT, SDL_SCANCODE_RALT, 0 /* S2 */,
};

const static char sdl_mousebtn[] = {
    0,                     // No button maps to SDL_BUTTON_WHEELUP
    SDL_BUTTON_RIGHT ,     // Right button maps to SDL_BUTTON_RIGHT
    SDL_BUTTON_LEFT  ,     // Left button maps to SDL_BUTTON_LEFT
    SDL_BUTTON_MIDDLE      // Side button maps to SDL_BUTTON_MIDDLE
};

static void mouse_update(void) {
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_MOUSE);
    if (!dev) return;

    mouse_state_t *state = maple_dev_status(dev);
    if (!state) return;

    static int abs_x = 0, abs_y = 0;
    static Uint8 prev_buttons = 0;

    // Scale mouse movement
    float mouse_scale_x = 640.0f / 320.0f;
    float mouse_scale_y = 480.0f / 240.0f;
    
    int scaled_dx = (int)(state->dx * mouse_scale_x);
    int scaled_dy = (int)(state->dy * mouse_scale_y);
    
    abs_x += scaled_dx;
    abs_y += scaled_dy;

    // Clamp to logical screen bounds
    if (abs_x < 0) abs_x = 0;
    if (abs_y < 0) abs_y = 0;
    if (abs_x > 639) abs_x = 639;
    if (abs_y > 479) abs_y = 479;

    // Send mouse motion event
    SDL_SendMouseMotion(NULL, 0, 0, abs_x, abs_y);

    // Process button states
    Uint8 changed_buttons = state->buttons ^ prev_buttons;
    
    for (int i = 0; i < sizeof(sdl_mousebtn) / sizeof(sdl_mousebtn[0]); i++) {
        if (changed_buttons & (1 << i)) {
            SDL_SendMouseButton(NULL, 0, 
                (state->buttons & (1 << i)) ? SDL_PRESSED : SDL_RELEASED, 
                sdl_mousebtn[i]);
        }
    }

    // Handle mouse wheel separately
    if (state->dz < 0) {
        SDL_Event wheel_event = {0};
        wheel_event.type = SDL_MOUSEWHEEL;
        wheel_event.wheel.x = 0;  // No horizontal wheel movement
        wheel_event.wheel.y = 1;  // Wheel up
        SDL_PushEvent(&wheel_event);
    } else if (state->dz > 0) {
        SDL_Event wheel_event = {0};
        wheel_event.type = SDL_MOUSEWHEEL;
        wheel_event.wheel.x = 0;  // No horizontal wheel movement
        wheel_event.wheel.y = -1; // Wheel down
        SDL_PushEvent(&wheel_event);
    }

    prev_buttons = state->buttons;
}

extern int dreamcast_text_input_enabled;
static void keyboard_update(void) {
    static kbd_state_t old_state;
    kbd_state_t *state;
    maple_device_t *dev;
    int shiftkeys;
    int i;

    if (!(dev = maple_enum_type(0, MAPLE_FUNC_KEYBOARD)))
        return;

    state = maple_dev_status(dev);
    if (!state)
        return;

    shiftkeys = state->shift_keys ^ old_state.shift_keys;
    for (i = 0; i < sizeof(sdl_shift) / sizeof(sdl_shift[0]); ++i) { // Adjusted to get the count of shift keys
        if ((shiftkeys >> i) & 1) {
            SDL_Event event;
            event.type = (state->shift_keys >> i) & 1 ? SDL_KEYDOWN : SDL_KEYUP;
            event.key.keysym.scancode = sdl_shift[i]; // Use scancode
            event.key.keysym.sym = SDL_GetKeyFromScancode(sdl_shift[i]); // Get the corresponding sym if needed
            SDL_PushEvent(&event);
        }
    }

    for (i = 0; i < sizeof(sdl_key) / sizeof(sdl_key[0]); ++i) { // Adjusted to get the count of keys
        if (state->matrix[i] != old_state.matrix[i]) {
            int key = sdl_key[i];
            if (key) {
                SDL_Event event;
                if (state->matrix[i]) { // Key pressed
                    event.type = SDL_KEYDOWN;
                    event.key.keysym.scancode = key; // Use scancode
                    event.key.keysym.sym = SDL_GetKeyFromScancode(key); // Get the corresponding sym if needed
                    event.key.keysym.mod = state->shift_keys;

    // If text input mode is enabled and the key is printable, send SDL_TEXTINPUT event
    if (dreamcast_text_input_enabled) {
        char text[2] = {0};
        Uint16 modState = state->shift_keys;

        // Check for alphanumeric characters
        if (key >= SDL_SCANCODE_A && key <= SDL_SCANCODE_Z) {
            text[0] = (modState & (KMOD_LSHIFT | KMOD_RSHIFT)) ? 'A' + (key - SDL_SCANCODE_A) : 'a' + (key - SDL_SCANCODE_A);
        } else if (key >= SDL_SCANCODE_1 && key <= SDL_SCANCODE_0) {
            // Handle number keys (1-0)
            text[0] = '0' + (key - SDL_SCANCODE_1 + 1) % 10;
        } else {
            // Handle special characters using mappings
            switch (key) {
                case SDL_SCANCODE_SLASH:
                    text[0] = (modState & (KMOD_LSHIFT | KMOD_RSHIFT)) ? '?' : '/';
                    break;
                case SDL_SCANCODE_BACKSLASH:
                    text[0] = (modState & (KMOD_LSHIFT | KMOD_RSHIFT)) ? '|' : '\\';
                    break;
                case SDL_SCANCODE_COMMA:
                    text[0] = (modState & (KMOD_LSHIFT | KMOD_RSHIFT)) ? '<' : ',';
                    break;
                case SDL_SCANCODE_PERIOD:
                    text[0] = (modState & (KMOD_LSHIFT | KMOD_RSHIFT)) ? '>' : '.';
                    break;
                case SDL_SCANCODE_SEMICOLON:
                    text[0] = (modState & (KMOD_LSHIFT | KMOD_RSHIFT)) ? ':' : ';';
                    break;
                case SDL_SCANCODE_APOSTROPHE:
                    text[0] = (modState & (KMOD_LSHIFT | KMOD_RSHIFT)) ? '"' : '\'';
                    break;
                case SDL_SCANCODE_MINUS:
                    text[0] = (modState & (KMOD_LSHIFT | KMOD_RSHIFT)) ? '_' : '-';
                    break;
                case SDL_SCANCODE_EQUALS:
                    text[0] = (modState & (KMOD_LSHIFT | KMOD_RSHIFT)) ? '+' : '=';
                    break;
                // Add more special key cases as necessary
                default:
                    break; // Ignore any keys that don't require special handling
            }
        }

        // Push the TEXTINPUT event if a character was generated
        if (text[0] != 0) {
            SDL_Event text_event;
            text_event.type = SDL_TEXTINPUT;
            SDL_strlcpy(text_event.text.text, text, sizeof(text_event.text.text));
            SDL_PushEvent(&text_event);
            // printf("Pushed SDL_TEXTINPUT: char='%c'\n", text[0]);
        }
    }

                    SDL_PushEvent(&event);
                } else { // Key released
                    event.type = SDL_KEYUP;
                    event.key.keysym.scancode = key;
                    event.key.keysym.sym = SDL_GetKeyFromScancode(key);
                    SDL_PushEvent(&event);
                }

                // printf("Pushed event: type=%d, key=%d, mod=%d\n", event.type, event.key.keysym.sym, event.key.keysym.mod);
            }
        }
    }

    old_state = *state;
}

Uint32 SDL_GetTicks(void);
void DREAMCAST_PumpEvents(_THIS)
{
    static Uint32 last_time = 0;
    Uint32 now = SDL_GetTicks();
    if (now - last_time > MIN_FRAME_UPDATE) {
        keyboard_update();
        mouse_update();
        last_time = now;
    }
}

#endif /* SDL_VIDEO_DRIVER_DREAMCAST */

