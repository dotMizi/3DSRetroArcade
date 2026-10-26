#ifndef SOUND_SYNTH_H
#define SOUND_SYNTH_H
#include <stddef.h>
#include <stdint.h>
#define DELTA_SOUND_RATE 22050

typedef enum
{
    SFX_PHEW,
    SFX_INK,
    SFX_BOING,
    SFX_THRUST,
    SFX_BACKGROUND,
    SFX_ROTOR,
    SFX_SHIELD_LAUNCH,
    SFX_SHIELD_BLAST,
    SFX_SHIELD_DAMAGE,
    SFX_SHIELD_BONUS,
    SFX_SHIELD_WAVE,
    SFX_RESCUE_OUCH,
    SFX_COUNT
} SoundEffect;

size_t soundSamples(SoundEffect effect);
void soundGenerate(SoundEffect effect, int16_t *pcm, size_t count);
#endif
