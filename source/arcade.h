#ifndef ARCADE_H
#define ARCADE_H
#include <stdbool.h>
#include <stdint.h>
#define MISSILES 60
#define BLASTS 40
#define INTERCEPTORS 32
#define PEOPLE 64
#define RESCUE_PEOPLE_PER_HOUSE 16
#define RESCUE_CAPACITY 16
#define RESCUE_HOME_DOOR_X 132
#define RESCUE_WORLD_WIDTH 3200
#define RESCUE_FIRST_GROUP_X 840
#define RESCUE_GROUP_SPACING 600
#define RESCUE_FENCE_X 600
#define RESCUE_SOUND_OUCH 1u
#define RESCUE_SOUND_HOUSE_HIT 2u
#define RESCUE_SOUND_TALLY 4u
#define RESCUE_SOUND_WAVE 8u
#define RESCUE_HOUSES 4
#define RESCUE_TANK_MIN_X (RESCUE_FENCE_X + 26)

enum
{
    PERSON_WAITING,
    PERSON_ABOARD,
    PERSON_INSIDE,
    PERSON_GOING_HOME,
    PERSON_LOST,
    PERSON_SHELTERED,
    PERSON_FLEEING
};

enum
{
    HOME_WALK,
    HOME_PAUSE,
    HOME_WAVE
};

typedef struct
{
    float speed, timer, phase;
    int action, facing, stops;
    uint32_t rng;
} HomeWalk;

#define RESCUE_HORIZON 190
#define RESCUE_LAND_Y 174
#define RESCUE_TANK_Y 210
#define TANKS 6
#define BULLETS 40
#define RESCUE_AIRCRAFT 10
#define RESCUE_MINES 12
#define SHIELD_ENTRY_Y 12

enum
{
    CITY_GONE,
    CITY_RUIN,
    CITY_EXPOSED,
    CITY_DOMED
};

typedef struct
{
    float x, y, tx, ty, speed;
    bool alive;
} Missile;

typedef struct
{
    float x, y, age;
    bool alive;
} Blast;

typedef struct
{
    int missiles, smartBombs;
    float missileSpeed, launchInterval;
} ShieldWaveConfig;

ShieldWaveConfig shieldWaveConfig(int wave);

enum
{
    SHIELD_SOUND_LAUNCH = 1,
    SHIELD_SOUND_BLAST = 2,
    SHIELD_SOUND_DAMAGE = 4,
    SHIELD_SOUND_BONUS = 8,
    SHIELD_SOUND_WAVE = 16
};

typedef struct
{
    Missile missiles[MISSILES], interceptors[INTERCEPTORS];
    Blast blasts[BLASTS];
    bool smart[MISSILES];
    int smartRemaining;
    unsigned soundEvents;
    float trailStartX[MISSILES]; /* Fixed entry points of incoming missile trails.
                                */
    unsigned char city[6];
    bool over, tally;
    int bonusAmmo, bonusCities[3], bonusTotal, bonusShown[4], bonusPaid;
    unsigned char tallyRow;
    bool baseDestroyed[3];
    int ammo[3], score, best, wave, remaining;
    float spawn, delay, cooldown, aimX, aimY;
    uint32_t rng;
} Shield;

void shieldStart(Shield *s, uint32_t seed, int best);
void shieldStep(Shield *s, float dt);
bool shieldFire(Shield *s, float x, float y);
float blastRadius(float age);
int shieldBonusTarget(const Shield *s, int row);
int shieldBonusRate(int row);

typedef struct
{
    float x;
    int state;
} Person;

typedef struct
{
    float x, fire;
    bool alive;
} Tank;

typedef struct
{
    float x, y, vx, vy, life;
    bool hostile, forward;
} Bullet;

typedef struct
{
    float x, y, life;
} BombImpact;

typedef struct
{
    float x, escapeDelay;
    bool burning, evacuating;
    int residents;
} RescueHouse;

enum
{
    RESCUE_JET,
    RESCUE_DRONE
};

typedef struct
{
    int jets, drones, tanks;
    bool mines;
} RescueThreatConfig;

RescueThreatConfig rescueThreatConfig(int stage);

typedef struct
{
    float x, y, fire, mineDelay;
    int kind, hp, facing;
    bool alive;
} RescueAircraft;

typedef struct
{
    float x, y, life;
    bool alive;
} RescueMine;

typedef struct
{
    float x, y, vx, vy, camera, shield, cooldown, gunCooldown, boarding, time, yaw, frontHold, tilt;
    float crashTimer;
    float tankSpawnDelay, tankLife[TANKS], ouchLife[PEOPLE];
    uint32_t patrolRng;
    unsigned soundEvents;
    int facing, lives, score, best, stage, onboard, rescued, lost;
    bool over, win, tally, invincible;
    int stageStartScore, tanksDestroyed, jetsDestroyed, dronesDestroyed, tallyShown[5], tallyRow, tallyScore;
    float tallyDelay;
    RescueHouse houses[RESCUE_HOUSES];
    Person people[PEOPLE];
    float peopleHome[PEOPLE];
    HomeWalk homeWalk[PEOPLE];
    float unloadDelay;
    float tankVelocity[TANKS];
    float aircraftSpawnDelay;
    RescueAircraft aircraft[RESCUE_AIRCRAFT];
    RescueMine mines[RESCUE_MINES];
    Tank tanks[TANKS];
    Bullet bullets[BULLETS];
    BombImpact bombImpacts[8];
} Rescue;

void rescueStart(Rescue *r, int best, int stage, int score);
void rescueStep(Rescue *r, float dx, float dy, bool fire, bool dropBomb, float dt);
int arcadeHit(int x, int y);
#endif
