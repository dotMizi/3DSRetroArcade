#include "game.h"
#include <math.h>
#include <string.h>
#define PI 3.14159265358979323846f

static float random01(Game *g)
{
    g->rng ^= g->rng << 13;
    g->rng ^= g->rng >> 17;
    g->rng ^= g->rng << 5;
    return (g->rng & 0xffffff) / 16777216.0f;
}

static float dist2(Body a, Body b)
{
    float x = a.x - b.x, y = a.y - b.y;
    return x * x + y * y;
}

bool arenaSolid(float x, float y, float r)
{
    return x < 10 + r || x > 390 - r || y < 10 + r || y > 230 - r ||
           (x > 155 - r && x < 245 + r && y > 85 - r && y < 155 + r);
}

/* Clockwise from the upper-left half: two bars on each outer edge. */
const int arenaWallLines[ARENA_WALL_SEGMENTS][4] = {{10, 10, 198, 10},
                                                    {202, 10, 390, 10},
                                                    {390, 10, 390, 118},
                                                    {390, 122, 390, 230},
                                                    {202, 230, 390, 230},
                                                    {10, 230, 198, 230},
                                                    {10, 122, 10, 230},
                                                    {10, 10, 10, 118}};

static void flashOuterWall(float *flash, float x, float y, float r)
{
    if (!flash)
        return;
    if (y < 10 + r)
        flash[x < 200 ? 0 : 1] = .18f;
    if (x > 390 - r)
        flash[y < 120 ? 2 : 3] = .18f;
    if (y > 230 - r)
        flash[x < 200 ? 5 : 4] = .18f;
    if (x < 10 + r)
        flash[y < 120 ? 7 : 6] = .18f;
}

static bool moveInArena(Body *b, float r, float dt, float *flash)
{
    bool bounced = false;
    float oldx = b->x, oldy = b->y;
    b->x += b->vx * dt;
    if (arenaSolid(b->x, oldy, r))
    {
        flashOuterWall(flash, b->x, oldy, r);
        b->x = oldx;
        b->vx = -b->vx * .9f;
        bounced = true;
    }
    b->y += b->vy * dt;
    if (arenaSolid(b->x, b->y, r))
    {
        flashOuterWall(flash, b->x, b->y, r);
        b->y = oldy;
        b->vy = -b->vy * .9f;
        bounced = true;
    }
    return bounced;
}

bool arenaMove(Body *b, float r, float dt)
{
    return moveInArena(b, r, dt, NULL);
}

static void burst(Game *g, Body b, unsigned color)
{
    for (int n = 0; n < 15; n++)
        for (int i = 0; i < MAX_SPARKS; i++)
            if (g->sparks[i].life <= 0)
            {
                float a = random01(g) * 2 * PI, s = 20 + random01(g) * 90;
                g->sparks[i] = (Spark){{b.x, b.y, cosf(a) * s, sinf(a) * s}, .25f + random01(g) * .5f, color};
                break;
            }
}

static void spawnWave(Game *g)
{
    g->wave++;
    g->count = 0;
    int n = 3 + g->wave * 2;
    if (n > MAX_ENEMIES)
        n = MAX_ENEMIES;
    for (int i = 0; i < n; i++)
    {
        float x, y;
        do
        {
            x = 25 + random01(g) * 350;
            y = 25 + random01(g) * 190;
        } while (arenaSolid(x, y, 10) ||
                 (x - g->ship.x) * (x - g->ship.x) + (y - g->ship.y) * (y - g->ship.y) < 95 * 95);
        float a = random01(g) * 2 * PI, s = 28 + fminf(g->wave * 4, 48);
        int type = (i + g->wave) % 3;
        g->enemies[i] =
            (Enemy){{x, y, cosf(a) * s, sinf(a) * s}, random01(g) * 6, 1 + random01(g) * 2, type == 2 ? 2 : 1, type};
        g->count++;
    }
}

void gameInit(Game *g, uint32_t seed, int best)
{
    memset(g, 0, sizeof(*g));
    g->rng = seed ? seed : 1234567;
    g->best = best;
    g->ship = (Body){80, 120, 0, 0};
}

void gameStart(Game *g)
{
    int best = g->best;
    uint32_t seed = g->rng;
    gameInit(g, seed, best);
    g->playing = true;
    g->lives = 3;
    g->shield = 3;
    g->angle = -PI / 2;
    spawnWave(g);
}

static void shoot(Game *g, Body b, float a, bool hostile)
{
    for (int i = 0; i < MAX_SHOTS; i++)
        if (g->shots[i].life <= 0)
        {
            float speed = hostile ? 105 : 235;
            g->shots[i] = (Shot){
                {b.x + cosf(a) * 11, b.y + sinf(a) * 11, b.vx * .4f + cosf(a) * speed, b.vy * .4f + sinf(a) * speed},
                hostile ? 2.6f : 1.25f,
                hostile,
                false};
            g->soundEvents |= hostile ? DELTA_SOUND_ENEMY : DELTA_SOUND_PLAYER;
            return;
        }
}

static void hitShip(Game *g)
{
    if (g->shield > 0)
        return;
    burst(g, g->ship, 0x58eaff);
    g->lives--;
    g->ship = (Body){80, 120, 0, 0};
    g->shield = 3;
    g->angle = -PI / 2;
    if (g->lives <= 0)
    {
        g->playing = false;
        if (g->score > g->best)
            g->best = g->score;
    }
}

void gameStep(Game *g, Input in, float dt)
{
    g->soundEvents = 0;
    if (!g->playing || g->paused)
        return;
    if (dt > .034f)
        dt = .034f;
    for (int i = 0; i < ARENA_WALL_SEGMENTS; i++)
        g->wallFlash[i] = fmaxf(0, g->wallFlash[i] - dt);
    g->bounceCooldown = fmaxf(0, g->bounceCooldown - dt);
    g->time += dt;
    g->shield = fmaxf(0, g->shield - dt);
    g->cooldown -= dt;
    g->thrust = in.thrust;
    g->angle += in.turn * 4.2f * dt;
    if (in.thrust)
    {
        g->ship.vx += cosf(g->angle) * 105 * dt;
        g->ship.vy += sinf(g->angle) * 105 * dt;
    }
    float drag = in.brake ? powf(.08f, dt) : powf(.975f, dt);
    g->ship.vx *= drag;
    g->ship.vy *= drag;
    float speed = hypotf(g->ship.vx, g->ship.vy);
    if (speed > 160)
    {
        g->ship.vx *= 160 / speed;
        g->ship.vy *= 160 / speed;
    }
    if (moveInArena(&g->ship, 6, dt, g->wallFlash) && speed > 10 && g->bounceCooldown <= 0)
    {
        g->soundEvents |= DELTA_SOUND_BOUNCE;
        g->bounceCooldown = .10f;
    }
    if (in.fire && g->cooldown <= 0)
    {
        shoot(g, g->ship, g->angle, false);
        g->cooldown = .14f;
    }
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *e = &g->enemies[i];
        if (e->hp <= 0)
            continue;
        if (e->type == 1)
        {
            float a = atan2f(g->ship.y - e->b.y, g->ship.x - e->b.x);
            e->b.vx += cosf(a) * 22 * dt;
            e->b.vy += sinf(a) * 22 * dt;
            float s = hypotf(e->b.vx, e->b.vy);
            if (s > 85)
            {
                e->b.vx *= 85 / s;
                e->b.vy *= 85 / s;
            }
        }
        e->turn += dt * 2;
        moveInArena(&e->b, 8, dt, g->wallFlash);
        e->fire -= dt;
        if (e->fire <= 0 && g->wave >= 2)
        {
            shoot(g, e->b, atan2f(g->ship.y - e->b.y, g->ship.x - e->b.x), true);
            e->fire = fmaxf(1.2f, 4 - g->wave * .15f) + random01(g);
        }
        if (dist2(e->b, g->ship) < 15 * 15)
            hitShip(g);
    }
    for (int i = 0; i < MAX_SHOTS; i++)
    {
        Shot *s = &g->shots[i];
        if (s->life <= 0)
            continue;
        s->life -= dt;
        s->b.x += s->b.vx * dt;
        s->b.y += s->b.vy * dt;
        if (arenaSolid(s->b.x, s->b.y, 1))
        {
            bool hitX = s->b.x < 11 || s->b.x > 389;
            bool hitY = s->b.y < 11 || s->b.y > 229;
            flashOuterWall(g->wallFlash, s->b.x, s->b.y, 1);
            if ((!hitX && !hitY) || s->reflected)
            {
                s->life = 0;
                continue;
            }
            /* Mirror the overshoot; a corner reflects both axes as one impact. */
            if (hitX)
            {
                s->b.x = s->b.x < 11 ? 22 - s->b.x : 778 - s->b.x;
                s->b.vx = -s->b.vx;
            }
            if (hitY)
            {
                s->b.y = s->b.y < 11 ? 22 - s->b.y : 458 - s->b.y;
                s->b.vy = -s->b.vy;
            }
            s->reflected = true;
        }
        if (s->hostile)
        {
            if (dist2(s->b, g->ship) < 8 * 8)
            {
                s->life = 0;
                hitShip(g);
            }
            continue;
        }
        for (int j = 0; j < MAX_ENEMIES; j++)
        {
            Enemy *e = &g->enemies[j];
            if (e->hp > 0 && dist2(s->b, e->b) < 11 * 11)
            {
                s->life = 0;
                e->hp--;
                if (e->hp <= 0)
                {
                    burst(g, e->b, 0xffa34f);
                    g->count--;
                    g->score += 100 * (e->type + 1);
                    if (g->score > g->best)
                        g->best = g->score;
                }
                break;
            }
        }
    }
    for (int i = 0; i < MAX_SPARKS; i++)
    {
        Spark *s = &g->sparks[i];
        if (s->life > 0)
        {
            s->life -= dt;
            s->b.x += s->b.vx * dt;
            s->b.y += s->b.vy * dt;
        }
    }
    if (!g->count && g->playing)
    {
        g->waveDelay += dt;
        if (g->waveDelay > 1.6f)
        {
            g->waveDelay = 0;
            g->shield = fmaxf(g->shield, 2);
            memset(g->shots, 0, sizeof(g->shots));
            spawnWave(g);
        }
    }
}
