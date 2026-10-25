#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>
#include "draw.h"
#include "score_ui.h"
#include "delta_intro.h"
#include "delta_audio.h"
#include "splash_bin.h"

static const char*savePath="sdmc:/3ds/RetroArcade/scores-v2.txt";
typedef enum { MENU, PLAY, ENTER_NAME, SCORE_LIST, DELTA_INTRO } Screen;
typedef struct {
 Game delta; Shield sky; Rescue rescue; Scores scores;
 Screen screen; int mode,selected,highlight[3]; bool completed[3];
 bool exitDialog,quit,dirty,saveFailed;
 u32 blockedPlayKeys;
 float introElapsed;
 int pendingScore,pendingRank,cursor,key; char initials[4];
} App;
static App app;

static void importLegacy(Scores*s){
 int best[3]={0};FILE*f=fopen("sdmc:/3ds/RetroArcade/highscores.txt","r");
 if(!f)f=fopen("sdmc:/3ds/retro-arcade/highscores.txt","r");
 if(f){for(int i=0;i<3;i++)if(fscanf(f,"%d",&best[i])!=1||best[i]<0)best[i]=0;fclose(f);}
 else{f=fopen("sdmc:/3ds/delta-race/highscore.txt","r");if(f){if(fscanf(f,"%d",&best[0])!=1||best[0]<0)best[0]=0;fclose(f);}}
 for(int i=0;i<3;i++)scoresInsert(s,i,best[i],"OLD");
}
static bool appSave(App*a){
 if(!a->dirty)return true;
 for(int i=0;i<2;i++){
  const char*dir=i?"sdmc:/3ds/RetroArcade":"sdmc:/3ds";
  if(mkdir(dir,0777)!=0&&errno!=EEXIST){scoresSetSaveError(i?"CREATE GAME DIR":"CREATE 3DS DIR",errno);a->saveFailed=true;return false;}
 }
 a->saveFailed=!scoresSave(&a->scores,savePath);
 if(!a->saveFailed)a->dirty=false;
 return !a->saveFailed;
}
static void appInit(App*a){
 memset(a,0,sizeof(*a));a->screen=MENU;a->mode=-1;
 for(int i=0;i<3;i++)a->highlight[i]=-1;
 if(!scoresLoadRecover(&a->scores,savePath))importLegacy(&a->scores);
}
static void appStart(App*a,unsigned seed,u32 startKeys){
 a->blockedPlayKeys=startKeys&(KEY_A|KEY_R|KEY_B|KEY_Y|KEY_TOUCH);
 int best=scoresBest(&a->scores,a->mode);a->screen=PLAY;
 if(a->mode==0){gameInit(&a->delta,seed,best);gameStart(&a->delta);}
 if(a->mode==1)shieldStart(&a->sky,seed,best);
 if(a->mode==2)rescueStart(&a->rescue,best,1,0);
}
static void appFinish(App*a,int score){
 a->completed[a->mode]=true;a->highlight[a->mode]=-1;
 a->pendingScore=score;a->pendingRank=scoresRank(&a->scores,a->mode,score);
 if(a->pendingRank>=0){a->screen=ENTER_NAME;memcpy(a->initials,"AAA",4);a->cursor=0;a->key=0;}
 else a->screen=SCORE_LIST;
}
static bool appCommit(App*a){
 if(a->screen==ENTER_NAME){
  a->highlight[a->mode]=scoresInsert(&a->scores,a->mode,a->pendingScore,a->initials);
  a->dirty=true;a->screen=SCORE_LIST;
 }
 return appSave(a);
}
static void typeInitial(App*a,int key){a->initials[a->cursor]=scoreKeys[key];a->cursor=(a->cursor+1)%3;}
static void appUpdate(App*a,u32 down,u32 held,circlePosition cp,touchPosition tp,float dt,unsigned seed){
 if(a->exitDialog){
  if(down&(KEY_B|KEY_START)){a->exitDialog=false;return;}
  if(down&KEY_SELECT){appCommit(a);a->exitDialog=false;a->screen=MENU;a->mode=-1;return;}
  if((down&KEY_X)&&a->saveFailed){a->quit=true;return;}
  if(down&KEY_A){if(appCommit(a))a->quit=true;return;}
  return;
 }
 if(down&KEY_START){a->exitDialog=true;return;}
 if(down&KEY_SELECT){appCommit(a);a->screen=MENU;a->mode=-1;return;}
 if(a->screen==MENU){
  if(down&KEY_LEFT)a->selected=(a->selected+2)%3;
  if(down&KEY_RIGHT)a->selected=(a->selected+1)%3;
  int launch=-1;if(down&KEY_TOUCH)launch=arcadeHit(tp.px,tp.py);
  if(down&KEY_A)launch=a->selected;
  if(launch>=0){a->mode=a->selected=launch;if(launch==0){a->screen=DELTA_INTRO;a->introElapsed=0;}else if(a->completed[launch]&&!(down&KEY_A))a->screen=SCORE_LIST;else appStart(a,seed,held|down);}
  if((down&KEY_Y)&&a->dirty)appSave(a);
  return;
 }
 if(a->screen==DELTA_INTRO){
  bool ready=deltaIntroComplete(a->introElapsed);
  a->introElapsed+=fmaxf(0,dt);
  if(ready&&((down&KEY_A)||((down&KEY_TOUCH)&&deltaIntroStartHit(tp.px,tp.py))))appStart(a,seed,held|down);
  return;
 }
 if(a->screen==SCORE_LIST){if(down&KEY_A)appStart(a,seed,held|down);if((down&KEY_Y)&&a->dirty)appSave(a);return;}
 if(a->screen==ENTER_NAME){
  if(down&KEY_LEFT)a->key=(a->key+36)%37;
  if(down&KEY_RIGHT)a->key=(a->key+1)%37;
  if(down&KEY_UP)a->key=(a->key+27)%37;
  if(down&KEY_DOWN)a->key=(a->key+10)%37;
  if(down&KEY_L)a->cursor=(a->cursor+2)%3;
  if(down&KEY_R)a->cursor=(a->cursor+1)%3;
  if(down&KEY_A)typeInitial(a,a->key);
  if(down&KEY_B){a->cursor=(a->cursor+2)%3;a->initials[a->cursor]=' ';}
  bool confirm=(down&KEY_Y)!=0;
  if(down&KEY_TOUCH){
   int key=scoreKeyHit(tp.px,tp.py);if(key>=0){a->key=key;typeInitial(a,key);}
   if(tp.py>=30&&tp.py<=54&&tp.px>=86&&tp.px<158)a->cursor=(tp.px-86)/24;
   if(tp.py>=207&&tp.py<=233){if(tp.px>=166&&tp.px<=309)confirm=true;else if(tp.px>=10&&tp.px<=153){a->cursor=(a->cursor+2)%3;a->initials[a->cursor]=' ';}}
  }
  if(confirm)appCommit(a);
  return;
 }
 /* Consume each button/contact held at launch until it has been released.
    Mask both edge and held inputs; a held touch may move onto the fire pad. */
 a->blockedPlayKeys&=held|down;
 held&=~a->blockedPlayKeys;down&=~a->blockedPlayKeys;
 float dx=abs(cp.dx)>18?cp.dx/156.f:0,dy=abs(cp.dy)>18?-cp.dy/156.f:0;
 if(held&KEY_LEFT)dx=-1;
 if(held&KEY_RIGHT)dx=1;
 if(held&KEY_UP)dy=-1;
 if(held&KEY_DOWN)dy=1;
 if(a->mode==0){bool touch=(held&KEY_TOUCH)&&tp.py>=187&&tp.py<217;Input in={dx,(held&(KEY_B|KEY_L|KEY_UP))||cp.dy>45||(touch&&tp.px<160),(held&(KEY_A|KEY_R))||(touch&&tp.px>=160),(held&(KEY_X|KEY_DOWN))!=0};gameStep(&a->delta,in,dt);if(!a->delta.playing)appFinish(a,a->delta.score);}
 if(a->mode==1){Shield*s=&a->sky;
  s->aimX=fmaxf(5,fminf(395,s->aimX+dx*170*dt));s->aimY=fmaxf(20,fminf(199,s->aimY+dy*130*dt));
  if((held&KEY_TOUCH)&&tp.px>=10&&tp.px<=310&&tp.py>=43&&tp.py<=167){s->aimX=(tp.px-10)/.75f;s->aimY=(tp.py-44)/.55f;shieldFire(s,s->aimX,s->aimY,-1);}
  if((down&KEY_TOUCH)&&tp.py>=177&&tp.py<=201){int battery=(tp.px-10)/102;if(tp.px>=10&&battery<3)shieldFire(s,s->aimX,s->aimY,battery);}
  if(held&KEY_Y)shieldFire(s,s->aimX,s->aimY,0);
  if(held&(KEY_A|KEY_R))shieldFire(s,s->aimX,s->aimY,1);
  if(held&KEY_B)shieldFire(s,s->aimX,s->aimY,2);
  shieldStep(s,dt);if(s->over)appFinish(a,s->score);
 }
 if(a->mode==2){rescueStep(&a->rescue,dx,dy,(held&(KEY_A|KEY_R))!=0,dt);if(a->rescue.over||a->rescue.win)appFinish(a,a->rescue.score);}
}
static void shieldPanel(Shield*s){if(s->tally){drawShieldTallyPanel(s);return;}char buf[64];clear(0x080b20);text(16,7,"SKY SHIELD",0xffa34f,2);text(16,28,"TOUCH: AIM AND FIRE",0xcbb7e8,1);
 box(9,43,302,124,0x384363);
 int x=10+s->aimX*.75f,y=44+s->aimY*.55f;line(x-4,y,x+4,y,0x58eaff);line(x,y-4,x,y+4,0x58eaff);
 for(int i=0;i<3;i++){box(10+i*102,177,96,24,0xffa34f);snprintf(buf,sizeof(buf),"%c  AMMO %02d","YAB"[i],s->ammo[i]);text(19+i*102,186,buf,0xffffff,1);}
 text(12,211,s->over?"GAME OVER - A: RESTART":"PAD: AIM  Y/A/B: BATTERIES",0xffffff,1);text(12,228,"START: EXIT   SELECT: MENU",0x9989b4,1);
}

static void appRender(App*a,unsigned char*top,unsigned char*bottom){
 canvas(top,400);
 if(a->screen==MENU)memcpy(top,splash_bin,400*240*3);
 else if(a->screen==DELTA_INTRO)drawDeltaIntro(a->introElapsed);
 else if(a->screen==SCORE_LIST)drawScoreList(&a->scores,a->mode,a->highlight[a->mode],false);
 else if(a->screen==ENTER_NAME){Scores preview=a->scores;int rank=scoresInsert(&preview,a->mode,a->pendingScore,a->initials);drawScoreList(&preview,a->mode,rank,true);}
 else if(a->mode==0)drawDelta(&a->delta);
 else if(a->mode==1)drawShield(&a->sky);
 else drawRescue(&a->rescue);
 canvas(bottom,320);char status[64];
 if(a->screen==MENU)drawMenu(a->selected);
 else if(a->screen==DELTA_INTRO)drawDeltaIntroPanel(&a->scores,deltaIntroComplete(a->introElapsed));
 else if(a->screen==SCORE_LIST)drawScoreActions(a->saveFailed);
 else if(a->screen==ENTER_NAME)drawScoreKeyboard(a->pendingRank,a->initials,a->pendingScore,a->cursor,a->key);
 else if(a->mode==0)drawDeltaPanel(&a->delta,scoresBest(&a->scores,0));
 else if(a->mode==1)shieldPanel(&a->sky);
 else{snprintf(status,sizeof(status),"LIVES %d  ABOARD %d  RESCUED %d",a->rescue.lives,a->rescue.onboard,a->rescue.rescued);drawPanel(2,a->rescue.score,scoresBest(&a->scores,2),status);}
 if(a->saveFailed&&a->screen!=SCORE_LIST)text(16,238-7,scoresSaveError(),0xff547b,1);
 if(a->screen==PLAY&&a->mode==0&&!deltaAudioAvailable())text(156,177,"AUDIO UNAVAILABLE",0xffa34f,1);
 if(a->exitDialog)drawExitDialog(a->saveFailed);
}
int main(void){
 gfxInitDefault();gfxSet3D(false);deltaAudioInit();appInit(&app);u64 last=svcGetSystemTick();
 while(aptMainLoop()){
  hidScanInput();circlePosition cp;hidCircleRead(&cp);touchPosition tp;hidTouchRead(&tp);
  u64 now=svcGetSystemTick();float dt=fminf((now-last)/268123480.f,.034f);last=now;
  appUpdate(&app,hidKeysDown(),hidKeysHeld(),cp,tp,dt,(unsigned)now);
  deltaAudioUpdate(&app.delta,app.screen==PLAY&&app.mode==0&&!app.exitDialog);
  if(app.quit)break;
  appRender(&app,gfxGetFramebuffer(GFX_TOP,GFX_LEFT,NULL,NULL),gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,NULL,NULL));
  gfxFlushBuffers();gfxSwapBuffers();gspWaitForVBlank();
 }
 if(!app.quit)appCommit(&app);
 deltaAudioExit();gfxExit();return 0;
}
