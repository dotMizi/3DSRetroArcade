#ifndef TEST_3DS_H
#define TEST_3DS_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef uint32_t u32;typedef uint64_t u64;
typedef struct {short dx,dy;} circlePosition;
typedef struct {unsigned short px,py;} touchPosition;
enum {GFX_TOP,GFX_BOTTOM,GFX_LEFT=0};
enum {KEY_A=1,KEY_B=2,KEY_SELECT=4,KEY_START=8,KEY_RIGHT=16,KEY_LEFT=32,KEY_UP=64,KEY_DOWN=128,KEY_R=256,KEY_L=512,KEY_X=1024,KEY_Y=2048,KEY_TOUCH=4096};
void gfxInitDefault(void);void gfxSet3D(bool enabled);void gfxExit(void);bool aptMainLoop(void);void hidScanInput(void);u32 hidKeysDown(void);u32 hidKeysHeld(void);void hidCircleRead(circlePosition*p);void hidTouchRead(touchPosition*p);u64 osGetTime(void);u64 svcGetSystemTick(void);unsigned char*gfxGetFramebuffer(int screen,int side,void*w,void*h);void gfxFlushBuffers(void);void gfxSwapBuffers(void);void gspWaitForVBlank(void);

typedef int32_t Result;
#define R_FAILED(result) ((result)<0)
typedef struct {int16_t*data_pcm16;u32 nsamples;bool looping;unsigned char status;} ndspWaveBuf;
enum {NDSP_WBUF_FREE,NDSP_WBUF_QUEUED,NDSP_WBUF_PLAYING,NDSP_WBUF_DONE};
enum {NDSP_OUTPUT_STEREO,NDSP_INTERP_LINEAR,NDSP_FORMAT_MONO_PCM16};
Result ndspInit(void);void ndspExit(void);void*linearAlloc(size_t size);void linearFree(void*p);
Result DSP_FlushDataCache(const void*p,u32 bytes);void ndspSetOutputMode(int mode);void ndspSetMasterVol(float volume);
void ndspChnReset(int channel);void ndspChnSetInterp(int channel,int interp);void ndspChnSetRate(int channel,float rate);void ndspChnSetFormat(int channel,int format);void ndspChnSetMix(int channel,float mix[12]);void ndspChnWaveBufAdd(int channel,ndspWaveBuf*wave);void ndspChnWaveBufClear(int channel);
#endif
