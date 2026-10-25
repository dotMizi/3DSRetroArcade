#include "arcade.h"
#include <math.h>
#include <string.h>
static float clamp(float x,float a,float b){return fmaxf(a,fminf(b,x));}
static float rand01(Shield*s){s->rng^=s->rng<<13;s->rng^=s->rng>>17;s->rng^=s->rng<<5;return(s->rng&0xffffff)/16777216.f;}
int arcadeHit(int x,int y){if(y<58||y>=186)return -1;for(int i=0;i<3;i++)if(x>=13+i*101&&x<105+i*101)return i;return -1;}
static void shieldWave(Shield*s){s->wave++;s->remaining=10+s->wave*3;s->spawn=.8f;s->delay=0;s->tally=false;for(int i=0;i<3;i++)s->ammo[i]=15;}
void shieldStart(Shield*s,uint32_t seed,int best){memset(s,0,sizeof(*s));s->rng=seed?seed:1;s->best=best;s->aimX=200;s->aimY=100;for(int i=0;i<6;i++)s->city[i]=CITY_DOMED;shieldWave(s);}
float blastRadius(float age){return age<.35f?age*80:age<.95f?28:fmaxf(0,28-(age-.95f)*56);}
static void blast(Shield*s,float x,float y){for(int i=0;i<BLASTS;i++)if(!s->blasts[i].alive){s->blasts[i]=(Blast){x,y,0,true};return;}}
bool shieldFire(Shield*s,float x,float y,int battery){
 if(s->over||s->tally||s->cooldown>0)return false;
 x=clamp(x,5,395);y=clamp(y,20,199);
 if(battery<0){float best=1e9f;for(int i=0;i<3;i++)if(s->ammo[i]>0&&fabsf(x-(30+i*170))<best){best=fabsf(x-(30+i*170));battery=i;}}
 if(battery<0||battery>2||s->ammo[battery]<=0)return false;
 for(int i=0;i<INTERCEPTORS;i++)if(!s->interceptors[i].alive){s->interceptors[i]=(Missile){30+battery*170,216,x,y,260,true};s->ammo[battery]--;s->cooldown=.16f;return true;}
 return false;
}
static bool travel(Missile*m,float dt){float dx=m->tx-m->x,dy=m->ty-m->y,d=hypotf(dx,dy);if(d<=m->speed*dt){m->x=m->tx;m->y=m->ty;return true;}m->x+=dx/d*m->speed*dt;m->y+=dy/d*m->speed*dt;return false;}
int shieldBonusTarget(const Shield*s,int row){return row==0?s->bonusAmmo:row>0&&row<4?s->bonusCities[3-row]:0;}
int shieldBonusRate(int row){static const int rates[4]={10,1500,1000,500};return row>=0&&row<4?rates[row]:0;}
static void shieldCountBonus(Shield*s,float dt){
 s->delay+=dt;
 if(s->tallyRow>=4){if(s->delay>=1.2f)shieldWave(s);return;}
 int row=s->tallyRow;float interval=row==0?.045f:.28f;
 if(s->delay<interval)return;
 if(s->bonusShown[row]<shieldBonusTarget(s,row)){
  s->delay-=interval;s->bonusShown[row]++;
  int points=shieldBonusRate(row);s->bonusPaid+=points;s->score+=points;
  if(s->score>s->best)s->best=s->score;
 }else{s->tallyRow++;s->delay=s->tallyRow==4?0:-.22f;}
}
void shieldStep(Shield*s,float dt){
 if(s->over)return;
 dt=clamp(dt,0,.034f);
 if(s->tally){shieldCountBonus(s,dt);return;}
 s->cooldown=fmaxf(0,s->cooldown-dt);s->spawn-=dt;
 if(s->remaining>0&&s->spawn<=0){for(int i=0;i<MISSILES;i++)if(!s->missiles[i].alive){int target=(int)(rand01(s)*6);s->missiles[i]=(Missile){10+rand01(s)*380,SHIELD_ENTRY_Y,55+target*58,215,22+fminf(s->wave*3,45),true};s->trailStartX[i]=s->missiles[i].x;s->remaining--;s->spawn=fmaxf(.26f,1.2f-s->wave*.06f);break;}}
 for(int i=0;i<INTERCEPTORS;i++){Missile*m=&s->interceptors[i];if(m->alive&&travel(m,dt)){m->alive=false;blast(s,m->x,m->y);}}
 for(int i=0;i<BLASTS;i++)if(s->blasts[i].alive){s->blasts[i].age+=dt;if(s->blasts[i].age>1.45f)s->blasts[i].alive=false;}
 int active=0;
 for(int i=0;i<MISSILES;i++){Missile*m=&s->missiles[i];if(!m->alive)continue;
  bool hit=false;for(int j=0;j<BLASTS;j++)if(s->blasts[j].alive){float radius=blastRadius(s->blasts[j].age);if(hypotf(m->x-s->blasts[j].x,m->y-s->blasts[j].y)<radius+2){hit=true;break;}}
  if(hit){m->alive=false;blast(s,m->x,m->y);s->score+=10;if(s->score>s->best)s->best=s->score;}
  else if(travel(m,dt)){m->alive=false;int city=(int)lroundf((m->tx-55)/58);if(city>=0&&city<6&&s->city[city]>CITY_GONE)s->city[city]--;}
  if(m->alive)active++;
 }
 int cities=0;for(int i=0;i<6;i++)cities+=s->city[i]>CITY_GONE;if(!cities){s->over=true;return;}
 if(s->remaining==0&&!active){
  s->tally=true;s->delay=-.4f;s->tallyRow=0;s->bonusPaid=0;memset(s->bonusShown,0,sizeof(s->bonusShown));s->bonusAmmo=s->ammo[0]+s->ammo[1]+s->ammo[2];
  memset(s->bonusCities,0,sizeof(s->bonusCities));
  for(int i=0;i<6;i++)if(s->city[i]>CITY_GONE)s->bonusCities[s->city[i]-1]++;
  s->bonusTotal=s->bonusAmmo*10+s->bonusCities[2]*1500+s->bonusCities[1]*1000+s->bonusCities[0]*500;

  memset(s->interceptors,0,sizeof(s->interceptors));memset(s->blasts,0,sizeof(s->blasts));
 }
}
void rescueStart(Rescue*r,int best,int stage,int score){
 memset(r,0,sizeof(*r));r->x=80;r->y=130;r->facing=1;r->yaw=1;r->lives=3;r->shield=3;r->best=best;r->stage=stage;r->score=score;
 for(int i=0;i<PEOPLE;i++)r->people[i]=(Person){400+(i/3)*300+(i%3)*18,0};
 for(int i=0;i<TANKS;i++)r->tanks[i]=(Tank){280+i*240,1.5f+i*.3f,true};
}
static void rescueHit(Rescue*r){if(r->shield>0)return;r->lives--;for(int i=0;i<PEOPLE;i++)if(r->people[i].state==1)r->people[i].state=0;r->onboard=0;r->x=80;r->y=130;r->vx=r->vy=0;r->shield=3;if(!r->lives)r->over=true;}
static void bullet(Rescue*r,float x,float y,float vx,float vy,bool hostile){for(int i=0;i<BULLETS;i++)if(r->bullets[i].life<=0){r->bullets[i]=(Bullet){x,y,vx,vy,3,hostile};break;}}
void rescueStep(Rescue*r,float dx,float dy,bool fire,float dt){
 if(r->over||r->win)return;
 dt=clamp(dt,0,.034f);r->time+=dt;r->shield=fmaxf(0,r->shield-dt);r->cooldown-=dt;
 if(fabsf(dx)>.1f)r->facing=dx>0?1:-1;
 /* Nose-down lean follows active steering, not residual velocity. */
 float targetTilt=fabsf(dx)>.1f?clamp(dx,-1,1)*.22f:0;
 float tiltStep=dt*1.5f;
 if(r->tilt<targetTilt)r->tilt=fminf(targetTilt,r->tilt+tiltStep);
 else r->tilt=fmaxf(targetTilt,r->tilt-tiltStep);
 /* Two 0.18-second half-turns with a 0.16-second front-facing dwell. */
 float turnDt=dt;
 if(r->frontHold>0){float held=fminf(turnDt,r->frontHold);r->frontHold-=held;turnDt-=held;}
 float previous=r->yaw,turnSpeed=2.f/.36f,turnStep=turnDt*turnSpeed;
 if(r->yaw<r->facing)r->yaw=fminf((float)r->facing,r->yaw+turnStep);
 else if(r->yaw>r->facing)r->yaw=fmaxf((float)r->facing,r->yaw-turnStep);
 bool crossed=(previous>0&&r->facing<0&&r->yaw<=.0001f)||(previous<0&&r->facing>0&&r->yaw>=-.0001f);
 if(crossed){r->yaw=0;r->frontHold=fmaxf(0,.16f-fmaxf(0,turnDt-fabsf(previous)/turnSpeed));}
 if(fabsf(r->yaw-r->facing)<.0001f)r->yaw=(float)r->facing;
 r->vx=clamp((r->vx+dx*230*dt)*powf(.3f,dt),-145,145);r->vy=clamp((r->vy+dy*230*dt)*powf(.12f,dt),-100,100);
 r->x=clamp(r->x+r->vx*dt,20,1580);r->y=clamp(r->y+r->vy*dt,35,196);
 if(r->y>=196){if(r->vy>65)rescueHit(r);r->vy=0;}
 if(r->x<=20||r->x>=1580)r->vx=0;
 r->camera=clamp(r->x-200,0,1200);
 if(fire&&r->cooldown<=0){bullet(r,r->x+r->facing*20,r->y+2,r->facing*290,100,false);r->cooldown=.18f;}
 bool landed=r->y>=193&&fabsf(r->vx)<35&&fabsf(r->vy)<25;
 if(landed){r->boarding+=dt;if(r->boarding>.35f){r->boarding=0;
   if(r->x<165){for(int i=0;i<PEOPLE;i++)if(r->people[i].state==1){r->people[i].state=2;r->onboard--;r->rescued++;r->score+=200;}}
   else if(r->onboard<4)for(int i=0;i<PEOPLE;i++)if(r->people[i].state==0&&fabsf(r->people[i].x-r->x)<45){r->people[i].state=1;r->onboard++;break;}
 }}else r->boarding=0;
 for(int i=0;i<TANKS;i++){Tank*t=&r->tanks[i];if(!t->alive)continue;t->fire-=dt;if(t->fire<=0){if(fabsf(t->x-r->x)<340){float a=atan2f(r->y-200,r->x-t->x),speed=75+fminf(r->stage*8,65);bullet(r,t->x,200,cosf(a)*speed,sinf(a)*speed,true);}t->fire=fmaxf(1,3.5f-r->stage*.2f)+i*.13f;}
  if(fabsf(t->x-r->x)<22&&r->y>182)rescueHit(r);
 }
 for(int i=0;i<BULLETS;i++){Bullet*b=&r->bullets[i];if(b->life<=0)continue;b->life-=dt;b->x+=b->vx*dt;b->y+=b->vy*dt;
  if(b->y>213||b->y<10||b->x<0||b->x>1600){b->life=0;continue;}
  if(b->hostile){if(hypotf(b->x-r->x,b->y-r->y)<15){b->life=0;rescueHit(r);}}
  else for(int j=0;j<TANKS;j++)if(r->tanks[j].alive&&fabsf(b->x-r->tanks[j].x)<15&&b->y>190){r->tanks[j].alive=false;b->life=0;r->score+=100;break;}
 }
 if(r->score>r->best)r->best=r->score;
 if(r->rescued==PEOPLE)r->win=true;
}
