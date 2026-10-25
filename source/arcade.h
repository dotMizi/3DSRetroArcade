#ifndef ARCADE_H
#define ARCADE_H
#include <stdbool.h>
#include <stdint.h>
#define MISSILES 60
#define BLASTS 40
#define INTERCEPTORS 32
#define PEOPLE 12
#define TANKS 6
#define BULLETS 40
#define SHIELD_ENTRY_Y 12
enum { CITY_GONE, CITY_RUIN, CITY_EXPOSED, CITY_DOMED };
typedef struct {float x,y,tx,ty,speed;bool alive;} Missile;
typedef struct {float x,y,age;bool alive;} Blast;
typedef struct {
 Missile missiles[MISSILES],interceptors[INTERCEPTORS];Blast blasts[BLASTS];
 float trailStartX[MISSILES]; /* Fixed entry points of incoming missile trails. */
 unsigned char city[6];bool over,tally;int bonusAmmo,bonusCities[3],bonusTotal,bonusShown[4],bonusPaid;unsigned char tallyRow;
 int ammo[3],score,best,wave,remaining;float spawn,delay,cooldown,aimX,aimY;uint32_t rng;
} Shield;
void shieldStart(Shield*s,uint32_t seed,int best);
void shieldStep(Shield*s,float dt);
bool shieldFire(Shield*s,float x,float y,int battery);
float blastRadius(float age);
int shieldBonusTarget(const Shield*s,int row);
int shieldBonusRate(int row);
typedef struct {float x;int state;} Person;
typedef struct {float x,fire;bool alive;} Tank;
typedef struct {float x,y,vx,vy,life;bool hostile;} Bullet;
typedef struct {
 float x,y,vx,vy,camera,shield,cooldown,boarding,time,yaw,frontHold,tilt;int facing,lives,score,best,stage,onboard,rescued;bool over,win;
 Person people[PEOPLE];Tank tanks[TANKS];Bullet bullets[BULLETS];
} Rescue;
void rescueStart(Rescue*r,int best,int stage,int score);
void rescueStep(Rescue*r,float dx,float dy,bool fire,float dt);
int arcadeHit(int x,int y);
#endif
