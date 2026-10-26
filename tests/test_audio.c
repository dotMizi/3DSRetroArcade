#include <3ds.h>
#include "delta_audio.h"
#include "sound_synth.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int live, allocations, failAllocation, adds, flushes, exits, initResult;
static bool dsp;
static float rates[19];
static ndspWaveBuf *queued[19];

Result ndspInit(void)
{
    dsp = initResult >= 0;
    return initResult;
}

void ndspExit(void)
{
    assert(dsp);
    for (int i = 0; i < 19; i++)
        assert(!queued[i]);
    dsp = false;
    exits++;
}

void *linearAlloc(size_t n)
{
    allocations++;
    if (failAllocation == allocations)
        return NULL;
    void *p = malloc(n);
    assert(p);
    live++;
    return p;
}

void linearFree(void *p)
{
    assert(!dsp);
    assert(p);
    free(p);
    live--;
}

Result DSP_FlushDataCache(const void *p, u32 n)
{
    assert(p && n >= 2 && n % 2 == 0);
    flushes++;
    return 0;
}

void ndspSetOutputMode(int mode)
{
    assert(dsp && mode == NDSP_OUTPUT_STEREO);
}

void ndspSetMasterVol(float volume)
{
    assert(volume > 0 && volume < 1);
}

void ndspChnReset(int ch)
{
    assert(ch >= 0 && ch < 19);
}

void ndspChnSetInterp(int ch, int interp)
{
    (void)ch;
    assert(interp == NDSP_INTERP_LINEAR);
}

void ndspChnSetRate(int ch, float rate)
{
    assert(ch >= 0 && ch < 19);
    if (ch == 10)
        assert(rate >= DELTA_SOUND_RATE * .64f && rate <= DELTA_SOUND_RATE * 1.06f);
    else
        assert(rate == DELTA_SOUND_RATE);
    rates[ch] = rate;
}

void ndspChnSetFormat(int ch, int format)
{
    (void)ch;
    assert(format == NDSP_FORMAT_MONO_PCM16);
}

void ndspChnSetMix(int ch, float mix[12])
{
    (void)ch;
    assert(mix[0] > 0 && mix[1] > 0);
}

void ndspChnWaveBufAdd(int ch, ndspWaveBuf *w)
{
    assert(dsp && w->data_pcm16 && w->nsamples);
    assert(!queued[ch] || queued[ch]->status == NDSP_WBUF_DONE);
    queued[ch] = w;
    w->status = NDSP_WBUF_QUEUED;
    adds++;
}

void ndspChnWaveBufClear(int ch)
{
    if (queued[ch])
        queued[ch]->status = NDSP_WBUF_FREE;
    queued[ch] = NULL;
}

static void noQueues(void)
{
    for (int i = 0; i < 19; i++)
        assert(!queued[i]);
}

int main(void)
{
    // Actual PCM synthesis: finite, bounded, click-free endpoints and deterministic.
    for (int effect = 0; effect < SFX_COUNT; effect++)
    {
        size_t n = soundSamples(effect);
        int16_t *p = calloc(n + 2, sizeof(*p)), *q = calloc(n + 2, sizeof(*q));
        assert(p && q);
        soundGenerate(effect, p, n + 2);
        soundGenerate(effect, q, n + 2);
        assert(!memcmp(p, q, (n + 2) * sizeof(*p)));
        double energy = 0;
        int peak = 0;
        for (size_t i = 0; i < n; i++)
        {
            int v = abs(p[i]);
            if (v > peak)
                peak = v;
            energy += (double)p[i] * p[i];
        }
        assert(energy > n * 10000.0 && peak < 20000 && p[0] == 0 && p[n - 1] == 0 && p[n] == 0 && p[n + 1] == 0);
        char path[80];
        snprintf(path, sizeof(path), "build-host/sfx-%d.pcm", effect);
        FILE *f = fopen(path, "wb");
        assert(f);
        assert(fwrite(p, sizeof(*p), n, f) == n);
        fclose(f);
        free(p);
        free(q);
    }
    assert(!soundSamples((SoundEffect)-1) && !soundSamples(SFX_COUNT));
    initResult = -1;
    assert(!deltaAudioInit() && !deltaAudioAvailable());
    Game g = {0};
    g.playing = true;
    g.soundEvents = 7;
    deltaAudioUpdate(&g, true);
    assert(!g.soundEvents && adds == 0);
    initResult = 0;
    allocations = 0;
    failAllocation = 3;
    assert(!deltaAudioInit() && live == 0 && exits == 1 && !dsp);
    allocations = 0;
    failAllocation = 0;
    flushes = 0;
    assert(deltaAudioInit() && deltaAudioAvailable() && live == SFX_COUNT && flushes == SFX_COUNT);
    g.thrust = true;
    g.soundEvents = 7;
    deltaAudioUpdate(&g, true);
    assert(adds == 5 && queued[0] && queued[3] && queued[6] && queued[8]->looping && queued[9]->looping &&
           !g.soundEvents);
    deltaAudioUpdate(&g, true);
    assert(adds == 5);
    g.thrust = false;
    deltaAudioUpdate(&g, true);
    assert(!queued[8] && queued[9]);
    for (int i = 0; i < 4; i++)
    {
        g.soundEvents = DELTA_SOUND_PLAYER;
        deltaAudioUpdate(&g, true);
    }
    assert(adds == 7);
    queued[0]->status = NDSP_WBUF_DONE;
    queued[0] = NULL;
    g.soundEvents = DELTA_SOUND_PLAYER;
    deltaAudioUpdate(&g, true);
    assert(adds == 8);
    g.soundEvents = 7;
    deltaAudioUpdate(&g, false);
    noQueues();
    assert(!g.soundEvents);
    deltaAudioUpdate(&g, true);
    assert(adds == 9 && queued[9] && !queued[0]);
    g.playing = false;
    deltaAudioUpdate(&g, true);
    noQueues();
    g.playing = true;
    g.thrust = true;
    deltaAudioUpdate(&g, true);
    assert(queued[8] && queued[9]);
    deltaAudioUpdate(&g, false);
    Rescue r = {0};
    r.y = RESCUE_LAND_Y;
    rescueAudioUpdate(&r, true, .016f);
    assert(queued[10] && queued[10]->looping && !queued[9]);
    float idle = rates[10];
    int loopAdds = adds;
    r.y = 100;
    rescueAudioUpdate(&r, true, .016f);
    assert(rates[10] > idle && rates[10] < DELTA_SOUND_RATE * 1.05f);
    float accelerating = rates[10];
    for (int frame = 0; frame < 180; frame++)
        rescueAudioUpdate(&r, true, .016f);
    assert(rates[10] > accelerating && rates[10] > DELTA_SOUND_RATE && adds == loopAdds);
    float flight = rates[10];
    r.y = RESCUE_LAND_Y;
    rescueAudioUpdate(&r, true, .016f);
    assert(rates[10] < flight && rates[10] > idle);
    for (int frame = 0; frame < 180; frame++)
        rescueAudioUpdate(&r, true, .016f);
    assert(fabsf(rates[10] - idle) < DELTA_SOUND_RATE * .002f);
    rescueAudioUpdate(&r, false, .016f);
    noQueues();
    rescueAudioUpdate(&r, true, .016f);
    r.win = true;
    rescueAudioUpdate(&r, true, .016f);
    noQueues();
    r.win = false;
    rescueAudioUpdate(&r, true, .016f);
    r.over = true;
    rescueAudioUpdate(&r, true, .016f);
    noQueues();
    /* Switching back to Delta leaves its own loops intact. */
    deltaAudioUpdate(&g, true);
    rescueAudioUpdate(&r, false, .016f);
    assert(queued[8] && queued[9] && !queued[10]);
    deltaAudioUpdate(&g, false);
    Shield sky = {0};
    sky.soundEvents =
        SHIELD_SOUND_LAUNCH | SHIELD_SOUND_BLAST | SHIELD_SOUND_DAMAGE | SHIELD_SOUND_BONUS | SHIELD_SOUND_WAVE;
    int beforeSky = adds;
    shieldAudioUpdate(&sky, true);
    assert(!sky.soundEvents && adds == beforeSky + 5);
    assert(queued[11] && queued[13] && queued[16] && queued[17] && queued[18]);
    shieldAudioUpdate(&sky, true);
    assert(adds == beforeSky + 5);
    sky.soundEvents = SHIELD_SOUND_LAUNCH | SHIELD_SOUND_BLAST;
    shieldAudioUpdate(&sky, true);
    assert(queued[12] && queued[14] && adds == beforeSky + 7);
    shieldAudioUpdate(&sky, false);
    noQueues();
    sky.soundEvents = SHIELD_SOUND_DAMAGE;
    shieldAudioUpdate(&sky, false);
    assert(!sky.soundEvents);
    noQueues();
    deltaAudioExit();
    assert(!dsp && live == 0 && !deltaAudioAvailable());
    noQueues();
    int previousExits = exits;
    deltaAudioExit();
    assert(exits == previousExits);
    puts(
        "PASS: eleven synthesized sounds and Sky Shield event audio and dynamic rotor acceleration/deceleration, deterministic PCM/levels/fades, DSP failure, allocation cleanup, cache sizes, polyphony/reuse, thrust loop, background loop, inactive silence, event consumption, safe shutdown.");
    return 0;
}
