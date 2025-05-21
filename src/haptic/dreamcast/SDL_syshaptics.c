#include "SDL_internal.h"

#ifdef SDL_HAPTIC_DREAMCAST
#include <kos.h>
#include <dc/maple.h>
#include <dc/maple/purupuru.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_haptic.h>
#include <SDL3/SDL_joystick.h>
#include "../SDL_haptic_c.h"
#include "../SDL_syshaptic.h"


#include "../../joystick/SDL_joystick_c.h"

#define MAX_HAPTIC_DEVICES 4

typedef struct haptic_hwdata {
    SDL_HapticID instance_id;         // SDL3 object ID
    maple_device_t *device;           // KOS device pointer
    SDL_Haptic *haptic;               // Back-reference (optional)
} haptic_hwdata;



struct haptic_hweffect {
    SDL_HapticEffect effect;
    int is_running;
    uint8_t intensity;
    uint16_t length;
};

static haptic_hwdata haptic_devices[MAX_HAPTIC_DEVICES] = {
    { 0, NULL, NULL }  // SDL_HapticID = 0, device = NULL, haptic = NULL
};

static int num_haptics = 0;

static haptic_hwdata *DC_HapticByInstanceID(SDL_HapticID id)
{
    for (int i = 0; i < MAX_HAPTIC_DEVICES; ++i) {
        if (haptic_devices[i].instance_id == id) {
            return &haptic_devices[i];
        }
    }
    return NULL;
}

bool SDL_SYS_HapticInit(void)
{
    num_haptics = 0;
    for (int i = 0; i < MAX_HAPTIC_DEVICES; ++i) {
        maple_device_t *dev = maple_enum_type(i, MAPLE_FUNC_PURUPURU);
        if (dev) {
            haptic_devices[num_haptics].device = dev;
            haptic_devices[num_haptics].haptic = NULL;
            haptic_devices[num_haptics].instance_id = SDL_GetNextObjectID();
            ++num_haptics;
        }
    }
    return true;
}




int SDL_SYS_NumHaptics(void)
{
    int count = 0;
    for (int i = 0; i < MAX_HAPTIC_DEVICES; i++) {
        if (haptic_devices[i].device) count++;
    }
    return count;
}

SDL_HapticID SDL_SYS_HapticInstanceID(int index)
{
    if (index < 0 || index >= MAX_HAPTIC_DEVICES || !haptic_devices[index].device) {
        return -1;
    }
    return haptic_devices[index].instance_id;  // ✅ Return real ID
}


const char *SDL_SYS_HapticName(int index)
{
    if (index < 0 || index >= MAX_HAPTIC_DEVICES || !haptic_devices[index].device) {
        SDL_SetError("Invalid haptic device index.");
        return NULL;
    }
    return "Dreamcast Jump Pack";
}

bool SDL_SYS_HapticOpen(SDL_Haptic *haptic)
{
    haptic_hwdata *hw = DC_HapticByInstanceID(haptic->instance_id);
    if (!hw || !hw->device) {
        SDL_SetError("Haptic device not found.");
        return false;
    }

    haptic->hwdata = hw;
    hw->haptic = haptic;

    haptic->supported = SDL_HAPTIC_LEFTRIGHT;
    haptic->neffects = 1;
    haptic->nplaying = 1;
    haptic->effects = SDL_calloc(haptic->neffects, sizeof(struct haptic_effect));
    return haptic->effects != NULL;
}





int SDL_SYS_HapticMouse(void)
{
    return -1;
}


bool SDL_SYS_JoystickIsHaptic(SDL_Joystick *joystick)
{
    SDL_JoystickID instance_id = SDL_GetJoystickID(joystick);
    return (instance_id >= 0 && instance_id < MAX_HAPTIC_DEVICES &&
            haptic_devices[instance_id].device != NULL);
}

bool SDL_SYS_HapticOpenFromJoystick(SDL_Haptic *haptic, SDL_Joystick *joystick)
{
    SDL_JoystickID instance_id = SDL_GetJoystickID(joystick);
    if (instance_id < 0 || instance_id >= MAX_HAPTIC_DEVICES || !haptic_devices[instance_id].device) {
        SDL_SetError("Invalid joystick instance ID for haptics.");
        return false;
    }

    haptic->hwdata = &haptic_devices[instance_id];
    haptic_devices[instance_id].haptic = haptic;
    return true;
}


bool SDL_SYS_JoystickSameHaptic(SDL_Haptic *haptic, SDL_Joystick *joystick)
{
    return false;
}

bool SDL_SYS_HapticRunEffect(SDL_Haptic *haptic, struct haptic_effect *effect, Uint32 iterations)
{
    if (!haptic || !haptic->hwdata) {
        SDL_SetError("Invalid haptic device.");
        return false;
    }

    haptic_hwdata *hwdata = (haptic_hwdata *)haptic->hwdata;
    maple_device_t *dev = hwdata->device;
    if (!dev) {
        SDL_SetError("No haptic device.");
        return false;
    }

    if (effect && effect->hweffect) {
        effect->hweffect->is_running = 1;
    }

    if (iterations > 0) {
        return purupuru_rumble_raw(dev, 1) == 0;
    } else {
        return SDL_SYS_HapticStopEffect(haptic, effect);
    }
}

bool SDL_SYS_HapticStopEffect(SDL_Haptic *haptic, struct haptic_effect *effect)
{
    if (!haptic || !haptic->hwdata) {
        SDL_SetError("Invalid haptic device.");
        return false;
    }

    haptic_hwdata *hwdata = (haptic_hwdata *)haptic->hwdata;
    maple_device_t *dev = hwdata->device;
    if (!dev) {
        SDL_SetError("No haptic device.");
        return false;
    }

    if (effect && effect->hweffect) {
        effect->hweffect->is_running = 0;
    }

    return purupuru_rumble_raw(dev, 0) == 0;
}

void SDL_SYS_HapticDestroyEffect(SDL_Haptic *haptic, struct haptic_effect *effect)
{
    if (effect && effect->hweffect) {
        SDL_free(effect->hweffect);
        effect->hweffect = NULL;
    }
}

void SDL_SYS_HapticClose(SDL_Haptic *haptic)
{
    if (haptic) {
        haptic->hwdata = NULL;
    }
}

void SDL_SYS_HapticQuit(void)
{
    for (int i = 0; i < MAX_HAPTIC_DEVICES; ++i) {
        haptic_devices[i].device = NULL;
    }
}

bool SDL_SYS_HapticNewEffect(SDL_Haptic *haptic, struct haptic_effect *effect, const SDL_HapticEffect *base)
{
    if (!haptic || !effect || !base) {
        SDL_SetError("Invalid effect creation parameters.");
        return false;
    }

    if (base->type != SDL_HAPTIC_LEFTRIGHT) {
        SDL_SetError("Only SDL_HAPTIC_LEFTRIGHT supported.");
        return false;
    }

    effect->hweffect = (struct haptic_hweffect *)SDL_calloc(1, sizeof(struct haptic_hweffect));
    if (!effect->hweffect) {
        SDL_OutOfMemory();
        return false;
    }

    effect->hweffect->effect = *base;
    return true;
}

bool SDL_SYS_HapticUpdateEffect(SDL_Haptic *haptic, struct haptic_effect *effect, const SDL_HapticEffect *data)
{
    if (!haptic || !effect || !data) {
        SDL_SetError("Invalid update parameters.");
        return false;
    }

    const SDL_HapticConstant *constant = &data->constant;

    uint16_t length = constant->length ? constant->length : 1000;
    uint16_t intensity = constant->level ? constant->level : 100;

    purupuru_effect_t dc_effect = {
        .duration = length,
        .effect1 = intensity,
        .effect2 = 0,
        .special = 0
    };

    haptic_hwdata *hwdata = (haptic_hwdata *)haptic->hwdata;
    if (!hwdata || !hwdata->device) {
        SDL_SetError("No valid device.");
        return false;
    }

    if (purupuru_rumble(hwdata->device, &dc_effect) < 0) {
        SDL_SetError("Rumble failed.");
        return false;
    }

    if (effect->hweffect) {
        effect->hweffect->intensity = intensity;
        effect->hweffect->length = length;
    }

    return true;
}

int SDL_SYS_HapticGetEffectStatus(SDL_Haptic *haptic, struct haptic_effect *effect)
{
    return (effect && effect->hweffect && effect->hweffect->is_running);
}

bool SDL_SYS_HapticSetGain(SDL_Haptic *haptic, int gain)
{
    return true;
}

bool SDL_SYS_HapticSetAutocenter(SDL_Haptic *haptic, int autocenter)
{
    return true;
}

bool SDL_SYS_HapticPause(SDL_Haptic *haptic)
{
    return SDL_SYS_HapticStopAll(haptic);
}

bool SDL_SYS_HapticResume(SDL_Haptic *haptic)
{
    return true;
}

bool SDL_SYS_HapticStopAll(SDL_Haptic *haptic)
{
    return SDL_SYS_HapticStopEffect(haptic, NULL);
}

#endif /* SDL_HAPTIC_DREAMCAST */
