#ifndef DELTA_GAME_H
#define DELTA_GAME_H
#include <stdbool.h>
#include <stdint.h>
#define MAX_ENEMIES 24
#define MAX_SHOTS 64
#define MAX_SPARKS 100
enum { DELTA_SOUND_PLAYER=1, DELTA_SOUND_ENEMY=2, DELTA_SOUND_BOUNCE=4 };
typedef struct { float x,y,vx,vy; } Body;
typedef struct { Body b; float life; bool hostile; } Shot;
typedef struct { Body b; float turn,fire; int hp,type; } Enemy;
typedef struct { Body b; float life; unsigned color; } Spark;
typedef struct {
 Body ship; float angle,shield,cooldown,waveDelay,time;
 int score,best,lives,wave,count; bool playing,paused,thrust;
 unsigned soundEvents;float bounceCooldown;
 uint32_t rng; Enemy enemies[MAX_ENEMIES]; Shot shots[MAX_SHOTS]; Spark sparks[MAX_SPARKS];
} Game;
typedef struct { float turn; bool thrust,fire,brake; } Input;
void gameInit(Game *g, uint32_t seed, int best);
void gameStart(Game *g);
void gameStep(Game *g, Input input, float dt);
bool arenaSolid(float x,float y,float radius);
bool arenaMove(Body *b,float radius,float dt);
#endif
