#include "draw.h"
#include "huey_sprite.h"
#include "menu_icons.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CYAN 0x58eaff
#define ORANGE 0xffa34f
static unsigned char *fb;static int width;
void canvas(unsigned char *p,int w){fb=p;width=w;}
void pixel(int x,int y,unsigned c){if(x<0||x>=width||y<0||y>=240)return;int p=(x*240+239-y)*3;fb[p]=c&255;fb[p+1]=(c>>8)&255;fb[p+2]=(c>>16)&255;}
void clear(unsigned c){if(!c){memset(fb,0,width*240*3);return;}for(int x=0;x<width;x++)for(int y=0;y<240;y++)pixel(x,y,c);}
void line(int x,int y,int xx,int yy,unsigned c){int dx=abs(xx-x),sx=x<xx?1:-1,dy=-abs(yy-y),sy=y<yy?1:-1,e=dx+dy;for(;;){pixel(x,y,c);if(x==xx&&y==yy)break;int t=2*e;if(t>=dy){e+=dy;x+=sx;}if(t<=dx){e+=dx;y+=sy;}}}
void box(int x,int y,int w,int h,unsigned c){line(x,y,x+w,y,c);line(x+w,y,x+w,y+h,c);line(x+w,y+h,x,y+h,c);line(x,y+h,x,y,c);}
void fill(int x,int y,int w,int h,unsigned c){for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)pixel(xx,yy,c);}
void circle(int x,int y,int r,unsigned c){for(int i=0;i<32;i++){float a=i*6.2831853f/32,b=(i+1)*6.2831853f/32;line(x+cosf(a)*r,y+sinf(a)*r,x+cosf(b)*r,y+sinf(b)*r,c);}}
static void ship(Game *g) {float a=g->angle;int x=g->ship.x,y=g->ship.y;float c=cosf(a),s=sinf(a);unsigned color=g->shield>0?0xffffff:CYAN;
 int nx=x+c*10,ny=y+s*10,lx=x-c*6-s*6,ly=y-s*6+c*6,rx=x-c*6+s*6,ry=y-s*6-c*6;
 line(nx,ny,lx,ly,color);line(lx,ly,x-c*3,y-s*3,color);line(x-c*3,y-s*3,rx,ry,color);line(rx,ry,nx,ny,color);
 if(g->thrust){line(lx,ly,x-c*17,y-s*17,ORANGE);line(rx,ry,x-c*17,y-s*17,ORANGE);}if(g->shield>0)circle(x,y,14,0x286b84);
}
/* Shared silhouettes keep the arcade point legend identical to the arena enemies. */
static void deltaEnemy(int x,int y,int type,float angle,float radius){
 unsigned color=type==1?0xff547b:ORANGE;int sides=type==2?6:4;
 for(int j=0;j<sides;j++){
  float a=angle+j*6.2831853f/sides,b=angle+(j+1)*6.2831853f/sides;
  line(x+cosf(a)*radius,y+sinf(a)*radius,x+cosf(b)*radius,y+sinf(b)*radius,color);
 }
 pixel(x,y,color);
}
void drawDelta(Game *g) {
 clear(0);
 for(int i=0;i<70;i++)pixel((i*137+23)%398,(i*i*17+31)%238,0x153047);
 box(10,10,380,220,0x327385);box(155,85,90,70,CYAN);box(159,89,82,62,0x163b56);
 // Original delta emblem at the centre of the arena.
 line(200,101,182,137,0x327385);line(182,137,218,137,0x327385);line(218,137,200,101,0x327385);
 for(int i=0;i<MAX_ENEMIES;i++){Enemy*e=&g->enemies[i];if(e->hp>0)deltaEnemy(e->b.x,e->b.y,e->type,e->turn,e->hp==2?9:7);}
 for(int i=0;i<MAX_SHOTS;i++){Shot *s=&g->shots[i];if(s->life>0)line(s->b.x,s->b.y,s->b.x-s->b.vx*.018f,s->b.y-s->b.vy*.018f,s->hostile?0xff547b:0xcaffff);}
 for(int i=0;i<MAX_SPARKS;i++){Spark *s=&g->sparks[i];if(s->life>0)pixel(s->b.x,s->b.y,s->color);}
 if(g->playing)ship(g);
}
static const unsigned char glyphs[][7]={
{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
{31,4,4,4,4,4,31},{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}};
void text(int x,int y,const char*s,unsigned c,int scale){for(;*s;s++,x+=6*scale){char ch=*s;if(ch>='a'&&ch<='z')ch-=32;int index=ch>='A'&&ch<='Z'?ch-'A':ch>='0'&&ch<='9'?26+ch-'0':-1;
 if(index>=0){for(int row=0;row<7;row++)for(int col=0;col<5;col++)if(glyphs[index][row]&(16>>col))fill(x+col*scale,y+row*scale,scale,scale,c);}
 else if(ch=='.')fill(x+2*scale,y+6*scale,scale,scale,c);else if(ch==','){fill(x+2*scale,y+5*scale,scale,scale,c);fill(x+scale,y+6*scale,scale,scale,c);}else if(ch=='-')fill(x,y+3*scale,5*scale,scale,c);else if(ch==':'){fill(x+2*scale,y+scale,scale,scale,c);fill(x+2*scale,y+5*scale,scale,scale,c);}else if(ch=='/')line(x,y+6*scale,x+4*scale,y,c);
}}
/* Compose the sprite and rotors before rotating the whole monochrome aircraft. */
static void heliMaskPixel(unsigned char mask[64][96],int x,int y){
 x+=48;y+=32;if(x>=0&&x<96&&y>=0&&y<64)mask[y][x]=1;
}
static void heli(int x,int y,float yaw,unsigned color,float time,float tilt){
 unsigned char mask[64][96]={0};
 float side=fabsf(yaw);int dir=yaw<0?-1:1;
 for(int row=0;row<HUEY_HEIGHT;row++)for(int col=0;col<HUEY_WIDTH;col++)
  if(side>.05f&&hueyMask[row][col]=='#')heliMaskPixel(mask,(int)lroundf(yaw*(col-HUEY_PIVOT_X)),row-HUEY_PIVOT_Y);
 if(side<.98f){
  float front=sqrtf(fmaxf(0,1-yaw*yaw));
  for(int row=0;row<HUEY_HEIGHT;row++)for(int col=0;col<HUEY_FRONT_WIDTH;col++)
   if(hueyFrontMask[row][col]=='#')heliMaskPixel(mask,(int)lroundf(front*(col-HUEY_FRONT_WIDTH/2)),row-HUEY_PIVOT_Y);
 }
 /* Two-bladed main rotor: eight discrete pixel frames, independent of frame rate. */
 static const int reach[8]={27,23,13,4,13,23,27,13};
 static const int slope[8]={0,1,1,0,-1,-1,0,1};
 int frame=((unsigned)(time*18))%8;
 for(int i=-reach[frame];i<=reach[frame];i++){
  int row=2+(i>0?slope[frame]:i<0?-slope[frame]:0);
  heliMaskPixel(mask,dir*i,row-HUEY_PIVOT_Y);
 }
 /* The tail rotor animates separately around its fixed hub. */
 static const int tail[4][2]={{0,3},{2,2},{3,0},{2,-2}};
 int tx=(int)lroundf(yaw*(7-HUEY_PIVOT_X)),ty=15-HUEY_PIVOT_Y;
 int tailFrame=((unsigned)(time*24))%4;
 if(side>.6f)for(int i=-3;i<=3;i++)heliMaskPixel(mask,tx+dir*tail[tailFrame][0]*i/3,ty+tail[tailFrame][1]*i/3);
 /* Inverse nearest-neighbour sampling avoids holes in rotated pixel silhouettes. */
 float c=cosf(tilt),s=sinf(tilt);
 for(int yy=-32;yy<32;yy++)for(int xx=-48;xx<48;xx++){
  int sx=(int)lroundf(c*xx+s*yy)+48,sy=(int)lroundf(-s*xx+c*yy)+32;
  if(sx>=0&&sx<96&&sy>=0&&sy<64&&mask[sy][sx])pixel(x+xx,y+yy,color);
 }
}
void drawMenu(int selected){clear(0x080b20);
 for(int y=0;y<240;y+=12)line(0,y,319,y,0x0d1632);
 text(61,13,"RETROARCADE",CYAN,3);text(112,42,"CHOOSE YOUR GAME",0xcbb7e8,1);
 unsigned colors[]={CYAN,0xffa34f,0xff5bbd};const char*labels[][2]={{"DELTA","RACE"},{"SKY","SHIELD"},{"RESCUE","WING"}};
 for(int i=0;i<3;i++){
  int x=13+i*101,cx=x+46;fill(x,58,92,128,0x11152e);box(x,58,91,127,colors[i]);if(i==selected)box(x-2,56,95,131,0xffffff);
  for(int yy=0;yy<MENU_ICON_SIZE;yy++)for(int xx=0;xx<MENU_ICON_SIZE;xx++){
   unsigned v=menuIcons[i][yy*MENU_ICON_SIZE+xx];
   unsigned r=(v>>11)&31,g=(v>>5)&63,b=v&31;
   unsigned color=(((r<<3)|(r>>2))<<16)|(((g<<2)|(g>>4))<<8)|(b<<3)|(b>>2);
   pixel(x+2+xx,60+yy,color);
  }
  text(cx-(int)strlen(labels[i][0])*3,158,labels[i][0],colors[i],1);text(cx-(int)strlen(labels[i][1])*3,174,labels[i][1],colors[i],1);
 }
 text(103,197,"TAP A GAME TO START",0xffffff,1);text(70,214,"LEFT / RIGHT: SELECT  A: START",0x9989b4,1);text(124,230,"START: EXIT",0x9989b4,1);
}

static void deltaLargeScore(int x,int score,unsigned color){
 char b[16];snprintf(b,sizeof(b),"%07d",score);int scale=strlen(b)<=7?3:2;text(x,28,b,color,scale);
}
void drawDeltaPanel(const Game*g,int best){
 char b[64];clear(0x080b20);
 text(12,8,"SCORE",CYAN,2);text(172,8,"BEST",0xffa34f,2);
 deltaLargeScore(12,g->score,0xffffff);deltaLargeScore(172,best>g->score?best:g->score,0xffffff);
 snprintf(b,sizeof(b),"LIVES %d   WAVE %d   ENEMIES %d",g->lives,g->wave,g->count);text(12,58,b,0xcbb7e8,1);
 line(12,72,308,72,0x384363);text(15,78,"TARGET",0x9989b4,1);text(214,78,"POINTS",0x9989b4,1);
 static const char*names[]={"DRONE","HUNTER","ARMOURED"};
 for(int i=0;i<3;i++){
  int y=99+i*25;deltaEnemy(30,y,i,g->time*2+i*.7f,i==2?11:10);
  text(58,y-3,names[i],i==1?0xff547b:ORANGE,1);
  snprintf(b,sizeof(b),"%d PTS",100*(i+1));text(210,y-6,b,0xffffff,2);
 }
 text(12,166,"PAD: ROTATE  B/L: THRUST  A/R: FIRE",0xcbb7e8,1);text(12,177,"X: BRAKE",0x9989b4,1);
 fill(12,187,144,30,0x12364a);fill(164,187,144,30,0x481d3c);
 text(48,198,"THRUST",CYAN,2);text(212,198,"FIRE",0xff5bbd,2);
 text(12,229,"START: EXIT    SELECT: MENU",0x9989b4,1);
}
void drawPanel(int game,int score,int best,const char*status){
 static const char*names[]={"DELTA RACE","SKY SHIELD","RESCUE WING"};char b[64];clear(0x080b20);text(16,12,names[game],CYAN,2);
 snprintf(b,sizeof(b),"SCORE %07d   BEST %07d",score,best);text(16,38,b,0xffffff,1);text(16,57,status,0xffa34f,1);
 line(16,77,304,77,0x384363);
 if(game==0){text(16,91,"PAD LEFT / RIGHT: ROTATE",0xcbb7e8,1);text(16,108,"B / L / UP: THRUST",0xcbb7e8,1);text(16,125,"A / R: FIRE   X: BRAKE",0xcbb7e8,1);fill(16,151,140,42,0x12364a);fill(164,151,140,42,0x481d3c);text(55,168,"THRUST",CYAN,1);text(203,168,"FIRE",0xff5bbd,1);}
 if(game==2){text(16,91,"PAD / D-PAD: FLY",0xcbb7e8,1);text(16,108,"A / R: FIRE",0xcbb7e8,1);text(16,125,"LAND GENTLY TO PICK UP PEOPLE",0xcbb7e8,1);text(16,144,"LEFT BASE: DROP OFF PASSENGERS",0xcbb7e8,1);text(16,163,"CAPACITY: 4 PASSENGERS",CYAN,1);}
 text(16,210,"START: EXIT    SELECT: MENU",0x9989b4,1);text(16,227,"AFTER GAME OVER: HIGH SCORES",0x9989b4,1);
}
/* Translucent shield blends with the skyline beneath it. */
static void blendPixel(int x,int y,unsigned color,unsigned alpha){
 if(x<0||x>=width||y<0||y>=240)return;
 int offset=(x*240+239-y)*3;
 unsigned r=(((color>>16)&255)*alpha+fb[offset+2]*(255-alpha))/255;
 unsigned g=(((color>>8)&255)*alpha+fb[offset+1]*(255-alpha))/255;
 unsigned b=((color&255)*alpha+fb[offset]*(255-alpha))/255;
 pixel(x,y,(r<<16)|(g<<8)|b);
}
static void skyCity(int x,int base,int state,int variant){
 if(state==CITY_GONE)return;
 static const int heights[5]={9,15,11,18,13};
 for(int i=0;i<5;i++){
  int left=x-18+i*7,height=heights[(i+variant)%5];
  if(state==CITY_RUIN){height=3+(i+variant)%5;fill(left,base-height,6,height,0x6f6268);line(left,base-height,left+2,base-height-2,0xa58c7b);}
  else{
   fill(left,base-height,6,height,i%2?0x436784:0x33516d);
   for(int yy=base-height+3;yy<base-2;yy+=4){pixel(left+1,yy,0xb9f5f1);pixel(left+4,yy,0x66cbd5);}
   if(i==3)line(left+3,base-height,left+3,base-height-3,0x7fc8cf);
  }
 }
 if(state==CITY_DOMED){
  for(int xx=-23;xx<=23;xx++){
   int h=(int)sqrtf(23*23-xx*xx);
   for(int yy=base-h;yy<=base;yy++)blendPixel(x+xx,yy,0x55dfff,40);
  }
  for(int i=0;i<32;i++){
   float a=i*3.14159265359f/32,b=(i+1)*3.14159265359f/32;
   line(x+cosf(a)*23,base-sinf(a)*23,x+cosf(b)*23,base-sinf(b)*23,0x75eaff);
  }
  line(x-23,base,x+23,base,0x337f9b);
 }
}
static void skyBonusIcon(int x,int base,int row){
 if(row){skyCity(x,base,4-row,row-1);return;}
 /* An interceptor picture replaces the ammunition text label. */
 fill(x-2,base-19,5,15,0xe1e9ff);line(x-2,base-19,x,base-23,0x58eaff);line(x,base-23,x+2,base-19,0x58eaff);
 line(x-2,base-9,x-6,base-3,0x58eaff);line(x+2,base-9,x+6,base-3,0x58eaff);
 line(x-1,base-3,x,base+1,0xffa34f);line(x,base+1,x+1,base-3,0xffa34f);
}
static void shieldTally(const Shield*s){
 char b[64];fill(24,20,352,188,0x080b20);box(24,20,352,188,0x58eaff);
 snprintf(b,sizeof(b),"WAVE %d COMPLETE",s->wave);text((400-(int)strlen(b)*12)/2,30,b,0xffffff,2);
 text(114,51,"COUNT",0x9989b4,1);text(253,51,"POINTS",0x9989b4,1);
 for(int row=0;row<4;row++){
  int base=84+row*30;if(s->tallyRow==row)fill(34,base-25,332,28,0x152a40);
  skyBonusIcon(65,base,row);
  snprintf(b,sizeof(b),"%02d",s->bonusShown[row]);text(120,base-15,b,0xffffff,2);
  snprintf(b,sizeof(b),"X %d",shieldBonusRate(row));text(165,base-12,b,0xcbb7e8,1);
  snprintf(b,sizeof(b),"%5d",s->bonusShown[row]*shieldBonusRate(row));text(248,base-15,b,0xffa34f,2);
 }
 snprintf(b,sizeof(b),"WAVE BONUS %d",s->bonusPaid);text((400-(int)strlen(b)*12)/2,188,b,0x58eaff,2);
}
void drawShieldTallyPanel(const Shield*s){
 char b[64];clear(0x080b20);text(52,12,"WAVE BONUS",0x58eaff,3);
 snprintf(b,sizeof(b),"SCORE %d",s->score);text(16,46,b,0xffffff,2);
 snprintf(b,sizeof(b),"BEST  %d",s->best);text(16,69,b,0xffa34f,2);
 for(int row=0;row<4;row++){
  int x=40+row*80;skyBonusIcon(x,132,row);
  snprintf(b,sizeof(b),"%02d",s->bonusShown[row]);text(x-12,146,b,0xffffff,2);
  snprintf(b,sizeof(b),"%d",s->bonusShown[row]*shieldBonusRate(row));text(x-(int)strlen(b)*3,170,b,0xffa34f,1);
 }
 text(40,198,s->tallyRow<4?"COUNTING BONUS":"NEXT WAVE...",0xcbb7e8,2);
 text(16,228,"START: EXIT   SELECT: MENU",0x9989b4,1);
}
void drawShield(Shield*s){clear(0x030916);char buf[64];line(10,216,390,216,0x326576);
 for(int i=0;i<6;i++)skyCity(55+i*58,215,s->city[i],i);
 for(int i=0;i<3;i++){fill(23+i*170,211,14,8,ORANGE);snprintf(buf,sizeof(buf),"%02d",s->ammo[i]);text(24+i*170,227,buf,ORANGE,1);}
 for(int i=0;i<MISSILES;i++){Missile*m=&s->missiles[i];if(m->alive){line(s->trailStartX[i],SHIELD_ENTRY_Y,m->x,m->y,0xff547b);}}
 for(int i=0;i<INTERCEPTORS;i++){Missile*m=&s->interceptors[i];if(m->alive)line(m->x,m->y,m->x,m->y+6,CYAN);}
 for(int i=0;i<BLASTS;i++)if(s->blasts[i].alive){int radius=blastRadius(s->blasts[i].age);circle(s->blasts[i].x,s->blasts[i].y,radius,ORANGE);if(radius>5)circle(s->blasts[i].x,s->blasts[i].y,radius-4,0xff547b);}
 line(s->aimX-5,s->aimY,s->aimX+5,s->aimY,CYAN);line(s->aimX,s->aimY-5,s->aimX,s->aimY+5,CYAN);
 snprintf(buf,sizeof(buf),"SKY SHIELD  WAVE %d  SCORE %d",s->wave,s->score);text(12,2,buf,0xffffff,1);
 if(s->tally)shieldTally(s);
}
void drawRescue(Rescue*r){clear(0x060f24);char buf[64];
 for(int i=0;i<10;i++){int x=i*180-r->camera*.4f;line(x,195,x+80,120,0x172640);line(x+80,120,x+180,195,0x172640);}
 line(0,212,399,212,0x6ea19a);int base=-r->camera;box(base+5,185,160,27,0x58eaff);text(base+23,196,"RESCUE BASE",CYAN,1);
 for(int i=0;i<4;i++){int x=380+i*300-r->camera;box(x,174,85,38,0x4c4570);text(x+12,180,"RESCUE",0x847ca7,1);}
 for(int i=0;i<PEOPLE;i++)if(r->people[i].state==0){int x=r->people[i].x-r->camera;circle(x,199,2,0xffd392);line(x,202,x,208,0xffd392);line(x,204,x-4,206,0xffd392);line(x,204,x+4,206,0xffd392);line(x,208,x-3,212,0xffd392);line(x,208,x+3,212,0xffd392);}
 for(int i=0;i<TANKS;i++)if(r->tanks[i].alive){int x=r->tanks[i].x-r->camera;box(x-13,202,26,10,ORANGE);box(x-6,197,12,5,ORANGE);line(x,198,x+(r->x>r->tanks[i].x?15:-15),189,ORANGE);}
 for(int i=0;i<BULLETS;i++){Bullet*b=&r->bullets[i];if(b->life>0)line(b->x-r->camera,b->y,b->x-r->camera-b->vx*.025f,b->y-b->vy*.025f,b->hostile?0xff547b:CYAN);}
 heli(r->x-r->camera,r->y,r->yaw,r->shield>0?0xffffff:0xb5d77a,r->time,r->tilt);
 snprintf(buf,sizeof(buf),"LIVES %d   ABOARD %d/4   RESCUED %d/12",r->lives,r->onboard,r->rescued);text(8,225,buf,0xffffff,1);
 snprintf(buf,sizeof(buf),"RESCUE WING  STAGE %d  SCORE %d",r->stage,r->score);text(8,5,buf,0xffffff,1);
}
