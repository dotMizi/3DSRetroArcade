#include "arcade.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void safeRescue(Rescue*r){for(int i=0;i<TANKS;i++)r->tanks[i].alive=false;}
int main(void){
 assert(arcadeHit(13,67)==0&&arcadeHit(104,170)==0);assert(arcadeHit(114,80)==1&&arcadeHit(215,80)==2);
 assert(arcadeHit(105,80)==-1&&arcadeHit(10,20)==-1&&arcadeHit(319,170)==-1);
 Shield s;shieldStart(&s,42,500);assert(s.wave==1&&s.ammo[0]==15&&s.best==500);
 assert(shieldFire(&s,100,70,0)&&s.ammo[0]==14);assert(!shieldFire(&s,100,70,0));
 for(int i=0;i<70;i++)shieldStep(&s,1.f/60);bool boom=false;for(int i=0;i<BLASTS;i++)boom|=s.blasts[i].alive;assert(boom);
 shieldStart(&s,3,0);s.remaining=0;s.blasts[0]=(Blast){100,100,.3f,true};s.missiles[0]=(Missile){100,100,55,215,30,true};shieldStep(&s,.01f);assert(!s.missiles[0].alive&&s.score==10&&s.tally);
 for(int i=0;i<850;i++)shieldStep(&s,.01f);assert(s.wave==2&&s.ammo[0]==15);
 s.ammo[0]=s.ammo[1]=s.ammo[2]=0;assert(!shieldFire(&s,100,100,-1));
 shieldStart(&s,3,0);s.remaining=0;for(int hit=1;hit<=3;hit++){
  for(int i=0;i<6;i++)s.missiles[i]=(Missile){55+i*58,214,55+i*58,215,200,true};
  shieldStep(&s,.01f);for(int i=0;i<6;i++)assert(s.city[i]==CITY_DOMED-hit);
  if(hit<3){assert(!s.over);s.tally=false;s.delay=0;}
 }assert(s.over);
 // Mixed damage states: 27 unused rounds, two domed, two exposed, one ruin.
 shieldStart(&s,3,0);s.remaining=0;s.ammo[0]=10;s.ammo[1]=8;s.ammo[2]=9;
 unsigned char states[6]={CITY_DOMED,CITY_EXPOSED,CITY_RUIN,CITY_GONE,CITY_DOMED,CITY_EXPOSED};memcpy(s.city,states,sizeof(states));
 shieldStep(&s,.01f);assert(s.tally&&s.bonusAmmo==27&&s.bonusTotal==5770&&s.score==0&&s.best==0&&s.bonusPaid==0);
 assert(s.bonusCities[0]==1&&s.bonusCities[1]==2&&s.bonusCities[2]==2);assert(!shieldFire(&s,100,100,0));
 int previous=0;bool intermediate=false;
 for(int i=0;i<100;i++){shieldStep(&s,.01f);assert(s.score>=previous);previous=s.score;if(s.score>0&&s.score<5770)intermediate=true;}
 assert(intermediate&&s.score<5770&&s.wave==1);
 for(int i=0;i<750;i++)shieldStep(&s,.01f);assert(s.bonusPaid==5770&&s.bonusShown[0]==27&&s.bonusShown[1]==2&&s.bonusShown[2]==2&&s.bonusShown[3]==1);assert(s.wave==2&&!s.tally&&s.ammo[0]==15&&s.score==5770&&!memcmp(s.city,states,sizeof(states)));
 // Three cities visibly advance 1/1500, 2/3000, 3/4500, with no duplicate awards.
 shieldStart(&s,42,0);s.remaining=0;s.ammo[0]=s.ammo[1]=s.ammo[2]=0;
 for(int i=0;i<6;i++)s.city[i]=i<3?CITY_DOMED:CITY_GONE;
 shieldStep(&s,.01f);int seen=0;
 for(int frame=0;frame<1500&&s.tally;frame++){
  shieldStep(&s,.01f);if(s.bonusShown[1]>seen){seen++;assert(s.bonusShown[1]==seen&&s.score==seen*1500);}
 }
 assert(seen==3&&s.score==4500&&s.bonusPaid==4500&&s.wave==2);
 // Trails retain their launch point as missiles travel; reused slots get a new origin.
 shieldStart(&s,42,0);for(int i=0;i<30;i++)shieldStep(&s,.034f);
 assert(s.missiles[0].alive&&s.missiles[0].y>SHIELD_ENTRY_Y);
 float origin=s.trailStartX[0];for(int i=0;i<30;i++)shieldStep(&s,.034f);assert(s.trailStartX[0]==origin);
 s.blasts[0]=(Blast){s.missiles[0].x,s.missiles[0].y,.3f,true};shieldStep(&s,.01f);assert(!s.missiles[0].alive);
 memset(s.blasts,0,sizeof(s.blasts));s.spawn=0;shieldStep(&s,.01f);assert(s.missiles[0].alive&&s.trailStartX[0]!=origin);
 Rescue r;rescueStart(&r,900,1,0);safeRescue(&r);r.x=418;r.y=196;
 for(int i=0;i<120;i++)rescueStep(&r,0,0,false,.01f);assert(r.onboard==3);
 r.x=80;r.vx=r.vy=0;for(int i=0;i<40;i++)rescueStep(&r,0,0,false,.01f);assert(r.onboard==0&&r.rescued==3&&r.score==600);
 // Pick up the remaining groups, then unload them at the base.
 for(int group=1;group<4;group++){
  r.x=418+group*300;r.y=196;r.vx=r.vy=0;for(int i=0;i<120;i++)rescueStep(&r,0,0,false,.01f);
  assert(r.onboard==3);r.x=80;for(int i=0;i<40;i++)rescueStep(&r,0,0,false,.01f);
 }
 assert(r.win&&r.rescued==12&&r.best>=2400);
 rescueStart(&r,0,1,0);safeRescue(&r);r.shield=0;r.y=195;r.vy=95;rescueStep(&r,0,0,false,.03f);assert(r.lives==2&&r.shield>0);
 rescueStart(&r,0,1,0);safeRescue(&r);r.x=418;r.y=196;for(int i=0;i<120;i++)rescueStep(&r,0,0,false,.01f);r.shield=0;r.bullets[0]=(Bullet){r.x,r.y,0,0,1,true};rescueStep(&r,0,0,false,.01f);assert(r.onboard==0&&r.people[0].state==0&&r.lives==2);
 rescueStart(&r,0,1,0);safeRescue(&r);r.tanks[0]=(Tank){300,100,true};r.bullets[0]=(Bullet){300,201,0,0,1,false};rescueStep(&r,0,0,false,.01f);assert(!r.tanks[0].alive&&r.score==100);
 // The sprite faces sideways at rest and turns through the front only briefly.
 rescueStart(&r,0,1,0);safeRescue(&r);assert(r.yaw==1);
 for(int i=0;i<9;i++)rescueStep(&r,-1,0,false,.02f);
 assert(r.yaw==0&&r.facing==-1&&r.frontHold>.15f);
 for(int i=0;i<7;i++){rescueStep(&r,-1,0,false,.02f);assert(r.yaw==0);}
 rescueStep(&r,-1,0,false,.02f);
 for(int i=0;i<9;i++)rescueStep(&r,-1,0,false,.02f);
 assert(r.yaw== -1);
 for(int i=0;i<100;i++)rescueStep(&r,0,0,false,.01f);
 assert(r.yaw== -1);
 rescueStep(&r,1,0,false,.02f);float yaw=r.yaw;
 rescueStep(&r,-1,0,false,.01f);assert(r.yaw<yaw&&r.yaw>=-1);
 for(int i=0;i<60;i++)rescueStep(&r,1,0,false,.01f);
 assert(r.yaw==1);
 // Lean follows steering; release levels the helicopter even while it drifts.
 rescueStart(&r,0,1,0);safeRescue(&r);
 for(int i=0;i<20;i++)rescueStep(&r,1,0,false,.01f);
 assert(r.tilt==.22f&&r.vx>0);
 for(int i=0;i<20;i++)rescueStep(&r,0,0,false,.01f);
 assert(r.tilt==0&&r.vx>0);
 for(int i=0;i<40;i++)rescueStep(&r,-1,0,false,.01f);
 assert(r.tilt==-.22f);
 for(int i=0;i<20;i++)rescueStep(&r,0,-1,false,.01f);
 assert(r.tilt==0);
 for(int seed=1;seed<=20;seed++){
  shieldStart(&s,seed,0);rescueStart(&r,0,1,0);
  for(int f=0;f<12000;f++){
   if(s.over)shieldStart(&s,seed,s.best);if(r.over||r.win)rescueStart(&r,r.best,1,0);
   shieldFire(&s,200+150*sinf(f*.01f),70+60*cosf(f*.007f),-1);shieldStep(&s,1.f/60);
   rescueStep(&r,sinf(f*.003f),cosf(f*.009f),true,1.f/60);
   assert(isfinite(r.x)&&isfinite(r.y)&&r.x>=20&&r.x<=1580&&r.y>=35&&r.y<=196);assert(r.onboard>=0&&r.onboard<=4&&r.rescued<=12&&r.lives>=0);
   for(int i=0;i<3;i++)assert(s.ammo[i]>=0&&s.ammo[i]<=15);
  }
 }
 puts("PASS: menu hitboxes, ammo, cooldown, explosions, chain hits, city loss, waves, rescue boarding/unloading/victory, crashes, return of passengers, tanks; 480000 simulation frames.");return 0;
}
