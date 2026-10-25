#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>
unsigned char splash_bin[288000];
#define main arcadeAppMain
#include "../source/main.c"
#undef main
/* App-flow tests substitute the hardware audio driver; its real logic has separate NDSP tests. */
bool deltaAudioInit(void){return true;}
bool deltaAudioAvailable(void){return true;}
void deltaAudioUpdate(Game*g,bool active){(void)active;g->soundEvents=0;}
void deltaAudioExit(void){}
static unsigned char topBuffer[400*240*3+32],bottomBuffer[320*240*3+32];
static int frame;static float exitTime;
void gfxInitDefault(void){memset(topBuffer,0xa5,sizeof(topBuffer));memset(bottomBuffer,0xa5,sizeof(bottomBuffer));}
void gfxSet3D(bool enabled){assert(!enabled);}void gfxExit(void){}
bool aptMainLoop(void){return ++frame<=15;}void hidScanInput(void){}
u32 hidKeysDown(void){switch(frame){case 2:return KEY_TOUCH;case 3:return KEY_START;case 5:return KEY_B;case 6:return KEY_SELECT;case 7:return KEY_RIGHT;case 8:return KEY_A;case 9:return KEY_SELECT;case 10:return KEY_START;case 11:return KEY_A;default:return 0;}}
u32 hidKeysHeld(void){return hidKeysDown();}
void hidCircleRead(circlePosition*p){p->dx=p->dy=0;}
void hidTouchRead(touchPosition*p){p->px=50;p->py=110;}
u64 osGetTime(void){return 42;}u64 svcGetSystemTick(void){return(u64)frame*4468724;}
unsigned char*gfxGetFramebuffer(int screen,int side,void*w,void*h){(void)side;(void)w;(void)h;return screen==GFX_TOP?topBuffer+16:bottomBuffer+16;}
void gfxFlushBuffers(void){}void gspWaitForVBlank(void){}
static void guards(void){for(int i=0;i<16;i++){assert(topBuffer[i]==0xa5&&topBuffer[sizeof(topBuffer)-1-i]==0xa5);assert(bottomBuffer[i]==0xa5&&bottomBuffer[sizeof(bottomBuffer)-1-i]==0xa5);}}
static void ppm(const char*name,unsigned char*data,int w){FILE*f=fopen(name,"wb");assert(f);fprintf(f,"P6\n%d 240\n255\n",w);for(int y=0;y<240;y++)for(int x=0;x<w;x++){int p=(x*240+239-y)*3;fputc(data[p+2],f);fputc(data[p+1],f);fputc(data[p],f);}fclose(f);}
static void capture(const char*name){char path[64];snprintf(path,sizeof(path),"%s-top.ppm",name);ppm(path,topBuffer+16,400);snprintf(path,sizeof(path),"%s-bottom.ppm",name);ppm(path,bottomBuffer+16,320);}
void gfxSwapBuffers(void){guards();if(frame==1)capture("menu");if(frame==2)assert(app.screen==DELTA_INTRO&&app.mode==0);if(frame==3)exitTime=app.introElapsed;if(frame==4)assert(app.introElapsed==exitTime);if(frame==3||frame==4)assert(app.exitDialog);if(frame==5)assert(!app.exitDialog);if(frame==6||frame==9)assert(app.screen==MENU);if(frame==8)assert(app.mode==1&&app.screen==PLAY);}
static void event(App*a,u32 down,int x,int y){appUpdate(a,down,down,(circlePosition){0,0},(touchPosition){x,y},.016f,42);}
static void render(App*a,const char*name){appRender(a,topBuffer+16,bottomBuffer+16);guards();capture(name);}
static void testStartRelease(void){
 for(int game=0;game<2;game++)for(int route=0;route<2;route++)for(int touch=0;touch<2;touch++){
  /* Score lists use A; touch launch covers the menu and Delta intro. */
  if(route&&touch)continue;
  App a;appInit(&a);a.mode=game;a.selected=game;a.screen=route?SCORE_LIST:MENU;
  u32 key=touch?KEY_TOUCH:KEY_A;
  if(route)event(&a,key,0,0);
  else if(game==0){event(&a,KEY_A,0,0);a.introElapsed=100;event(&a,key,150,220);}
  else event(&a,key,151,110);
  assert(a.screen==PLAY);a.delta.shield=10;a.sky.spawn=100;
  int ammo=a.sky.ammo[0]+a.sky.ammo[1]+a.sky.ammo[2];
  touchPosition target=game?(touchPosition){150,110}:(touchPosition){200,205};
  for(int i=0;i<30;i++){
   appUpdate(&a,0,key,(circlePosition){0,0},target,.016f,42);
   if(game)assert(a.sky.ammo[0]+a.sky.ammo[1]+a.sky.ammo[2]==ammo);
   else assert(!(a.delta.soundEvents&DELTA_SOUND_PLAYER)&&a.delta.cooldown<=0);
  }
  appUpdate(&a,0,0,(circlePosition){0,0},target,.016f,42);
  assert(a.blockedPlayKeys==0);
  appUpdate(&a,key,key,(circlePosition){0,0},target,.016f,42);
  if(game)assert(a.sky.ammo[0]+a.sky.ammo[1]+a.sky.ammo[2]==ammo-1);
  else assert((a.delta.soundEvents&DELTA_SOUND_PLAYER)&&a.delta.cooldown>0);
 }
}
int main(void){
 mkdir("sdmc:",0777);mkdir("sdmc:/3ds",0777);mkdir("sdmc:/3ds/RetroArcade",0777);mkdir("sdmc:/3ds/retro-arcade",0777);
 remove("sdmc:/3ds/RetroArcade/scores-v2.txt.tmp");remove("sdmc:/3ds/RetroArcade/scores-v2.txt.bak");remove(savePath);FILE*legacy=fopen("sdmc:/3ds/RetroArcade/highscores.txt","w");assert(legacy);fputs("100 200 300\n",legacy);fclose(legacy);
 const char*expected="IN THE YEAR 2187, THE DELTA SYSTEM WAS CREATED TO PROTECT THE COLONIES OF MANKIND. CONTROLLED BY AN ADVANCED ARTIFICIAL INTELLIGENCE, ITS DEFENSE FORCES WERE THOUGHT INVINCIBLE. BUT DELTA HAS TURNED AGAINST ITS MAKERS. NOW ONLY THE BRAVEST PILOTS DARE TO ENTER THE COMBAT ZONE. AGAINST ENDLESS WAVES OF DEADLY MACHINES, ONE PILOT MUST SURVIVE. THE FATE OF MANKIND RESTS UPON SKILL, COURAGE AND SPEED. THE FINAL BATTLE HAS BEEN GIVEN A CODE NAME... DELTA-RACE";
 char normalized[1024];int at=0;for(const char*p=deltaIntroText;*p;p++){char c=*p=='\n'?' ':*p;if(c==' '&&at&&normalized[at-1]==' ')continue;normalized[at++]=c;}normalized[at]=0;assert(!strcmp(expected,normalized));
 assert(deltaIntroVisible(0)==0&&deltaIntroVisible(1)==35&&!deltaIntroComplete(1)&&deltaIntroComplete(100));
 assert(strstr(deltaIntroText,"DELTA-RACE")&&deltaIntroStartHit(150,220)&&!deltaIntroStartHit(150,190));
 assert(scoreKeyHit(10,65)==0&&scoreKeyHit(202,161)==36&&scoreKeyHit(38,65)==-1&&scoreKeyHit(319,180)==-1);
 App a;appInit(&a);assert(scoresBest(&a.scores,0)==100&&scoresBest(&a.scores,2)==300);assert(!fopen(savePath,"r"));
 FILE*f=fopen("../data/splash.bin","rb");assert(f);assert(fread(splash_bin,1,sizeof(splash_bin),f)==sizeof(splash_bin));fclose(f);gfxInitDefault();
 for(int game=0;game<3;game++){
  a.screen=MENU;a.mode=-1;a.selected=game;event(&a,KEY_A,0,0);
  if(game==0){
   assert(a.screen==DELTA_INTRO&&a.introElapsed==0);render(&a,"delta-intro-empty");for(int px=0;px<400*240*3;px++)assert(topBuffer[16+px]==0);for(int px=0;px<320*240*3;px++)assert(bottomBuffer[16+px]==0);
   event(&a,KEY_A,0,0);assert(a.screen==DELTA_INTRO);
   for(int i=0;i<150;i++)event(&a,0,0,0);render(&a,"delta-intro-partial");for(int px=0;px<320*240*3;px++)assert(bottomBuffer[16+px]==0);
   for(int i=0;i<1000;i++)event(&a,0,0,0);
   assert(deltaIntroComplete(a.introElapsed)&&a.screen==DELTA_INTRO);render(&a,"delta-intro-ready");for(int px=0;px<400*240*3;px++)assert(topBuffer[16+px]==0||topBuffer[16+px]==255);
   event(&a,KEY_TOUCH,150,220);
  }
  assert(a.mode==game&&a.screen==PLAY);render(&a,game==0?"delta":game==1?"sky":"rescue");
  if(game==0){a.delta.score=1500;a.delta.playing=false;}
  if(game==1){a.sky.score=2500;a.sky.over=true;}
  if(game==2){a.rescue.score=3500;a.rescue.win=true;}
  event(&a,0,0,0);if(game==0)assert(!fopen(savePath,"r"));assert(a.screen==ENTER_NAME&&a.pendingRank==0&&a.cursor==0);
  char prefix[32];snprintf(prefix,sizeof(prefix),"entry-%d",game);render(&a,prefix);
  event(&a,KEY_TOUCH,10+9*30+12,65+2*28+12); // 3
  event(&a,KEY_TOUCH,10+6*30+12,65+3*28+12); // space
  event(&a,KEY_TOUCH,10+5*30+12,65+2*28+12); // Z
  assert(!strcmp(a.initials,"3 Z")&&a.cursor==0);event(&a,KEY_B,0,0);assert(!strcmp(a.initials,"3  ")&&a.cursor==2);event(&a,KEY_TOUCH,10+5*30+12,65+2*28+12);event(&a,KEY_Y,0,0);assert(a.screen==SCORE_LIST&&!a.dirty&&!a.saveFailed);
  assert(!strcmp(a.scores.entries[game][0].name,"3 Z"));int count=a.scores.count[game];
  for(int i=0;i<30;i++)event(&a,0,0,0);assert(a.screen==SCORE_LIST&&a.scores.count[game]==count);
  snprintf(prefix,sizeof(prefix),"scores-%d",game);render(&a,prefix);
  event(&a,KEY_SELECT,0,0);assert(a.screen==MENU);event(&a,KEY_TOUCH,50+game*101,110);if(game==0){assert(a.screen==DELTA_INTRO);a.introElapsed=100;}else assert(a.screen==SCORE_LIST);
  event(&a,KEY_A,0,0);assert(a.screen==PLAY);assert((game==0?a.delta.score:game==1?a.sky.score:a.rescue.score)==0);
 }
 // Incoming trails stay on the main screen; the touch pad never reveals threats.
 a.mode=1;a.screen=PLAY;shieldStart(&a.sky,42,0);a.sky.remaining=0;
 a.sky.missiles[0]=(Missile){100,110,100,215,25,true};a.sky.trailStartX[0]=100;
 render(&a,"sky-trails");
 int mainPoint=(100*240+239-50)*3,radarPoint=(85*240+239-75)*3;
 assert(topBuffer[16+mainPoint]==0x7b&&topBuffer[16+mainPoint+1]==0x54&&topBuffer[16+mainPoint+2]==0xff);
 assert(bottomBuffer[16+radarPoint]!=0x7b);
 unsigned char hiddenThreats[320*240*3];memcpy(hiddenThreats,bottomBuffer+16,sizeof(hiddenThreats));
 a.sky.missiles[0].x=250;a.sky.missiles[0].y=90;a.sky.blasts[1]=(Blast){140,130,.3f,true};
 appRender(&a,topBuffer+16,bottomBuffer+16);guards();assert(!memcmp(hiddenThreats,bottomBuffer+16,sizeof(hiddenThreats)));
 a.sky.missiles[0].x=100;a.sky.missiles[0].y=110;memset(a.sky.blasts,0,sizeof(a.sky.blasts));
 a.sky.blasts[0]=(Blast){100,110,.3f,true};shieldStep(&a.sky,.01f);assert(!a.sky.missiles[0].alive);render(&a,"sky-trail-exploded");
 assert(topBuffer[16+mainPoint]!=0x7b&&bottomBuffer[16+radarPoint]!=0x7b);
 shieldStart(&a.sky,42,0);a.sky.remaining=1;a.sky.spawn=100;
 unsigned char cityStages[6]={CITY_DOMED,CITY_EXPOSED,CITY_RUIN,CITY_GONE,CITY_DOMED,CITY_EXPOSED};memcpy(a.sky.city,cityStages,sizeof(cityStages));
 render(&a,"sky-city-states");a.sky.remaining=0;a.sky.ammo[0]=10;a.sky.ammo[1]=8;a.sky.ammo[2]=9;
 shieldStep(&a.sky,.01f);assert(a.sky.tally&&a.sky.score==0);render(&a,"sky-tally-zero");
 for(int frame=0;frame<240;frame++){shieldStep(&a.sky,.01f);}assert(a.sky.score>0&&a.sky.score<5770);render(&a,"sky-tally-counting");
 while(a.sky.tallyRow<4)shieldStep(&a.sky,.01f);
 assert(a.sky.score==5770&&a.sky.bonusPaid==5770);render(&a,"sky-wave-tally");
 App reloaded;appInit(&reloaded);assert(reloaded.scores.count[0]==2&&reloaded.scores.entries[2][0].score==3500);
 // A full list rejects a tied last-place score; rejected runs never modify it.
 for(int i=0;i<12;i++)scoresInsert(&a.scores,0,10000+i*100,"TOP");a.mode=0;Scores before=a.scores;appFinish(&a,1);assert(a.screen==SCORE_LIST&&!memcmp(&before,&a.scores,sizeof(before)));render(&a,"scores-full");
 // SELECT commits the visible three characters, and save failures can be retried.
 a.mode=1;appFinish(&a,9000);memcpy(a.initials,"A 9",4);const char*normal=savePath;savePath="missing-dir/scores.txt";event(&a,KEY_SELECT,0,0);assert(a.screen==MENU&&a.dirty&&a.saveFailed);int n=a.scores.count[1];savePath=normal;event(&a,KEY_Y,0,0);assert(!a.dirty&&a.scores.count[1]==n);
 // Exit dialog freezes play, can be cancelled, and saves pending initials.
 a.mode=2;appFinish(&a,8000);memcpy(a.initials,"END",4);event(&a,KEY_START,0,0);assert(a.exitDialog&&!a.quit);event(&a,KEY_B,0,0);assert(!a.exitDialog&&a.screen==ENTER_NAME);event(&a,KEY_START,0,0);savePath="missing-dir/scores.txt";event(&a,KEY_A,0,0);assert(!a.quit&&a.exitDialog&&a.saveFailed&&a.screen==SCORE_LIST);int savedCount=a.scores.count[2];savePath=normal;event(&a,KEY_A,0,0);assert(a.quit&&!a.dirty&&a.scores.count[2]==savedCount);appInit(&reloaded);assert(!strcmp(reloaded.scores.entries[2][0].name,"END"));
 testStartRelease();
 frame=0;assert(arcadeAppMain()==0&&frame==11);
 puts("PASS: all three end-of-game flows, shared initials keyboard, list persistence, restart, SELECT commit, save/reload/retry, rejected ranks, exit/cancel, real main input routing, screen guards.");
}
