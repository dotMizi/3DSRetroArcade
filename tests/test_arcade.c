#include "arcade.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void safeRescue(Rescue *r)
{
    for (int i = 0; i < TANKS; i++)
        r->tanks[i].alive = false;
}

static void testShieldSoundEvents(void)
{
    Shield s;
    shieldStart(&s, 42, 0);
    assert(s.soundEvents & SHIELD_SOUND_WAVE);
    s.soundEvents = 0;
    s.spawn = 100;
    assert(shieldFire(&s, 30, 100));
    assert(s.soundEvents == SHIELD_SOUND_LAUNCH);
    shieldStep(&s, .01f);
    assert(s.soundEvents & SHIELD_SOUND_LAUNCH);
    s.soundEvents = 0;
    assert(!shieldFire(&s, 30, 100) && !s.soundEvents);
    s.interceptors[0] = (Missile){100, 99, 100, 100, 260, true};
    shieldStep(&s, .01f);
    assert(s.soundEvents & SHIELD_SOUND_BLAST);
    s.soundEvents = 0;
    s.missiles[0] = (Missile){55, 214, 55, 215, 200, true};
    shieldStep(&s, .01f);
    assert(s.soundEvents & SHIELD_SOUND_DAMAGE);
    s.soundEvents = 0;
    s.tally = true;
    s.tallyRow = 0;
    s.delay = 0;
    s.bonusAmmo = 1;
    shieldStep(&s, .03f);
    shieldStep(&s, .03f);
    assert(s.soundEvents & SHIELD_SOUND_BONUS);
}

static void testSmartBombGroundArrival(void)
{
    const float steps[] = {1.f / 120, 1.f / 60, .034f};
    const float targets[] = {30, 55, 200, 345, 370};
    for (unsigned rate = 0; rate < sizeof(steps) / sizeof(steps[0]); rate++)
        for (unsigned target = 0; target < sizeof(targets) / sizeof(targets[0]); target++)
        {
            Shield s;
            shieldStart(&s, 42, 0);
            s.wave = 6;
            s.remaining = s.smartRemaining = 0;
            s.ammo[0] = s.ammo[1] = s.ammo[2] = 0;
            /* Reproduce the screenshot: final bomb over an already destroyed base. */
            s.baseDestroyed[0] = true;
            s.smart[0] = true;
            s.missiles[0] = (Missile){targets[target], 214, targets[target], 215, 28, true};
            for (int frame = 0; frame < 120 && !s.tally; frame++)
                shieldStep(&s, steps[rate]);
            assert(!s.missiles[0].alive && s.tally && s.bonusAmmo == 0 && s.score == 0);
            if (targets[target] == 55)
                assert(s.city[0] == CITY_EXPOSED);
            if (targets[target] == 200)
                assert(s.baseDestroyed[1]);
            if (targets[target] == 370)
                assert(s.baseDestroyed[2]);
            for (int frame = 0; frame < 3000 && s.wave == 6; frame++)
                shieldStep(&s, steps[rate]);
            assert(s.wave == 7 && !s.tally);
        }
}

static void testOriginalWaveCurve(void)
{
    const int missiles[] = {12, 15, 18, 12, 16, 14, 17, 10, 13, 16, 19, 12, 14, 16, 18, 14, 17, 19, 22};
    const int smart[] = {0, 0, 0, 0, 0, 1, 1, 2, 3, 4, 4, 5, 5, 6, 6, 7, 7, 7, 7};
    float lastSpeed = 0;
    for (int wave = 1; wave <= 25; wave++)
    {
        Shield s;
        shieldStart(&s, 42, 0);
        s.wave = wave - 1;
        s.tally = true;
        s.tallyRow = 4;
        s.delay = 1.19f;
        shieldStep(&s, .02f);
        ShieldWaveConfig config = shieldWaveConfig(wave);
        int index = wave < 19 ? wave - 1 : 18;
        assert(s.remaining == missiles[index] && s.smartRemaining == smart[index]);
        assert(config.missileSpeed >= lastSpeed);
        if (wave >= 15)
            assert(config.missileSpeed == 60);
        lastSpeed = config.missileSpeed;
        int actualMissiles = 0, actualSmart = 0;
        while (s.remaining + s.smartRemaining > 0)
        {
            s.spawn = 0;
            shieldStep(&s, .001f);
            for (int i = 0; i < MISSILES; i++)
                if (s.missiles[i].alive)
                {
                    actualSmart += s.smart[i];
                    actualMissiles += !s.smart[i];
                    assert(s.missiles[i].speed == (s.smart[i] ? 28.f : config.missileSpeed));
                    s.missiles[i].alive = false;
                }
        }
        assert(actualMissiles == missiles[index] && actualSmart == smart[index]);
        for (int i = 0; i < 3; i++)
            assert(s.ammo[i] == 10);
    }
    Shield s;
    shieldStart(&s, 42, 0);
    s.wave = 6;
    s.remaining = 0;
    s.smartRemaining = 1;
    s.spawn = 100;
    shieldStep(&s, .01f);
    assert(!s.tally);
    s.smartRemaining = 0;
    s.smart[0] = true;
    s.missiles[0] = (Missile){100, 100, 100, 215, 28, true};
    s.blasts[0] = (Blast){100, 100, .3f, true};
    shieldStep(&s, .01f);
    assert(!s.missiles[0].alive && s.score == 10 && s.tally);
    for (int wave = 8; wave <= 9; wave++)
    {
        shieldStart(&s, 42, 0);
        s.wave = wave;
        s.spawn = 100;
        s.smart[0] = true;
        s.missiles[0] = (Missile){100, 70, 100, 215, 0, true};
        s.blasts[0] = (Blast){100, 100, .3f, true};
        shieldStep(&s, .001f);
        assert(s.missiles[0].alive);
        if (wave == 8)
            assert(s.missiles[0].y < 70);
        else
            assert(s.missiles[0].y == 70);
    }
}

static void testAutomaticBases(void)
{
    Shield s;
    for (int base = 0; base < 3; base++)
    {
        shieldStart(&s, 42, 0);
        assert(shieldFire(&s, 30 + base * 170, 80));
        assert(s.ammo[base] == 9 && s.interceptors[0].x == 30 + base * 170);
    }
    shieldStart(&s, 42, 0);
    s.ammo[0] = 0;
    assert(shieldFire(&s, 30, 80) && s.interceptors[0].x == 200);
    shieldStart(&s, 42, 0);
    s.remaining = 1;
    s.spawn = 100;
    unsigned char cities[6];
    memcpy(cities, s.city, sizeof(cities));
    for (int base = 0; base < 3; base++)
    {
        int x = 30 + base * 170;
        s.missiles[0] = (Missile){x, 214, x, 215, 200, true};
        shieldStep(&s, .01f);
        assert(s.baseDestroyed[base] && s.ammo[base] == 0);
        assert(!memcmp(cities, s.city, sizeof(cities)));
    }
    assert(!shieldFire(&s, 30, 80));
    s.ammo[0] = 10;
    assert(!shieldFire(&s, 30, 80));
    s.ammo[0] = 0;
    s.remaining = 0;
    shieldStep(&s, .01f);
    assert(s.tally && s.bonusAmmo == 0);
    s.tallyRow = 4;
    s.delay = 1.19f;
    shieldStep(&s, .02f);
    for (int i = 0; i < 3; i++)
        assert(!s.baseDestroyed[i] && s.ammo[i] == 10);
    /* Losing the nearest base changes both launch position and travel distance. */
    shieldStart(&s, 42, 0);
    s.baseDestroyed[0] = s.baseDestroyed[1] = true;
    s.ammo[0] = s.ammo[1] = 0;
    assert(shieldFire(&s, 30, 80) && s.interceptors[0].x == 370);
}

static void testIndependentWeapons(void)
{
    Rescue r;
    for (int facing = -1; facing <= 1; facing += 2)
    {
        rescueStart(&r, 0, 1, 0);
        safeRescue(&r);
        r.x = 500;
        r.y = 90;
        r.facing = facing;
        rescueStep(&r, 0, 0, true, true, .01f);
        Bullet *shot = &r.bullets[0], *bomb = &r.bullets[1];
        assert(shot->forward && !shot->hostile && shot->vx * facing > 280 && shot->vy == 0);
        assert(!bomb->forward && !bomb->hostile && bomb->vx == 0 && bomb->vy > 20);
        assert((shot->x - r.x) * facing > 23 && bomb->y > r.y + 11);
        float shotY = shot->y, bombSpeed = bomb->vy;
        for (int frame = 0; frame < 20; frame++)
            rescueStep(&r, 0, 0, true, true, .01f);
        int guns = 0, bombs = 0;
        for (int i = 0; i < BULLETS; i++)
            if (r.bullets[i].life > 0)
            {
                guns += r.bullets[i].forward;
                bombs += !r.bullets[i].forward && !r.bullets[i].hostile;
            }
        assert(guns == 2 && bombs == 1 && shot->y == shotY && bomb->vy > bombSpeed);
    }
}

static void testHomePassengers(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    r.x = 410;
    r.y = RESCUE_LAND_Y;
    for (int i = 0; i < PEOPLE; i++)
        r.people[i].x = 410;
    for (int frame = 0; frame < 500; frame++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.onboard == RESCUE_CAPACITY);
    int waiting = 0;
    for (int i = 0; i < PEOPLE; i++)
        waiting += r.people[i].state == PERSON_WAITING;
    assert(waiting == PEOPLE - RESCUE_CAPACITY);
    r.x = 50;
    rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.onboard == 7 && r.rescued == 1 && r.people[0].state == PERSON_GOING_HOME);
    bool pause = false, wave = false;
    for (int frame = 0; frame < 2500; frame++)
    {
        int rescued = r.rescued;
        rescueStep(&r, 0, 0, false, false, .01f);
        assert(r.rescued - rescued <= 1);
        for (int i = 0; i < RESCUE_CAPACITY; i++)
        {
            HomeWalk *walk = &r.homeWalk[i];
            if (r.people[i].state == PERSON_GOING_HOME)
            {
                pause |= walk->action == HOME_PAUSE;
                wave |= walk->action == HOME_WAVE;
                if (walk->action == HOME_WAVE)
                    assert(walk->facing == (r.x < r.people[i].x ? -1 : 1));
            }
        }
    }
    assert(pause && wave && r.onboard == 0 && r.rescued == 8 && r.score == 1600);
    assert(!r.win && r.homeWalk[0].speed != r.homeWalk[1].speed);
    for (int i = 0; i < RESCUE_CAPACITY; i++)
        assert(r.people[i].state == PERSON_INSIDE);
    /* Final delivery must finish its walk before switching to the score screen. */
    for (int i = 8; i < PEOPLE; i++)
        r.people[i].state = PERSON_ABOARD;
    r.onboard = 4;
    while (r.onboard)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.rescued == PEOPLE && !r.win);
    for (int frame = 0; frame < 2500 && !r.win; frame++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.win && r.score == 2400);
}

static void testGroundPatrols(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    r.shield = 100;
    float person = r.people[1].x;
    float tank = r.tanks[0].x;
    for (int i = 0; i < 50; i++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.tanks[0].x > tank && r.people[1].x != person);
    assert(fabsf(r.people[1].x - r.peopleHome[1]) <= 14.1f);
    r.tanks[0].x = 1554.9f;
    r.tankVelocity[0] = 20;
    rescueStep(&r, 0, 0, false, false, .02f);
    assert(r.tanks[0].x <= 1555 && r.tankVelocity[0] < 0);
    r.tanks[0].x = 230.1f;
    r.tankVelocity[0] = -20;
    rescueStep(&r, 0, 0, false, false, .02f);
    assert(r.tanks[0].x >= 230 && r.tankVelocity[0] > 0);
}

static void testRescueBombs(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    r.x = 500;
    r.y = 90;
    r.vx = 100;
    r.facing = -1;
    rescueStep(&r, 0, 0, false, true, .01f);
    Bullet *bomb = &r.bullets[0];
    assert(bomb->life > 0 && !bomb->hostile);
    assert(bomb->vx == r.vx && bomb->vx > 0);
    assert(fabsf(bomb->x - (r.x + r.vx * .01f)) < .001f);
    assert(bomb->y > r.y + 11 && bomb->vy > 20);
    float vy = bomb->vy;
    for (int i = 0; i < 10; i++)
        rescueStep(&r, 0, 0, false, true, .01f);
    assert(bomb->vy > vy + 14);
    int count = 0;
    for (int i = 0; i < BULLETS; i++)
        count += r.bullets[i].life > 0;
    assert(count == 1);
    for (int i = 0; i < 200; i++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(bomb->life <= 0);
    /* A stationary aircraft drops vertically; a tank is hit only under that path. */
    for (int miss = 0; miss < 2; miss++)
    {
        rescueStart(&r, 0, 1, 0);
        safeRescue(&r);
        r.x = 500;
        r.y = 90;
        r.tanks[0] = (Tank){500 + miss * 50, 100, true};
        r.tankVelocity[0] = 0;
        rescueStep(&r, 0, 0, false, true, .01f);
        assert(r.bullets[0].vx == 0);
        for (int i = 0; i < 115; i++)
            rescueStep(&r, 0, 0, false, false, .01f);
        assert(r.tanks[0].alive == (bool)miss);
        assert(r.score == (miss ? 0 : 100));
        bool impact = false;
        for (int i = 0; i < 8; i++)
            impact |= r.bombImpacts[i].life > 0;
        assert(impact);
    }
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    r.y = RESCUE_LAND_Y;
    rescueStep(&r, 0, 0, false, true, .01f);
    assert(r.bullets[0].life == 0);
    /* Enemy projectiles remain aimed shots, without bomb gravity. */
    r.bullets[0] = (Bullet){300, 80, 20, -30, 1, true, false};
    rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.bullets[0].vy == -30);
}

int main(void)
{
    testRescueBombs();
    testGroundPatrols();
    testHomePassengers();
    testIndependentWeapons();
    testAutomaticBases();
    testOriginalWaveCurve();
    testSmartBombGroundArrival();
    testShieldSoundEvents();
    assert(arcadeHit(13, 67) == 0 && arcadeHit(104, 170) == 0);
    assert(arcadeHit(114, 80) == 1 && arcadeHit(215, 80) == 2);
    assert(arcadeHit(105, 80) == -1 && arcadeHit(10, 20) == -1 && arcadeHit(319, 170) == -1);
    Shield s;
    shieldStart(&s, 42, 500);
    assert(s.wave == 1 && s.ammo[0] == 10 && s.best == 500);
    assert(shieldFire(&s, 100, 70) && s.ammo[0] == 9);
    assert(!shieldFire(&s, 100, 70));
    for (int i = 0; i < 70; i++)
        shieldStep(&s, 1.f / 60);
    bool boom = false;
    for (int i = 0; i < BLASTS; i++)
        boom |= s.blasts[i].alive;
    assert(boom);
    shieldStart(&s, 3, 0);
    s.remaining = 0;
    s.blasts[0] = (Blast){100, 100, .3f, true};
    s.missiles[0] = (Missile){100, 100, 55, 215, 30, true};
    shieldStep(&s, .01f);
    assert(!s.missiles[0].alive && s.score == 10 && s.tally);
    for (int i = 0; i < 850; i++)
        shieldStep(&s, .01f);
    assert(s.wave == 2 && s.ammo[0] == 10);
    s.ammo[0] = s.ammo[1] = s.ammo[2] = 0;
    assert(!shieldFire(&s, 100, 100));
    shieldStart(&s, 3, 0);
    s.remaining = 0;
    for (int hit = 1; hit <= 3; hit++)
    {
        for (int i = 0; i < 6; i++)
            s.missiles[i] = (Missile){55 + i * 58, 214, 55 + i * 58, 215, 200, true};
        shieldStep(&s, .01f);
        for (int i = 0; i < 6; i++)
            assert(s.city[i] == CITY_DOMED - hit);
        if (hit < 3)
        {
            assert(!s.over);
            s.tally = false;
            s.delay = 0;
        }
    }
    assert(s.over);
    // Mixed damage states: 27 unused rounds, two domed, two exposed, one ruin.
    shieldStart(&s, 3, 0);
    s.remaining = 0;
    s.ammo[0] = 10;
    s.ammo[1] = 8;
    s.ammo[2] = 9;
    unsigned char states[6] = {CITY_DOMED, CITY_EXPOSED, CITY_RUIN, CITY_GONE, CITY_DOMED, CITY_EXPOSED};
    memcpy(s.city, states, sizeof(states));
    shieldStep(&s, .01f);
    assert(s.tally && s.bonusAmmo == 27 && s.bonusTotal == 5770 && s.score == 0 && s.best == 0 && s.bonusPaid == 0);
    assert(s.bonusCities[0] == 1 && s.bonusCities[1] == 2 && s.bonusCities[2] == 2);
    assert(!shieldFire(&s, 100, 100));
    int previous = 0;
    bool intermediate = false;
    for (int i = 0; i < 100; i++)
    {
        shieldStep(&s, .01f);
        assert(s.score >= previous);
        previous = s.score;
        if (s.score > 0 && s.score < 5770)
            intermediate = true;
    }
    assert(intermediate && s.score < 5770 && s.wave == 1);
    for (int i = 0; i < 750; i++)
        shieldStep(&s, .01f);
    assert(s.bonusPaid == 5770 && s.bonusShown[0] == 27 && s.bonusShown[1] == 2 && s.bonusShown[2] == 2 &&
           s.bonusShown[3] == 1);
    assert(s.wave == 2 && !s.tally && s.ammo[0] == 10 && s.score == 5770 && !memcmp(s.city, states, sizeof(states)));
    // Three cities visibly advance 1/1500, 2/3000, 3/4500, with no duplicate awards.
    shieldStart(&s, 42, 0);
    s.remaining = 0;
    s.ammo[0] = s.ammo[1] = s.ammo[2] = 0;
    for (int i = 0; i < 6; i++)
        s.city[i] = i < 3 ? CITY_DOMED : CITY_GONE;
    shieldStep(&s, .01f);
    int seen = 0;
    for (int frame = 0; frame < 1500 && s.tally; frame++)
    {
        shieldStep(&s, .01f);
        if (s.bonusShown[1] > seen)
        {
            seen++;
            assert(s.bonusShown[1] == seen && s.score == seen * 1500);
        }
    }
    assert(seen == 3 && s.score == 4500 && s.bonusPaid == 4500 && s.wave == 2);
    // Trails retain their launch point as missiles travel; reused slots get a new origin.
    shieldStart(&s, 42, 0);
    for (int i = 0; i < 30; i++)
        shieldStep(&s, .034f);
    assert(s.missiles[0].alive && s.missiles[0].y > SHIELD_ENTRY_Y);
    float origin = s.trailStartX[0];
    for (int i = 0; i < 30; i++)
        shieldStep(&s, .034f);
    assert(s.trailStartX[0] == origin);
    s.blasts[0] = (Blast){s.missiles[0].x, s.missiles[0].y, .3f, true};
    shieldStep(&s, .01f);
    assert(!s.missiles[0].alive);
    memset(s.blasts, 0, sizeof(s.blasts));
    s.spawn = 0;
    shieldStep(&s, .01f);
    assert(s.missiles[0].alive && s.trailStartX[0] != origin);
    Rescue r;
    rescueStart(&r, 900, 1, 0);
    safeRescue(&r);
    r.x = 418;
    r.y = RESCUE_LAND_Y;
    for (int i = 0; i < 120; i++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.onboard == 3);
    r.x = 80;
    r.vx = r.vy = 0;
    for (int i = 0; i < 300; i++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.onboard == 0 && r.rescued == 3 && r.score == 600);
    // Pick up the remaining groups, then unload them at the base.
    for (int group = 1; group < 4; group++)
    {
        r.x = 418 + group * 300;
        r.y = RESCUE_LAND_Y;
        r.vx = r.vy = 0;
        for (int i = 0; i < 120; i++)
            rescueStep(&r, 0, 0, false, false, .01f);
        assert(r.onboard == 3);
        r.x = 80;
        for (int i = 0; i < 300; i++)
            rescueStep(&r, 0, 0, false, false, .01f);
    }
    for (int i = 0; i < 2000 && !r.win; i++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.win && r.rescued == 12 && r.best >= 2400);
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    r.shield = 0;
    r.y = RESCUE_LAND_Y - 1;
    r.vy = 95;
    rescueStep(&r, 0, 0, false, false, .03f);
    assert(r.lives == 2 && r.shield > 0);
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    r.x = 418;
    r.y = RESCUE_LAND_Y;
    for (int i = 0; i < 120; i++)
        rescueStep(&r, 0, 0, false, false, .01f);
    r.shield = 0;
    r.bullets[0] = (Bullet){r.x, r.y, 0, 0, 1, true, false};
    rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.onboard == 0 && r.people[0].state == 0 && r.lives == 2);
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    r.tanks[0] = (Tank){300, 100, true};
    r.bullets[0] = (Bullet){300, 201, 0, 0, 1, false, false};
    rescueStep(&r, 0, 0, false, false, .01f);
    assert(!r.tanks[0].alive && r.score == 100);
    // The sprite faces sideways at rest and turns through the front only briefly.
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    assert(r.yaw == 1);
    for (int i = 0; i < 9; i++)
        rescueStep(&r, -1, 0, false, false, .02f);
    assert(r.yaw == 0 && r.facing == -1 && r.frontHold > .15f);
    for (int i = 0; i < 7; i++)
    {
        rescueStep(&r, -1, 0, false, false, .02f);
        assert(r.yaw == 0);
    }
    rescueStep(&r, -1, 0, false, false, .02f);
    for (int i = 0; i < 9; i++)
        rescueStep(&r, -1, 0, false, false, .02f);
    assert(r.yaw == -1);
    for (int i = 0; i < 100; i++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.yaw == -1);
    rescueStep(&r, 1, 0, false, false, .02f);
    float yaw = r.yaw;
    rescueStep(&r, -1, 0, false, false, .01f);
    assert(r.yaw < yaw && r.yaw >= -1);
    for (int i = 0; i < 60; i++)
        rescueStep(&r, 1, 0, false, false, .01f);
    assert(r.yaw == 1);
    // Lean follows steering; release levels the helicopter even while it drifts.
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    for (int i = 0; i < 20; i++)
        rescueStep(&r, 1, 0, false, false, .01f);
    assert(r.tilt == .22f && r.vx > 0);
    for (int i = 0; i < 20; i++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.tilt == 0 && r.vx > 0);
    for (int i = 0; i < 40; i++)
        rescueStep(&r, -1, 0, false, false, .01f);
    assert(r.tilt == -.22f);
    for (int i = 0; i < 20; i++)
        rescueStep(&r, 0, -1, false, false, .01f);
    assert(r.tilt == 0);
    for (int seed = 1; seed <= 20; seed++)
    {
        shieldStart(&s, seed, 0);
        rescueStart(&r, 0, 1, 0);
        for (int f = 0; f < 12000; f++)
        {
            if (s.over)
                shieldStart(&s, seed, s.best);
            if (r.over || r.win)
                rescueStart(&r, r.best, 1, 0);
            shieldFire(&s, 200 + 150 * sinf(f * .01f), 70 + 60 * cosf(f * .007f));
            shieldStep(&s, 1.f / 60);
            rescueStep(&r, sinf(f * .003f), cosf(f * .009f), false, true, 1.f / 60);
            assert(isfinite(r.x) && isfinite(r.y) && r.x >= 20 && r.x <= 1580 && r.y >= 35 && r.y <= RESCUE_LAND_Y);
            assert(r.onboard >= 0 && r.onboard <= RESCUE_CAPACITY && r.rescued <= 12 && r.lives >= 0);
            for (int i = 0; i < 3; i++)
                assert(s.ammo[i] >= 0 && s.ammo[i] <= 10);
        }
    }
    puts(
        "PASS: menu hitboxes, ammo, cooldown, explosions, chain hits, city loss, waves, rescue boarding/unloading/victory, crashes, return of passengers, tanks; 480000 simulation frames.");
    return 0;
}
