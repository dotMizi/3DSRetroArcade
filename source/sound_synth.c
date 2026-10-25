#include "sound_synth.h"
#include <math.h>
#include <stdbool.h>
#define TAU 6.28318530718f
size_t soundSamples(SoundEffect effect){
 static const float duration[SFX_COUNT]={.19f,.12f,.42f,1.f,.8f};
 return effect>=0&&effect<SFX_COUNT?(size_t)(duration[effect]*DELTA_SOUND_RATE):0;
}
void soundGenerate(SoundEffect effect,int16_t*pcm,size_t count){
 uint32_t rng=0x234a091u;float phase=0,filter=0;size_t samples=soundSamples(effect);
 if(!pcm)return;
 for(size_t i=0;i<count;i++){
  if(i>=samples||!samples){pcm[i]=0;continue;}
  float t=i/(float)DELTA_SOUND_RATE,u=i/(float)(samples-1);
  rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;
  float noise=(rng&65535)/32767.5f-1;filter+=.16f*(noise-filter);
  float freq=0,v=0,env=fminf(1,t/.003f)*fminf(1,(samples-1-i)/(float)(DELTA_SOUND_RATE*.008f));
  switch(effect){
   case SFX_PHEW:
    freq=180+1400*expf(-20*t);phase+=TAU*freq/DELTA_SOUND_RATE;
    v=(sinf(phase)*.85f+filter*.55f)*expf(-12*t)*9500;break;
   case SFX_INK:
    freq=640+850*sinf(u*3.14159265359f);phase+=TAU*freq/DELTA_SOUND_RATE;
    v=(sinf(phase+1.7f*sinf(phase*1.61f))*.75f+sinf(phase*2.07f)*.25f)*expf(-23*t)*7200;break;
   case SFX_BOING:
    freq=100+280*expf(-23*t)+32*sinf(TAU*19*t)*expf(-8*t);phase+=TAU*freq/DELTA_SOUND_RATE;
    v=(sinf(phase)+.22f*sinf(phase*2))*expf(-8*t)*10500;break;
   case SFX_THRUST:
    /* Broadband hiss with a low rumble; short edge fades hide the loop join. */
    v=(noise*.48f+filter*1.5f+sinf(TAU*58*t)*.12f)*2700;break;
   case SFX_BACKGROUND:{
    /* Original four-pulse arcade ostinato: low BAP, bright DAT, repeat. */
    int beat=(int)(i/(DELTA_SOUND_RATE/5));
    float bt=(i%(DELTA_SOUND_RATE/5))/(float)DELTA_SOUND_RATE;
    bool bright=(beat&1)!=0;
    freq=bright?196.f:130.8128f;phase+=TAU*freq/DELTA_SOUND_RATE;
    float note=fminf(1,bt/.002f)*expf(-(bright?38.f:25.f)*bt);
    v=(sinf(phase)+.35f*sinf(phase*2)+(bright?.22f*sinf(phase*4):0))*note*2600;
    break;
   }
   default:break;
  }
  v*=env;if(v>32767)v=32767;if(v< -32768)v=-32768;pcm[i]=(int16_t)v;
 }
}
