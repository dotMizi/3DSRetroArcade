#include "delta_intro.h"
#include "draw.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

const char deltaIntroText[]=
 "IN THE YEAR 2187, THE DELTA SYSTEM WAS\n"
 "CREATED TO PROTECT THE COLONIES OF MANKIND.\n"
 "CONTROLLED BY AN ADVANCED ARTIFICIAL\n"
 "INTELLIGENCE, ITS DEFENSE FORCES WERE\n"
 "THOUGHT INVINCIBLE. BUT DELTA HAS TURNED\n"
 "AGAINST ITS MAKERS. NOW ONLY THE BRAVEST\n"
 "PILOTS DARE TO ENTER THE COMBAT ZONE.\n"
 "\n"
 "AGAINST ENDLESS WAVES OF DEADLY MACHINES,\n"
 "ONE PILOT MUST SURVIVE. THE FATE OF MANKIND\n"
 "RESTS UPON SKILL, COURAGE AND SPEED.\n"
 "\n"
 "THE FINAL BATTLE HAS BEEN GIVEN A CODE\n"
 "NAME...\n"
 "\n"
 "DELTA-RACE"
;

int deltaIntroVisible(float elapsed){
 int length=(int)strlen(deltaIntroText);
 return elapsed<=0?0:elapsed>=length/35.f?length:(int)floorf(elapsed*35.f);
}
bool deltaIntroComplete(float elapsed){return deltaIntroVisible(elapsed)==(int)strlen(deltaIntroText);}
void drawDeltaIntro(float elapsed){
 clear(0);int visible=deltaIntroVisible(elapsed),index=0,row=0;
 const char*p=deltaIntroText;
 while(*p){
  const char*end=strchr(p,'\n');int length=end?(int)(end-p):(int)strlen(p);
  bool title=!end;int advance=title?18:8,x=(400-length*advance)/2,y=title?208:12+row*12;
  for(int col=0;col<length;col++){
   if(index+col>=visible)return;
   char c[2]={p[col],0};text(x+col*advance,y,c,0xffffff,title?3:1);
  }
  index+=length;if(!end)break;
  index++;p=end+1;row++;
 }
}
bool deltaIntroStartHit(int x,int y){return x>=40&&x<280&&y>=207&&y<235;}
void drawDeltaIntroPanel(const Scores*s,bool ready){
 clear(0);
 if(!ready)return;
 char b[64];text(46,8,"DELTA-RACE TOP 10",0xffffff,2);
 text(20,30,"RANK",0xffffff,1);text(88,30,"NAME",0xffffff,1);text(220,30,"SCORE",0xffffff,1);
 for(int i=0;i<10;i++){
  int y=46+i*15;snprintf(b,sizeof(b),"%02d",i+1);text(24,y,b,0xffffff,1);
  text(88,y,i<s->count[0]?s->entries[0][i].name:"---",0xffffff,1);
  if(i<s->count[0])snprintf(b,sizeof(b),"%10d",s->entries[0][i].score);else snprintf(b,sizeof(b),"         -");text(192,y,b,0xffffff,1);
 }
 box(40,207,239,27,0xffffff);text(76,217,"A: START DELTA-RACE",0xffffff,1);
}
