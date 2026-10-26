#include "arcade.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void safeRescue(Rescue *r)
{
    r->tankSpawnDelay = 1e9f;
    /* Legacy rescue-flow fixtures explicitly put all people outside. */
    for (int i = 0; i < PEOPLE; i++)
        if (r->people[i].state == PERSON_SHELTERED)
        {
            r->people[i].state = PERSON_WAITING;
            r->people[i].x = RESCUE_FIRST_GROUP_X + (i / RESCUE_PEOPLE_PER_HOUSE) * RESCUE_GROUP_SPACING +
                             (i % RESCUE_PEOPLE_PER_HOUSE) * 18;
            r->peopleHome[i] = r->people[i].x;
        }
    for (int i = 0; i < RESCUE_HOUSES; i++)
        r->houses[i].residents = 0;
    for (int i = 0; i < TANKS; i++)
        r->tanks[i].alive = false;
}

static void testRescueAirWaves(void)
{
    for (int stage = 1; stage <= 5; stage++)
    {
        Rescue r;
        rescueStart(&r, 0, stage, 0);
        r.x = 1600;
        r.y = 90;
        r.shield = 1000;
        r.tankSpawnDelay = 1e9f;
        bool sawMine = false;
        RescueThreatConfig config = rescueThreatConfig(stage);
        assert(config.jets == (stage == 1 ? 0 : stage == 2 ? 1 : 2));
        assert(config.drones == (stage < 3 ? 0 : stage < 5 ? 2 : 3));
        for (int frame = 0; frame < 4500; frame++)
        {
            rescueStep(&r, 0, 0, false, false, .02f);
            int jets = 0, drones = 0;
            for (int i = 0; i < RESCUE_AIRCRAFT; i++)
                if (r.aircraft[i].alive)
                {
                    assert(isfinite(r.aircraft[i].x) && isfinite(r.aircraft[i].y));
                    jets += r.aircraft[i].kind == RESCUE_JET;
                    drones += r.aircraft[i].kind == RESCUE_DRONE;
                }
            assert(jets <= config.jets && drones <= config.drones);
            if (frame == 4499)
                assert(jets == config.jets && drones == config.drones);
            for (int mine = 0; mine < RESCUE_MINES; mine++)
                sawMine |= r.mines[mine].alive;
        }
        assert(sawMine == (stage >= 4));
    }
    Rescue r;
    rescueStart(&r, 0, 3, 0);
    r.x = 1200;
    r.y = 90;
    r.shield = 1000;
    r.tankSpawnDelay = r.aircraftSpawnDelay = 1e9f;
    for (int kind = RESCUE_JET; kind <= RESCUE_DRONE; kind++)
    {
        r.aircraft[0] = (RescueAircraft){1000, 90, 1000, 1000, kind, kind == RESCUE_JET ? 3 : 2, 1, true};
        int health = r.aircraft[0].hp;
        for (int hit = 0; hit < health; hit++)
        {
            r.bullets[0] = (Bullet){r.aircraft[0].x, r.aircraft[0].y, 0, 0, 1, false, true};
            rescueStep(&r, 0, 0, false, false, .001f);
        }
        assert(!r.aircraft[0].alive);
    }
    assert(r.jetsDestroyed == 1 && r.dronesDestroyed == 1 && r.score == 450);
    r.shield = 0;
    r.aircraft[0] = (RescueAircraft){r.x, r.y, 1000, 1000, RESCUE_JET, 3, 1, true};
    r.aircraft[1] = (RescueAircraft){r.x, r.y, 1000, 1000, RESCUE_DRONE, 2, 1, true};
    rescueStep(&r, 0, 0, false, false, .001f);
    assert(r.lives == 3 && !r.crashTimer);
    r.aircraft[0].x = r.x - 100;
    r.aircraft[0].fire = 0;
    r.shield = 100;
    rescueStep(&r, 0, 0, false, false, .001f);
    bool shot = false;
    for (int i = 0; i < BULLETS; i++)
        shot |= r.bullets[i].hostile && r.bullets[i].life > 0;
    assert(shot);
    r.mines[0] = (RescueMine){r.x + 30, r.y, 10, true};
    r.bullets[0] = (Bullet){r.x + 30, r.y, 0, 0, 1, false, true};
    rescueStep(&r, 0, 0, false, false, .001f);
    assert(!r.mines[0].alive);
    r.shield = 0;
    r.mines[0] = (RescueMine){r.x, r.y, 10, true};
    rescueStep(&r, 0, 0, false, false, .001f);
    assert(!r.mines[0].alive && r.lives == 2 && r.crashTimer > 0);
    rescueStart(&r, 0, 2, 0);
    r.x = 1600;
    r.y = 90;
    r.invincible = true;
    r.tankSpawnDelay = r.aircraftSpawnDelay = 1e9f;
    r.aircraft[0] = (RescueAircraft){1500, 90, 1000, 1000, RESCUE_JET, 3, 1, true};
    int passes = 0;
    for (int frame = 0; frame < 1000; frame++)
    {
        float previousX = r.aircraft[0].x;
        float previousY = r.aircraft[0].y;
        r.y = frame % 100 < 50 ? 50 : 145;
        int direction = r.aircraft[0].facing;
        rescueStep(&r, 0, 0, false, false, .02f);
        assert(fabsf(r.aircraft[0].y - previousY) <= 8 * .02f + .001f);
        if (r.aircraft[0].facing == direction)
            assert(fabsf((r.aircraft[0].x - previousX) - direction * 196 * .02f) < .01f);
        else
        {
            assert(r.aircraft[0].x < r.camera || r.aircraft[0].x > r.camera + 400);
            passes++;
        }
    }
    assert(passes >= 1 && passes <= 2);
    RescueThreatConfig late = rescueThreatConfig(1000);
    assert(late.jets + late.drones <= RESCUE_AIRCRAFT && late.tanks <= TANKS);
}

static void testEarlyTankAccuracy(void)
{
    float error[2] = {0}, reload[2] = {0}, speed[2] = {0};
    for (int difficulty = 0; difficulty < 2; difficulty++)
        for (int sample = 0; sample < 32; sample++)
        {
            Rescue r;
            rescueStart(&r, sample * 997, difficulty ? 10 : 1, 0);
            r.x = 1500;
            r.y = 80;
            r.shield = 100;
            r.tankSpawnDelay = 1e9f;
            for (int house = 0; house < RESCUE_HOUSES; house++)
                r.houses[house].burning = true;
            r.tanks[0] = (Tank){1300, 0, true};
            rescueStep(&r, 0, 0, false, false, .01f);
            Bullet *shot = &r.bullets[0];
            assert(shot->life > 0 && shot->hostile);
            float direct = atan2f(r.y - RESCUE_TANK_Y, r.x - r.tanks[0].x);
            error[difficulty] += fabsf(atan2f(shot->vy, shot->vx) - direct);
            reload[difficulty] += r.tanks[0].fire;
            speed[difficulty] += hypotf(shot->vx, shot->vy);
        }
    assert(error[0] > error[1] * 3);
    assert(reload[0] > reload[1] + 32 * 2);
    assert(speed[0] < speed[1]);
}

static void testRescueWaveProgression(void)
{
    Rescue r;
    rescueStart(&r, 500, 1, 500);
    r.lives = 2;
    for (int round = 1; round <= 3; round++)
    {
        int startScore = r.score;
        r.rescued = PEOPLE;
        r.tanksDestroyed = 2;
        r.score += PEOPLE * 200 + 200;
        for (int i = 0; i < PEOPLE; i++)
            r.people[i].state = PERSON_INSIDE;
        rescueStep(&r, 0, 0, false, false, .01f);
        assert(r.win && r.tally && !r.over && r.tallyScore == startScore);
        int earned = r.score;
        bool intermediate = false, fullyCounted = false;
        for (int frame = 0; frame < 1500 && r.stage == round; frame++)
        {
            rescueStep(&r, 1, 1, true, true, .01f);
            assert(r.score == earned && r.lives == 2);
            if (r.stage == round)
            {
                intermediate |= r.tallyScore > startScore && r.tallyScore < earned;
                if (r.tallyRow == 5)
                {
                    assert(r.tallyScore == earned && r.tallyShown[0] == PEOPLE && r.tallyShown[2] == 2);
                    fullyCounted = true;
                }
            }
        }
        assert(intermediate && fullyCounted && r.stage == round + 1 && !r.tally && !r.win);
        assert(r.lives == 2 && r.score == earned && r.stageStartScore == earned && !r.rescued && !r.lost);
        for (int i = 0; i < RESCUE_HOUSES; i++)
            assert(!r.houses[i].burning && r.houses[i].residents == 8);
    }
}

static void testPeacefulHouseEvacuation(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    assert(PEOPLE == 64 && RESCUE_CAPACITY == 16);
    for (int house = 0; house < RESCUE_HOUSES; house++)
    {
        int outside = 0, hidden = 0;
        for (int person = house * RESCUE_PEOPLE_PER_HOUSE; person < (house + 1) * RESCUE_PEOPLE_PER_HOUSE; person++)
        {
            outside += r.people[person].state == PERSON_WAITING;
            hidden += r.people[person].state == PERSON_SHELTERED;
        }
        assert(outside == 8 && hidden == 8 && r.houses[house].residents == 8);
    }
    r.tankSpawnDelay = 1e9f;
    r.x = r.houses[0].x - 80;
    r.y = RESCUE_LAND_Y;
    rescueStep(&r, 0, 0, false, false, .02f);
    assert(r.houses[0].evacuating && !r.houses[0].burning);
    assert(r.houses[0].residents == 7);
    for (int frame = 0; frame < 50; frame++)
        rescueStep(&r, 0, 0, false, false, .02f);
    assert(r.houses[0].residents == 7);
    for (int frame = 0; frame < 1500; frame++)
        rescueStep(&r, 0, 0, false, false, .02f);
    assert(r.houses[0].residents == 0 && !r.houses[0].burning && !r.lost);
    assert(!r.houses[1].evacuating && r.houses[1].residents == 8);
    r.x = 80;
    rescueStep(&r, 0, 0, false, false, .02f);
    assert(r.camera == 0 && !r.houses[0].burning && r.houses[0].residents == 0);
}

static void testPersistentRescueHouses(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    int outside = 0, hidden = 0, residents = 0;
    for (int i = 0; i < PEOPLE; i++)
    {
        outside += r.people[i].state == PERSON_WAITING;
        hidden += r.people[i].state == PERSON_SHELTERED;
    }
    for (int i = 0; i < RESCUE_HOUSES; i++)
        residents += r.houses[i].residents;
    assert(outside == PEOPLE / 2 && hidden == PEOPLE / 2 && residents == hidden);
    r.tankSpawnDelay = 1e9f;
    r.x = r.houses[0].x + 100;
    r.y = 60;
    r.shield = 1000;
    int originalResidents = r.houses[0].residents;
    r.bullets[0] = (Bullet){r.houses[0].x, RESCUE_HORIZON - 10, 0, 0, 1, true, false};
    rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.houses[0].burning && r.houses[0].residents == originalResidents);
    assert(r.soundEvents & RESCUE_SOUND_HOUSE_HIT);
    r.x = 80;
    rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.houses[0].residents == 0);
    for (int frame = 0; frame < 1600; frame++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.camera == 0 && r.houses[0].burning && r.houses[0].residents == 0);
    assert(r.people[8].state == PERSON_WAITING || r.people[8].state == PERSON_FLEEING);
    assert(!r.houses[1].burning && r.houses[1].residents == 8);
    assert(!r.houses[2].burning && r.houses[2].residents == 8);
    r.x = r.houses[0].x + 100;
    rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.houses[0].burning && r.houses[0].residents == 0);
    /* A visible tank can ignite a visible house through actual projectile travel. */
    rescueStart(&r, 0, 1, 0);
    r.tankSpawnDelay = 1e9f;
    r.x = r.houses[0].x + 100;
    r.y = 60;
    r.shield = 1000;
    r.tanks[0] = (Tank){r.houses[0].x - 100, 0, true};
    r.tankVelocity[0] = 0;
    for (int frame = 0; frame < 1500 && !r.houses[0].burning; frame++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.houses[0].burning);
}

static void testDisembarkedPassengerProtection(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    r.x = 80;
    r.y = RESCUE_LAND_Y;
    r.onboard = 1;
    r.people[0].state = PERSON_ABOARD;
    rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.people[0].state == PERSON_GOING_HOME && r.rescued == 1 && r.score == 200);
    /* Repeated landings directly on the passenger cannot undo a safe delivery. */
    for (int landing = 0; landing < 5; landing++)
    {
        r.x = r.people[0].x;
        r.y = RESCUE_LAND_Y - 1;
        r.vy = 40;
        rescueStep(&r, 0, 0, false, false, .034f);
        assert(r.people[0].state == PERSON_GOING_HOME);
        assert(r.rescued == 1 && r.score == 200 && !r.lost && !r.ouchLife[0]);
    }
    for (int frame = 0; frame < 2000; frame++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.people[0].state == PERSON_INSIDE && r.rescued == 1 && !r.lost);
}

static void testExtendedFrontAndLandingLoss(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    assert(RESCUE_WORLD_WIDTH == 3200 && RESCUE_FENCE_X > 300);
    for (int i = 0; i < TANKS; i++)
        assert(!r.tanks[i].alive);
    r.x = 1600;
    r.y = 80;
    r.shield = 1000;
    int appearances = 0, lastActive = 0;
    for (int frame = 0; frame < 10000; frame++)
    {
        rescueStep(&r, 0, 0, false, false, .02f);
        int active = 0;
        for (int i = 0; i < TANKS; i++)
            active += r.tanks[i].alive;
        if (active > lastActive)
            appearances++;
        lastActive = active;
        assert(active <= 2);
    }
    assert(appearances >= 3 && appearances <= 18);
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    r.x = RESCUE_FIRST_GROUP_X;
    r.y = RESCUE_LAND_Y - 1;
    r.vy = 40;
    rescueStep(&r, 0, 0, false, false, .034f);
    assert(r.people[0].state == PERSON_LOST && r.lost >= 1 && r.ouchLife[0] > 0);
    assert(r.soundEvents & RESCUE_SOUND_OUCH);
    int lost = r.lost;
    for (int frame = 0; frame < 100; frame++)
        rescueStep(&r, 0, 0, false, false, .02f);
    assert(r.people[0].state == PERSON_LOST && r.lost == lost && r.ouchLife[0] == 0);
    /* A safe landing beside the group can still board survivors normally. */
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    r.x = RESCUE_FIRST_GROUP_X - 30;
    r.y = RESCUE_LAND_Y;
    for (int frame = 0; frame < 300; frame++)
        rescueStep(&r, 0, 0, false, false, .02f);
    assert(r.onboard > 0 && !r.lost);
    r.x = RESCUE_WORLD_WIDTH - 21;
    r.vx = 140;
    rescueStep(&r, 1, 0, false, false, .034f);
    assert(r.x == RESCUE_WORLD_WIDTH - 20 && r.camera == RESCUE_WORLD_WIDTH - 400);
}

static void testBaseBoundary(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    r.x = 80;
    r.y = RESCUE_LAND_Y;
    r.shield = 1000;
    assert(RESCUE_FENCE_X > 165 + 100);
    for (int i = 0; i < TANKS; i++)
    {
        r.tanks[i].alive = true;
        r.tanks[i].x = RESCUE_TANK_MIN_X + .1f;
        r.tankVelocity[i] = -24;
        r.tanks[i].fire = 1000;
    }
    for (int frame = 0; frame < 15000; frame++)
    {
        rescueStep(&r, 0, 0, false, false, .02f);
        for (int i = 0; i < TANKS; i++)
        {
            assert(r.tanks[i].x >= RESCUE_TANK_MIN_X && r.tanks[i].x <= RESCUE_WORLD_WIDTH - 45);
            assert(r.tanks[i].x - 13 > RESCUE_FENCE_X + 12);
        }
    }
}

static void testRescueCrashLosses(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    r.x = 500;
    r.y = 90;
    r.camera = 300;
    r.shield = 0;
    r.onboard = 2;
    r.people[0].state = r.people[1].state = PERSON_ABOARD;
    r.people[2].state = PERSON_INSIDE;
    r.rescued = 1;
    r.bullets[0] = (Bullet){r.x, r.y, 0, 0, 1, true, false};
    rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.crashTimer > 0 && !r.over && r.lives == 2 && r.lost == 2 && r.onboard == 0);
    assert(r.people[0].state == PERSON_LOST && r.people[1].state == PERSON_LOST);
    assert(r.people[2].state == PERSON_INSIDE && r.people[3].state == PERSON_WAITING);
    float x = r.x, y = r.y;
    rescueStep(&r, 1, 1, true, true, .01f);
    assert(r.x == x && r.y == y && r.lost == 2 && r.lives == 2);
    for (int i = 0; i < 100; i++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.crashTimer == 0 && r.x == 80 && r.y == 130 && r.shield > 0);
    assert(r.people[0].state == PERSON_LOST && r.people[1].state == PERSON_LOST);
    r.lives = 1;
    r.shield = 0;
    r.bullets[0] = (Bullet){r.x, r.y, 0, 0, 1, true, false};
    rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.lives == 0 && r.crashTimer > 0 && !r.over);
    for (int i = 0; i < 100; i++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.over);
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    assert(!r.lost && !r.crashTimer);
    for (int i = 0; i < PEOPLE; i++)
        r.people[i].state = i < 3 ? PERSON_LOST : PERSON_INSIDE;
    r.lost = 3;
    r.rescued = PEOPLE - 3;
    rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.win && !r.score);
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
            for (int frame = 0; frame < 4500 && s.wave == 6; frame++)
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
    /* Losing the nearest base changes both launch position and travel distance.
   */
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
    for (int frame = 0; frame < 700; frame++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.onboard == RESCUE_CAPACITY);
    int waiting = 0;
    for (int i = 0; i < PEOPLE; i++)
        waiting += r.people[i].state == PERSON_WAITING;
    assert(waiting == PEOPLE - RESCUE_CAPACITY);
    r.x = 50;
    rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.onboard == RESCUE_CAPACITY - 1 && r.rescued == 1 && r.people[0].state == PERSON_GOING_HOME);
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
    assert(pause && wave && r.onboard == 0 && r.rescued == RESCUE_CAPACITY && r.score == RESCUE_CAPACITY * 200);
    assert(!r.win && r.homeWalk[0].speed != r.homeWalk[1].speed);
    for (int i = 0; i < RESCUE_CAPACITY; i++)
        assert(r.people[i].state == PERSON_INSIDE);
    /* Final delivery must finish its walk before switching to the score screen.
   */
    for (int group = 1; group < PEOPLE / RESCUE_CAPACITY; group++)
    {
        for (int i = group * RESCUE_CAPACITY; i < (group + 1) * RESCUE_CAPACITY; i++)
            r.people[i].state = PERSON_ABOARD;
        r.onboard = RESCUE_CAPACITY;
        while (r.onboard)
            rescueStep(&r, 0, 0, false, false, .01f);
    }
    assert(r.rescued == PEOPLE && !r.win);
    for (int frame = 0; frame < 2500 && !r.win; frame++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.win && r.score == PEOPLE * 200);
}

static void testGroundPatrols(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    r.shield = 100;
    float person = r.people[1].x;
    r.tanks[0] = (Tank){RESCUE_TANK_MIN_X + 20, 100, true};
    r.tankVelocity[0] = 20;
    float tank = r.tanks[0].x;
    for (int i = 0; i < 50; i++)
        rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.tanks[0].x > tank && r.people[1].x != person);
    assert(fabsf(r.people[1].x - r.peopleHome[1]) <= 14.1f);
    r.tanks[0].x = RESCUE_WORLD_WIDTH - 45.1f;
    r.tankVelocity[0] = 20;
    rescueStep(&r, 0, 0, false, false, .02f);
    assert(r.tanks[0].x <= RESCUE_WORLD_WIDTH - 45 && r.tankVelocity[0] < 0);
    r.tanks[0].x = RESCUE_TANK_MIN_X + .1f;
    r.tankVelocity[0] = -20;
    rescueStep(&r, 0, 0, false, false, .02f);
    assert(r.tanks[0].x >= RESCUE_TANK_MIN_X && r.tankVelocity[0] > 0);
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
    /* A stationary aircraft drops vertically; a tank is hit only under that path.
   */
    for (int miss = 0; miss < 2; miss++)
    {
        rescueStart(&r, 0, 1, 0);
        safeRescue(&r);
        r.x = 500;
        r.y = 90;
        r.x = 1000;
        r.tanks[0] = (Tank){1000 + miss * 50, 100, true};
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
    testRescueCrashLosses();
    testBaseBoundary();
    testExtendedFrontAndLandingLoss();
    testDisembarkedPassengerProtection();
    testPersistentRescueHouses();
    testPeacefulHouseEvacuation();
    testRescueWaveProgression();
    testEarlyTankAccuracy();
    testRescueAirWaves();
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
    // Three cities visibly advance 1/1500, 2/3000, 3/4500, with no duplicate
    // awards.
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
    // Trails retain their launch point as missiles travel; reused slots get a new
    // origin.
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
    r.x = RESCUE_FIRST_GROUP_X + 18;
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
        r.x = RESCUE_FIRST_GROUP_X + 18 + group * RESCUE_GROUP_SPACING;
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
    assert(!r.win && r.rescued == 12 && r.best >= 2400);
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    r.shield = 0;
    r.y = RESCUE_LAND_Y - 1;
    r.vy = 95;
    rescueStep(&r, 0, 0, false, false, .03f);
    assert(r.lives == 2 && r.shield > 0);
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    r.x = RESCUE_FIRST_GROUP_X + 18;
    r.y = RESCUE_LAND_Y;
    for (int i = 0; i < 120; i++)
        rescueStep(&r, 0, 0, false, false, .01f);
    r.shield = 0;
    r.bullets[0] = (Bullet){r.x, r.y, 0, 0, 1, true, false};
    rescueStep(&r, 0, 0, false, false, .01f);
    assert(r.onboard == 0 && r.people[0].state == PERSON_LOST && r.lost == 3 && r.lives == 2);
    rescueStart(&r, 0, 1, 0);
    safeRescue(&r);
    r.tanks[0] = (Tank){900, 100, true};
    r.bullets[0] = (Bullet){900, 201, 0, 0, 1, false, false};
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
            assert(isfinite(r.x) && isfinite(r.y) && r.x >= 20 && r.x <= RESCUE_WORLD_WIDTH - 20 && r.y >= 35 &&
                   r.y <= RESCUE_LAND_Y);
            assert(r.onboard >= 0 && r.onboard <= RESCUE_CAPACITY && r.rescued <= PEOPLE && r.lives >= 0);
            for (int i = 0; i < 3; i++)
                assert(s.ammo[i] >= 0 && s.ammo[i] <= 10);
        }
    }
    puts("PASS: menu hitboxes, ammo, cooldown, explosions, chain hits, city "
         "loss, waves, rescue boarding/unloading/victory, crashes, lost "
         "passengers, tanks; 480000 simulation frames.");
    return 0;
}
