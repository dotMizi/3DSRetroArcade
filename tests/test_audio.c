#include <3ds.h>
#include "delta_audio.h"
#include "sound_synth.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int live,allocations,failAllocation,adds,flushes,exits,initResult;
static bool dsp;
static ndspWaveBuf*queued[10];
Result ndspInit(void){dsp=initResult>=0;return initResult;}
void ndspExit(void){assert(dsp);for(int i=0;i<10;i++)assert(!queued[i]);dsp=false;exits++;}
void*linearAlloc(size_t n){allocations++;if(failAllocation==allocations)return NULL;void*p=malloc(n);assert(p);live++;return p;}
void linearFree(void*p){assert(!dsp);assert(p);free(p);live--;}
Result DSP_FlushDataCache(const void*p,u32 n){assert(p&&n>=2&&n%2==0);flushes++;return 0;}
void ndspSetOutputMode(int mode){assert(dsp&&mode==NDSP_OUTPUT_STEREO);}
void ndspSetMasterVol(float volume){assert(volume>0&&volume<1);}
void ndspChnReset(int ch){assert(ch>=0&&ch<10);}
void ndspChnSetInterp(int ch,int interp){(void)ch;assert(interp==NDSP_INTERP_LINEAR);}
void ndspChnSetRate(int ch,float rate){(void)ch;assert(rate==DELTA_SOUND_RATE);}
void ndspChnSetFormat(int ch,int format){(void)ch;assert(format==NDSP_FORMAT_MONO_PCM16);}
void ndspChnSetMix(int ch,float mix[12]){(void)ch;assert(mix[0]>0&&mix[1]>0);}
void ndspChnWaveBufAdd(int ch,ndspWaveBuf*w){assert(dsp&&w->data_pcm16&&w->nsamples);assert(!queued[ch]||queued[ch]->status==NDSP_WBUF_DONE);queued[ch]=w;w->status=NDSP_WBUF_QUEUED;adds++;}
void ndspChnWaveBufClear(int ch){if(queued[ch])queued[ch]->status=NDSP_WBUF_FREE;queued[ch]=NULL;}
static void noQueues(void){for(int i=0;i<10;i++)assert(!queued[i]);}
int main(void){
 // Actual PCM synthesis: finite, bounded, click-free endpoints and deterministic.
 for(int effect=0;effect<SFX_COUNT;effect++){
  size_t n=soundSamples(effect);int16_t*p=calloc(n+2,sizeof(*p)),*q=calloc(n+2,sizeof(*q));assert(p&&q);soundGenerate(effect,p,n+2);soundGenerate(effect,q,n+2);assert(!memcmp(p,q,(n+2)*sizeof(*p)));
  double energy=0;int peak=0;for(size_t i=0;i<n;i++){int v=abs(p[i]);if(v>peak)peak=v;energy+=(double)p[i]*p[i];}assert(energy>n*10000.0&&peak<20000&&p[0]==0&&p[n-1]==0&&p[n]==0&&p[n+1]==0);
  char path[80];snprintf(path,sizeof(path),"build-host/sfx-%d.pcm",effect);FILE*f=fopen(path,"wb");assert(f);assert(fwrite(p,sizeof(*p),n,f)==n);fclose(f);free(p);free(q);
 }
 assert(!soundSamples((SoundEffect)-1)&&!soundSamples(SFX_COUNT));
 initResult=-1;assert(!deltaAudioInit()&&!deltaAudioAvailable());Game g={0};g.playing=true;g.soundEvents=7;deltaAudioUpdate(&g,true);assert(!g.soundEvents&&adds==0);
 initResult=0;allocations=0;failAllocation=3;assert(!deltaAudioInit()&&live==0&&exits==1&&!dsp);
 allocations=0;failAllocation=0;flushes=0;assert(deltaAudioInit()&&deltaAudioAvailable()&&live==SFX_COUNT&&flushes==SFX_COUNT);
 g.thrust=true;g.soundEvents=7;deltaAudioUpdate(&g,true);assert(adds==5&&queued[0]&&queued[3]&&queued[6]&&queued[8]->looping&&queued[9]->looping&&!g.soundEvents);
 deltaAudioUpdate(&g,true);assert(adds==5);g.thrust=false;deltaAudioUpdate(&g,true);assert(!queued[8]&&queued[9]);
 for(int i=0;i<4;i++){g.soundEvents=DELTA_SOUND_PLAYER;deltaAudioUpdate(&g,true);}assert(adds==7);queued[0]->status=NDSP_WBUF_DONE;queued[0]=NULL;g.soundEvents=DELTA_SOUND_PLAYER;deltaAudioUpdate(&g,true);assert(adds==8);
 g.soundEvents=7;deltaAudioUpdate(&g,false);noQueues();assert(!g.soundEvents);deltaAudioUpdate(&g,true);assert(adds==9&&queued[9]&&!queued[0]);
 g.playing=false;deltaAudioUpdate(&g,true);noQueues();g.playing=true;g.thrust=true;deltaAudioUpdate(&g,true);assert(queued[8]&&queued[9]);deltaAudioExit();assert(!dsp&&live==0&&!deltaAudioAvailable());noQueues();int previousExits=exits;deltaAudioExit();assert(exits==previousExits);
 puts("PASS: five synthesized sounds, deterministic PCM/levels/fades, DSP failure, allocation cleanup, cache sizes, polyphony/reuse, thrust loop, background loop, inactive silence, event consumption, safe shutdown.");return 0;
}
