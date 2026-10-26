#include "game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void noEnemies(Game *g)
{
    memset(g->enemies, 0, sizeof(g->enemies));
    g->count = 1;
}

static void testWallSegments(void)
{
    const Body contact[ARENA_WALL_SEGMENTS] = {{100, 17, 0, -100},
                                               {300, 17, 0, -100},
                                               {383, 60, 100, 0},
                                               {383, 180, 100, 0},
                                               {300, 223, 0, 100},
                                               {100, 223, 0, 100},
                                               {17, 180, -100, 0},
                                               {17, 60, -100, 0}};
    Game g;
    gameInit(&g, 42, 0);
    for (int edge = 0; edge < ARENA_WALL_SEGMENTS; edge++)
    {
        gameStart(&g);
        noEnemies(&g);
        g.ship = contact[edge];
        gameStep(&g, (Input){0}, .02f);
        assert(g.soundEvents & DELTA_SOUND_BOUNCE);
        for (int i = 0; i < ARENA_WALL_SEGMENTS; i++)
            assert((g.wallFlash[i] > 0) == (i == edge));
        g.paused = true;
        float flash = g.wallFlash[edge];
        gameStep(&g, (Input){0}, .02f);
        assert(g.wallFlash[edge] == flash);
        g.paused = false;
        g.ship = (Body){80, 120, 0, 0};
        for (int i = 0; i < 12; i++)
            gameStep(&g, (Input){0}, .02f);
        assert(g.wallFlash[edge] == 0);
    }
    gameStart(&g);
    noEnemies(&g);
    g.ship = (Body){17, 17, -100, -100};
    gameStep(&g, (Input){0}, .02f);
    assert(g.wallFlash[0] > 0 && g.wallFlash[7] > 0);
    gameStart(&g);
    noEnemies(&g);
    g.ship = (Body){148, 120, 100, 0};
    gameStep(&g, (Input){0}, .02f);
    for (int i = 0; i < ARENA_WALL_SEGMENTS; i++)
        assert(g.wallFlash[i] == 0);
    g.enemies[0] = (Enemy){{19, 60, -100, 0}, 0, 10, 1, 0};
    gameStep(&g, (Input){0}, .02f);
    assert(g.wallFlash[7] > 0);
    gameStart(&g);
    noEnemies(&g);
    g.shots[0] = (Shot){{300, 12, 0, -200}, 1, false, false};
    gameStep(&g, (Input){0}, .02f);
    assert(g.wallFlash[1] > 0 && g.shots[0].life > 0 && g.shots[0].reflected);
}

static void testShotReflection(void)
{
    const Body impacts[] = {
        {12, 60, -200, 0}, {388, 60, 200, 0}, {300, 12, 0, -200}, {300, 228, 0, 200}, {12, 12, -200, -200}};
    for (int hostile = 0; hostile < 2; hostile++)
    {
        for (unsigned i = 0; i < sizeof(impacts) / sizeof(impacts[0]); i++)
        {
            Game g;
            gameInit(&g, 42, 0);
            gameStart(&g);
            noEnemies(&g);
            g.shots[0] = (Shot){impacts[i], 2, hostile, false};
            gameStep(&g, (Input){0}, .02f);
            Shot *shot = &g.shots[0];
            assert(shot->life > 0 && shot->reflected && shot->hostile == (bool)hostile);
            assert(shot->b.vx == -impacts[i].vx && shot->b.vy == -impacts[i].vy);
            assert(!arenaSolid(shot->b.x, shot->b.y, 1));
            gameStep(&g, (Input){0}, .02f);
            assert(shot->life > 0);
            shot->b = impacts[i];
            gameStep(&g, (Input){0}, .02f);
            assert(shot->life == 0);
        }
    }
    Game g;
    gameInit(&g, 42, 0);
    gameStart(&g);
    noEnemies(&g);
    g.shots[0] = (Shot){{153, 120, 200, 0}, 2, false, false};
    gameStep(&g, (Input){0}, .02f);
    assert(g.shots[0].life == 0 && !g.shots[0].reflected);
    /* A reflected shot can still destroy an enemy on its return path. */
    g.shots[0] = (Shot){{12, 60, -200, 0}, 2, false, false};
    gameStep(&g, (Input){0}, .02f);
    g.enemies[0] = (Enemy){{25, 60, 0, 0}, 0, 10, 1, 0};
    gameStep(&g, (Input){0}, .02f);
    assert(g.enemies[0].hp == 0 && g.score == 100);
}

int main(void)
{
    testWallSegments();
    testShotReflection();
    Game g;
    gameInit(&g, 42, 900);
    assert(!g.playing && g.best == 900);
    gameStart(&g);
    assert(g.lives == 3 && g.count == 5 && g.wave == 1);
    for (int i = 0; i < g.count; i++)
        assert(!arenaSolid(g.enemies[i].b.x, g.enemies[i].b.y, 8));
    Body b = {20, 30, -100, 0};
    arenaMove(&b, 6, .1f);
    assert(b.x == 20 && b.vx > 0);
    b = (Body){140, 120, 100, 0};
    arenaMove(&b, 6, .1f);
    assert(b.x == 140 && b.vx < 0);
    noEnemies(&g);
    gameStep(&g, (Input){0, true, false, false}, .02f);
    assert(g.ship.vy < 0);
    float v = g.ship.vy;
    gameStep(&g, (Input){0}, .02f);
    assert(g.ship.vy < 0 && fabsf(g.ship.vy) > fabsf(v) * .99f);
    gameStep(&g, (Input){0, false, false, true}, .02f);
    assert(fabsf(g.ship.vy) < fabsf(v) * .98f);
    g.paused = true;
    Body old = g.ship;
    gameStep(&g, (Input){1, true, true, false}, .02f);
    assert(g.ship.x == old.x && g.ship.y == old.y);
    g.paused = false;
    gameStep(&g, (Input){0, false, true, false}, .02f);
    int shots = 0;
    for (int i = 0; i < MAX_SHOTS; i++)
        shots += g.shots[i].life > 0;
    assert(shots == 1);
    memset(g.shots, 0, sizeof(g.shots));
    g.enemies[0] = (Enemy){{100, 50, 0, 0}, 0, 10, 1, 0};
    g.count = 1;
    g.shots[0] = (Shot){{100, 50, 0, 0}, 1, false, false};
    int score = g.score;
    gameStep(&g, (Input){0}, .01f);
    assert(g.score == score + 100 && g.count == 0);
    for (int i = 0; i < 170; i++)
        gameStep(&g, (Input){0}, .01f);
    assert(g.wave == 2 && g.count == 7);
    noEnemies(&g);
    memset(g.shots, 0, sizeof(g.shots));
    g.shield = 0;
    g.enemies[0] = (Enemy){g.ship, 0, 10, 1, 0};
    int lives = g.lives;
    gameStep(&g, (Input){0}, .01f);
    assert(g.lives == lives - 1 && g.shield > 0);
    gameStep(&g, (Input){0}, .01f);
    assert(g.lives == lives - 1);
    g.shield = 0;
    g.lives = 1;
    g.enemies[0].b = g.ship;
    gameStep(&g, (Input){0}, .01f);
    assert(!g.playing && g.lives == 0);
    gameStart(&g);
    noEnemies(&g);
    g.shield = 0;
    g.shots[0] = (Shot){g.ship, 1, true, false};
    gameStep(&g, (Input){0}, .01f);
    assert(g.lives == 2);
    // Sounds are actual shot/bounce events, rather than repeated input-button events.
    gameStart(&g);
    noEnemies(&g);
    gameStep(&g, (Input){0, false, true, false}, .01f);
    assert(g.soundEvents & DELTA_SOUND_PLAYER);
    gameStep(&g, (Input){0, false, true, false}, .01f);
    assert(!(g.soundEvents & DELTA_SOUND_PLAYER));
    g.ship = (Body){17, 30, -100, 0};
    gameStep(&g, (Input){0}, .02f);
    assert(g.soundEvents & DELTA_SOUND_BOUNCE);
    g.ship = (Body){17, 30, -100, 0};
    gameStep(&g, (Input){0}, .02f);
    assert(!(g.soundEvents & DELTA_SOUND_BOUNCE));
    noEnemies(&g);
    g.wave = 2;
    g.enemies[0] = (Enemy){{300, 40, 0, 0}, 0, 0, 1, 0};
    gameStep(&g, (Input){0}, .01f);
    assert(g.soundEvents & DELTA_SOUND_ENEMY);
    g.paused = true;
    gameStep(&g, (Input){0}, .01f);
    assert(!g.soundEvents);
    // Seeded stress run: many waves, collisions, and projectile recycling.
    for (int run = 1; run <= 40; run++)
    {
        gameInit(&g, run, 0);
        gameStart(&g);
        for (int f = 0; f < 15000; f++)
        {
            if (!g.playing)
                gameStart(&g);
            gameStep(&g, (Input){sinf(f * .007f), true, true, f % 500 < 40}, 1.0f / 60);
            assert(isfinite(g.ship.x) && isfinite(g.ship.y));
            assert(!arenaSolid(g.ship.x, g.ship.y, 5.99f));
            assert(g.count >= 0 && g.count <= MAX_ENEMIES);
            assert(g.lives >= 0 && g.lives <= 3);
        }
    }
    puts(
        "PASS: inertia, brake, walls, obstacle, pause, fire, score, waves, shield, hostile shots, game over; 600000 simulation frames.");
    return 0;
}
