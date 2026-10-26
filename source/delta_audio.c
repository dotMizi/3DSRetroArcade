#include "delta_audio.h"
#include "sound_synth.h"
#include <3ds.h>
#include <math.h>
#include <string.h>
#define VOICES 23
#define RESCUE_TALLY_VOICE 21
#define RESCUE_WAVE_VOICE 22
#define RESCUE_OUCH_VOICE 20
#define RESCUE_CRASH_VOICE 19
#define SHIELD_FIRST_VOICE 11
#define ROTOR_VOICE 10
#define BACKGROUND_VOICE 9
#define THRUST_VOICE 8
static bool started, ready, activeLast, thrustPlaying, rotorPlaying;
static bool shieldActiveLast, rescueCrashPlaying;
static float rotorRate;
static int16_t *samples[SFX_COUNT];
static ndspWaveBuf voices[VOICES];

static void stopVoices(void)
{
    for (int i = 0; i < VOICES; i++)
        ndspChnWaveBufClear(i);
    shieldActiveLast = false;
    rescueCrashPlaying = false;
    rotorPlaying = false;
    thrustPlaying = false;
    activeLast = false;
}

void deltaAudioExit(void)
{
    if (started)
    {
        stopVoices();
        ndspExit();
        started = false;
    }
    /* NDSP is shut down before its DMA-readable sample memory is released. */
    for (int i = 0; i < SFX_COUNT; i++)
    {
        if (samples[i])
            linearFree(samples[i]);
        samples[i] = NULL;
    }
    ready = false;
}

bool deltaAudioInit(void)
{
    deltaAudioExit();
    memset(voices, 0, sizeof(voices));
    if (R_FAILED(ndspInit()))
        return false;
    started = true;
    for (int i = 0; i < SFX_COUNT; i++)
    {
        size_t count = soundSamples((SoundEffect)i);
        samples[i] = linearAlloc(count * sizeof(int16_t));
        if (!samples[i])
        {
            deltaAudioExit();
            return false;
        }
        soundGenerate((SoundEffect)i, samples[i], count);
        DSP_FlushDataCache(samples[i], count * sizeof(int16_t));
    }
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspSetMasterVol(.65f);
    for (int i = 0; i < VOICES; i++)
    {
        float mix[12] = {.7f, .7f};
        ndspChnReset(i);
        ndspChnSetInterp(i, NDSP_INTERP_LINEAR);
        ndspChnSetRate(i, DELTA_SOUND_RATE);
        ndspChnSetFormat(i, NDSP_FORMAT_MONO_PCM16);
        ndspChnSetMix(i, mix);
    }
    ready = true;
    return true;
}

bool deltaAudioAvailable(void)
{
    return ready;
}

static void queueVoice(int voice, SoundEffect effect, bool loop)
{
    ndspWaveBuf *w = &voices[voice];
    memset(w, 0, sizeof(*w));
    w->data_pcm16 = samples[effect];
    w->nsamples = soundSamples(effect);
    w->looping = loop;
    ndspChnWaveBufAdd(voice, w);
}

static void oneShot(SoundEffect effect, int first, int end)
{
    for (int i = first; i < end; i++)
        if (voices[i].status == NDSP_WBUF_FREE || voices[i].status == NDSP_WBUF_DONE)
        {
            queueVoice(i, effect, false);
            return;
        }
}

void deltaAudioUpdate(Game *g, bool active)
{
    unsigned events = g->soundEvents;
    g->soundEvents = 0;
    if (!ready)
        return;
    active = active && g->playing && !g->paused;
    if (!active)
    {
        if (activeLast)
            stopVoices();
        return;
    }
    if (!activeLast)
        queueVoice(BACKGROUND_VOICE, SFX_BACKGROUND, true);
    activeLast = true;
    if (events & DELTA_SOUND_PLAYER)
        oneShot(SFX_PHEW, 0, 3);
    if (events & DELTA_SOUND_ENEMY)
        oneShot(SFX_INK, 3, 6);
    if (events & DELTA_SOUND_BOUNCE)
        oneShot(SFX_BOING, 6, 8);
    if (g->thrust && !thrustPlaying)
    {
        queueVoice(THRUST_VOICE, SFX_THRUST, true);
        thrustPlaying = true;
    }
    else if (!g->thrust && thrustPlaying)
    {
        ndspChnWaveBufClear(THRUST_VOICE);
        thrustPlaying = false;
    }
}

void rescueAudioUpdate(Rescue *r, bool active, float dt)
{
    unsigned events = r->soundEvents;
    r->soundEvents = 0;
    if (!ready)
        return;
    active = active && !r->over && (!r->win || r->tally);
    if (!active)
    {
        if (rotorPlaying)
            ndspChnWaveBufClear(ROTOR_VOICE);
        rotorPlaying = false;
        ndspChnWaveBufClear(RESCUE_CRASH_VOICE);
        rescueCrashPlaying = false;
        ndspChnWaveBufClear(RESCUE_OUCH_VOICE);
        ndspChnWaveBufClear(RESCUE_TALLY_VOICE);
        ndspChnWaveBufClear(RESCUE_WAVE_VOICE);
        return;
    }
    if (events & RESCUE_SOUND_TALLY)
        oneShot(SFX_SHIELD_BONUS, RESCUE_TALLY_VOICE, RESCUE_TALLY_VOICE + 1);
    if (events & RESCUE_SOUND_WAVE)
        oneShot(SFX_SHIELD_WAVE, RESCUE_WAVE_VOICE, RESCUE_WAVE_VOICE + 1);
    if (r->tally)
    {
        if (rotorPlaying)
            ndspChnWaveBufClear(ROTOR_VOICE);
        rotorPlaying = false;
        return;
    }
    if (events & RESCUE_SOUND_HOUSE_HIT)
        oneShot(SFX_SHIELD_DAMAGE, RESCUE_CRASH_VOICE, RESCUE_CRASH_VOICE + 1);
    if (events & RESCUE_SOUND_OUCH)
        oneShot(SFX_RESCUE_OUCH, RESCUE_OUCH_VOICE, RESCUE_OUCH_VOICE + 1);
    if (r->crashTimer > 0)
    {
        if (rotorPlaying)
            ndspChnWaveBufClear(ROTOR_VOICE);
        rotorPlaying = false;
        if (!rescueCrashPlaying)
            oneShot(SFX_SHIELD_DAMAGE, RESCUE_CRASH_VOICE, RESCUE_CRASH_VOICE + 1);
        rescueCrashPlaying = true;
        return;
    }
    rescueCrashPlaying = false;
    bool grounded = r->y >= RESCUE_LAND_Y - 3 && fabsf(r->vx) < 35 && fabsf(r->vy) < 25;
    float target = grounded ? .65f : 1.05f;
    if (!rotorPlaying)
    {
        rotorRate = target;
        ndspChnSetRate(ROTOR_VOICE, DELTA_SOUND_RATE * rotorRate);
        queueVoice(ROTOR_VOICE, SFX_ROTOR, true);
        rotorPlaying = true;
    }
    else
    {
        /* Change playback rate in place: no restart or phase jump between frames.
     */
        float blend = fminf(.1f, fmaxf(0, dt)) * 2.f;
        rotorRate += (target - rotorRate) * blend;
        ndspChnSetRate(ROTOR_VOICE, DELTA_SOUND_RATE * rotorRate);
    }
}

void shieldAudioUpdate(Shield *s, bool active)
{
    unsigned events = s->soundEvents;
    s->soundEvents = 0;
    if (!ready)
        return;
    if (!active)
    {
        if (shieldActiveLast)
            for (int i = SHIELD_FIRST_VOICE; i < RESCUE_CRASH_VOICE; i++)
                ndspChnWaveBufClear(i);
        shieldActiveLast = false;
        return;
    }
    shieldActiveLast = true;
    if (events & SHIELD_SOUND_LAUNCH)
        oneShot(SFX_SHIELD_LAUNCH, 11, 13);
    if (events & SHIELD_SOUND_BLAST)
        oneShot(SFX_SHIELD_BLAST, 13, 16);
    if (events & SHIELD_SOUND_DAMAGE)
        oneShot(SFX_SHIELD_DAMAGE, 16, 17);
    if (events & SHIELD_SOUND_BONUS)
        oneShot(SFX_SHIELD_BONUS, 17, 18);
    if (events & SHIELD_SOUND_WAVE)
        oneShot(SFX_SHIELD_WAVE, 18, 19);
}
