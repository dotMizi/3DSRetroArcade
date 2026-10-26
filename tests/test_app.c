#include <3ds.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
unsigned char splash_bin[288000];
#define main arcadeAppMain
#include "../source/main.c"
#undef main

/* App-flow tests substitute the hardware audio driver; its real logic has
 * separate NDSP tests. */
bool deltaAudioInit(void)
{
    return true;
}

bool deltaAudioAvailable(void)
{
    return true;
}

void deltaAudioUpdate(Game *g, bool active)
{
    (void)active;
    g->soundEvents = 0;
}

void shieldAudioUpdate(Shield *s, bool active)
{
    (void)active;
    s->soundEvents = 0;
}

void rescueAudioUpdate(Rescue *r, bool active, float dt)
{
    (void)r;
    (void)active;
    (void)dt;
}

void deltaAudioExit(void)
{
}

static unsigned char topBuffer[400 * 240 * 3 + 32], bottomBuffer[320 * 240 * 3 + 32];
static int frame;
static float exitTime;

void gfxInitDefault(void)
{
    memset(topBuffer, 0xa5, sizeof(topBuffer));
    memset(bottomBuffer, 0xa5, sizeof(bottomBuffer));
}

void gfxSet3D(bool enabled)
{
    assert(!enabled);
}

void gfxExit(void)
{
}

bool aptMainLoop(void)
{
    return ++frame <= 15;
}

void hidScanInput(void)
{
}

u32 hidKeysDown(void)
{
    switch (frame)
    {
    case 2:
        return KEY_TOUCH;
    case 3:
    case 4:
        return KEY_START;
    case 5:
        return KEY_B;
    case 6:
        return KEY_SELECT;
    case 7:
        return KEY_RIGHT;
    case 8:
        return KEY_A;
    case 9:
        return KEY_SELECT;
    case 10:
        return KEY_START;
    case 11:
        return KEY_A;
    default:
        return 0;
    }
}

u32 hidKeysHeld(void)
{
    return hidKeysDown();
}

void hidCircleRead(circlePosition *p)
{
    p->dx = p->dy = 0;
}

void hidTouchRead(touchPosition *p)
{
    p->px = 50;
    p->py = 110;
}

u64 osGetTime(void)
{
    return 42;
}

u64 svcGetSystemTick(void)
{
    return (u64)frame * 4468724;
}

unsigned char *gfxGetFramebuffer(int screen, int side, void *w, void *h)
{
    (void)side;
    (void)w;
    (void)h;
    return screen == GFX_TOP ? topBuffer + 16 : bottomBuffer + 16;
}

void gfxFlushBuffers(void)
{
}

void gspWaitForVBlank(void)
{
}

static void guards(void)
{
    for (int i = 0; i < 16; i++)
    {
        assert(topBuffer[i] == 0xa5 && topBuffer[sizeof(topBuffer) - 1 - i] == 0xa5);
        assert(bottomBuffer[i] == 0xa5 && bottomBuffer[sizeof(bottomBuffer) - 1 - i] == 0xa5);
    }
}

static void ppm(const char *name, unsigned char *data, int w)
{
    FILE *f = fopen(name, "wb");
    assert(f);
    fprintf(f, "P6\n%d 240\n255\n", w);
    for (int y = 0; y < 240; y++)
        for (int x = 0; x < w; x++)
        {
            int p = (x * 240 + 239 - y) * 3;
            fputc(data[p + 2], f);
            fputc(data[p + 1], f);
            fputc(data[p], f);
        }
    fclose(f);
}

static void capture(const char *name)
{
    char path[64];
    snprintf(path, sizeof(path), "%s-top.ppm", name);
    ppm(path, topBuffer + 16, 400);
    snprintf(path, sizeof(path), "%s-bottom.ppm", name);
    ppm(path, bottomBuffer + 16, 320);
}

void gfxSwapBuffers(void)
{
    guards();
    if (frame == 1)
        capture("menu");
    if (frame == 2)
        assert(app.screen == DELTA_INTRO && app.mode == 0);
    if (frame == 3)
        exitTime = app.introElapsed;
    if (frame == 4)
        assert(app.introElapsed == exitTime);
    if (frame == 3)
        assert(deltaIntroComplete(app.introElapsed) && !app.exitDialog);
    if (frame == 4)
        assert(app.exitDialog);
    if (frame == 5)
        assert(!app.exitDialog);
    if (frame == 6 || frame == 9)
        assert(app.screen == MENU);
    if (frame == 8)
        assert(app.mode == 1 && app.screen == PLAY);
}

static void event(App *a, u32 down, int x, int y)
{
    appUpdate(a, down, down, (circlePosition){0, 0}, (touchPosition){x, y}, .016f, 42);
}

static void render(App *a, const char *name)
{
    appRender(a, topBuffer + 16, bottomBuffer + 16);
    guards();
    capture(name);
}

static void testRescueScenery(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    r.x = 65;
    r.y = RESCUE_LAND_Y;
    r.shield = 0;
    unsigned char previous[30 * 26 * 3];
    for (int frame = 0; frame < 8; frame++)
    {
        r.time = frame * .12f;
        Rescue before = r;
        canvas(topBuffer + 16, 400);
        drawRescue(&r);
        assert(!memcmp(&r, &before, sizeof(r)));
        unsigned char flag[sizeof(previous)];
        int at = 0;
        for (int x = 140; x < 170; x++)
            for (int y = 138; y < 164; y++)
                for (int channel = 0; channel < 3; channel++)
                    flag[at++] = topBuffer[16 + (x * 240 + 239 - y) * 3 + channel];
        if (frame)
            assert(memcmp(flag, previous, sizeof(flag)));
        memcpy(previous, flag, sizeof(flag));
        int ground = 16 + (340 * 240 + 239 - 216) * 3;
        assert(topBuffer[ground] == 0x85 && topBuffer[ground + 1] == 0x85 && topBuffer[ground + 2] == 0x85);
        char name[64];
        snprintf(name, sizeof(name), "rescue-homebase-%02d.ppm", frame);
        ppm(name, topBuffer + 16, 400);
        guards();
    }
    unsigned char lastLandscape[400 * 240 * 3];
    for (int camera = 0; camera <= RESCUE_WORLD_WIDTH - 400; camera += 40)
    {
        r.camera = camera;
        r.x = camera + 180;
        r.y = 100;
        r.time = camera / 120.f;
        Rescue before = r;
        canvas(topBuffer + 16, 400);
        drawRescue(&r);
        assert(!memcmp(&r, &before, sizeof(r)));
        if (camera)
            assert(memcmp(lastLandscape, topBuffer + 16, sizeof(lastLandscape)));
        memcpy(lastLandscape, topBuffer + 16, sizeof(lastLandscape));
        drawRescue(&r);
        assert(!memcmp(lastLandscape, topBuffer + 16, sizeof(lastLandscape)));
        char name[64];
        snprintf(name, sizeof(name), "rescue-landscape-%02d.ppm", camera / 40);
        ppm(name, topBuffer + 16, 400);
        guards();
    }
}

static void testBombRendering(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    for (int i = 0; i < TANKS; i++)
        r.tanks[i].alive = false;
    r.x = 500;
    r.y = 90;
    r.vx = 60;
    r.shield = 10;
    r.x = 1000;
    r.tanks[0] = (Tank){1065, 100, true};
    r.tankVelocity[0] = 0;
    for (int frame = 0; frame < 80; frame++)
    {
        rescueStep(&r, 0, 0, false, frame == 0, .02f);
        if (frame % 2)
            continue;
        canvas(topBuffer + 16, 400);
        Rescue before = r;
        drawRescue(&r);
        assert(!memcmp(&r, &before, sizeof(r)));
        guards();
        char name[64];
        snprintf(name, sizeof(name), "rescue-bomb-%02d.ppm", frame / 2);
        ppm(name, topBuffer + 16, 400);
    }
    assert(!r.tanks[0].alive && r.score == 100);
}

static void testGroundAnimation(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    r.x = 470;
    r.y = RESCUE_LAND_Y;
    r.shield = 100;
    for (int i = 0; i < TANKS; i++)
        r.tanks[i].fire = 100;
    for (int frame = 0; frame < 48; frame++)
    {
        for (int step = 0; step < 6; step++)
            rescueStep(&r, 0, 0, false, false, .02f);
        canvas(topBuffer + 16, 400);
        drawRescue(&r);
        guards();
        char name[64];
        snprintf(name, sizeof(name), "rescue-ground-%02d.ppm", frame);
        ppm(name, topBuffer + 16, 400);
    }
}

static void testAirCombatAnimation(void)
{
    Rescue r;
    rescueStart(&r, 0, 4, 0);
    r.x = 1600;
    r.y = 90;
    r.shield = 1000;
    r.tankSpawnDelay = 1e9f;
    for (int frame = 0; frame < 1200; frame++)
        rescueStep(&r, 0, 0, false, false, .02f);
    for (int frame = 0; frame < 60; frame++)
    {
        for (int step = 0; step < 3; step++)
            rescueStep(&r, .25f, 0, false, false, .02f);
        canvas(topBuffer + 16, 400);
        drawRescue(&r);
        guards();
        char name[64];
        snprintf(name, sizeof(name), "rescue-air-%02d.ppm", frame);
        ppm(name, topBuffer + 16, 400);
    }
}

static void testRescueWaveFlow(void)
{
    App a;
    appInit(&a);
    a.mode = 2;
    a.screen = PLAY;
    rescueStart(&a.rescue, 0, 1, 0);
    event(&a, KEY_Y, 0, 0);
    assert(a.rescue.invincible);
    a.rescue.shield = 0;
    a.rescue.bullets[0] = (Bullet){a.rescue.x, a.rescue.y, 0, 0, 1, true, false};
    event(&a, 0, 0, 0);
    assert(a.rescue.lives == 3 && !a.rescue.crashTimer);
    event(&a, KEY_Y, 0, 0);
    assert(!a.rescue.invincible);
    a.rescue.lives = 1;
    a.rescue.rescued = PEOPLE - 4;
    a.rescue.lost = 4;
    a.rescue.tanksDestroyed = 3;
    a.rescue.score = a.rescue.rescued * 200 + 300;
    for (int i = 0; i < PEOPLE; i++)
        a.rescue.people[i].state = i < 4 ? PERSON_LOST : PERSON_INSIDE;
    event(&a, 0, 0, 0);
    assert(a.screen == PLAY && a.rescue.tally && !a.rescue.over);
    for (int frame = 0; frame < 65; frame++)
    {
        for (int step = 0; step < 5; step++)
            event(&a, 0, 0, 0);
        appRender(&a, topBuffer + 16, bottomBuffer + 16);
        guards();
        char name[64];
        snprintf(name, sizeof(name), "rescue-tally-%02d.ppm", frame);
        ppm(name, topBuffer + 16, 400);
    }
    assert(a.screen == PLAY);
    a.rescue.tallyRow = 5;
    a.rescue.tallyDelay = 1.29f;
    event(&a, 0, 0, 0);
    assert(a.screen == PLAY && a.rescue.stage == 2 && a.rescue.lives == 1);
    a.rescue.shield = 0;
    a.rescue.bullets[0] = (Bullet){a.rescue.x, a.rescue.y, 0, 0, 1, true, false};
    event(&a, 0, 0, 0);
    assert(a.screen == PLAY && a.rescue.crashTimer > 0);
    for (int frame = 0; frame < 65; frame++)
        event(&a, 0, 0, 0);
    assert(a.rescue.over && (a.screen == ENTER_NAME || a.screen == SCORE_LIST));
}

static void testBurningHouseAnimation(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    r.x = r.houses[0].x + 110;
    r.y = 95;
    r.shield = 100;
    r.tankSpawnDelay = 1e9f;
    r.tanks[0] = (Tank){r.houses[0].x - 100, 100, true};
    r.tankVelocity[0] = 0;
    for (int frame = 0; frame < 120; frame++)
    {
        if (frame == 8)
            r.bullets[0] = (Bullet){r.houses[0].x - 90, 205, 90, -25, 3, true, false};
        for (int step = 0; step < 5; step++)
            rescueStep(&r, 0, 0, false, false, .02f);
        canvas(topBuffer + 16, 400);
        Rescue before = r;
        drawRescue(&r);
        assert(!memcmp(&r, &before, sizeof(r)));
        guards();
        char name[64];
        snprintf(name, sizeof(name), "rescue-house-%02d.ppm", frame);
        ppm(name, topBuffer + 16, 400);
    }
    assert(r.houses[0].burning && r.houses[0].residents == 0);
}

static void testFenceAnimation(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    r.x = RESCUE_FENCE_X + 100;
    r.y = 100;
    r.shield = 100;
    for (int i = 0; i < TANKS; i++)
        r.tanks[i].fire = 100;
    r.tanks[0].alive = true;
    r.tanks[0].x = RESCUE_TANK_MIN_X + 22;
    r.tankVelocity[0] = -24;
    for (int frame = 0; frame < 40; frame++)
    {
        for (int step = 0; step < 5; step++)
            rescueStep(&r, 0, 0, false, false, .02f);
        canvas(topBuffer + 16, 400);
        drawRescue(&r);
        guards();
        char name[64];
        snprintf(name, sizeof(name), "rescue-fence-%02d.ppm", frame);
        ppm(name, topBuffer + 16, 400);
    }
}

static void testCrashAnimation(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    for (int i = 0; i < TANKS; i++)
        r.tanks[i].alive = false;
    r.x = 500;
    r.y = 95;
    r.camera = 300;
    r.shield = 0;
    r.onboard = 3;
    for (int i = 0; i < 3; i++)
        r.people[i].state = PERSON_ABOARD;
    for (int frame = 0; frame < 50; frame++)
    {
        if (frame == 5)
            r.bullets[0] = (Bullet){r.x, r.y, 0, 0, 1, true, false};
        for (int step = 0; step < 2; step++)
            rescueStep(&r, 0, 0, false, false, .02f);
        canvas(topBuffer + 16, 400);
        Rescue before = r;
        drawRescue(&r);
        assert(!memcmp(&r, &before, sizeof(r)));
        guards();
        char name[64];
        snprintf(name, sizeof(name), "rescue-crash-%02d.ppm", frame);
        ppm(name, topBuffer + 16, 400);
    }
    assert(r.lives == 2 && r.lost == 3 && r.onboard == 0 && !r.crashTimer);
}

static void testHomecomingAnimation(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    for (int i = 0; i < TANKS; i++)
        r.tanks[i].alive = false;
    r.x = 50;
    r.y = RESCUE_LAND_Y;
    r.onboard = RESCUE_CAPACITY;
    for (int i = 0; i < RESCUE_CAPACITY; i++)
        r.people[i].state = PERSON_ABOARD;
    for (int frame = 0; frame < 100; frame++)
    {
        for (int step = 0; step < 6; step++)
            rescueStep(&r, 0, 0, false, false, .02f);
        canvas(topBuffer + 16, 400);
        Rescue before = r;
        drawRescue(&r);
        assert(!memcmp(&r, &before, sizeof(r)));
        guards();
        char name[64];
        snprintf(name, sizeof(name), "rescue-homecoming-%03d.ppm", frame);
        ppm(name, topBuffer + 16, 400);
    }
    assert(r.onboard == 0 && r.rescued == RESCUE_CAPACITY);
}

static void testMountainParallax(void)
{
    Rescue r;
    rescueStart(&r, 0, 1, 0);
    r.x = -1000;
    bool distant[400];
    int mountainPixels = 0;
    for (int pass = 0; pass < 2; pass++)
    {
        r.camera = pass * 100;
        canvas(topBuffer + 16, 400);
        drawRescue(&r);
        for (int x = 0; x < 400; x++)
        {
            int at = 16 + (x * 240 + 239 - 85) * 3;
            unsigned color = (topBuffer[at + 2] << 16) | (topBuffer[at + 1] << 8) | topBuffer[at];
            bool mountain =
                color == 0x293b57 || color == 0x172338 || color == 0x506680 || color == 0x9bb6c6 || color == 0x617e9b;
            if (!pass)
            {
                distant[x] = mountain;
                mountainPixels += mountain;
            }
            else if (x < 384)
                assert(mountain == distant[x + 16]);
        }
        guards();
    }
    assert(mountainPixels > 0 && mountainPixels < 400);
}

static void testWallRendering(void)
{
    Game g;
    gameInit(&g, 42, 0);
    gameStart(&g);
    memset(g.enemies, 0, sizeof(g.enemies));
    for (int active = -1; active < ARENA_WALL_SEGMENTS; active++)
    {
        memset(g.wallFlash, 0, sizeof(g.wallFlash));
        if (active >= 0)
            g.wallFlash[active] = .18f;
        canvas(topBuffer + 16, 400);
        drawDelta(&g);
        for (int edge = 0; edge < ARENA_WALL_SEGMENTS; edge++)
        {
            const int *bar = arenaWallLines[edge];
            int x = (bar[0] + bar[2]) / 2, y = (bar[1] + bar[3]) / 2;
            int at = 16 + (x * 240 + 239 - y) * 3;
            bool lit = topBuffer[at] == 0xff && topBuffer[at + 1] == 0xea && topBuffer[at + 2] == 0x58;
            assert(lit == (active == edge));
        }
        guards();
        if (active == 0)
        {
            int center = 16 + (100 * 240 + 239 - 10) * 3;
            assert(topBuffer[center] > topBuffer[center + 3]);
            assert(topBuffer[center + 3] > topBuffer[center + 6]);
            assert(topBuffer[center + 6] > topBuffer[center + 9]);
            ppm("delta-wall-contact.ppm", topBuffer + 16, 400);
        }
    }
    memset(g.wallFlash, 0, sizeof(g.wallFlash));
    g.wallFlash[0] = .09f;
    canvas(topBuffer + 16, 400);
    drawDelta(&g);
    int center = 16 + (100 * 240 + 239 - 10) * 3;
    assert(topBuffer[center] > 100 && topBuffer[center] < 150);
    guards();
}

static void testAutomaticBaseInputs(void)
{
    App a;
    appInit(&a);
    a.screen = PLAY;
    a.mode = 1;
    shieldStart(&a.sky, 42, 0);
    a.sky.spawn = 100;
    a.sky.aimX = 370;
    a.sky.aimY = 80;
    const u32 keys[] = {KEY_A, KEY_Y, KEY_B, KEY_R};
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
        a.sky.cooldown = 0;
        event(&a, keys[i], 0, 0);
        assert(a.sky.ammo[2] == 9 - (int)i && a.sky.ammo[0] == 10 && a.sky.ammo[1] == 10);
    }
    a.sky.ammo[2] = 0;
    a.sky.baseDestroyed[2] = true;
    a.sky.cooldown = 0;
    event(&a, KEY_TOUCH, 300, 100);
    assert(a.sky.ammo[1] == 9 && a.sky.ammo[0] == 10);
}

static void testRescueWeaponInputs(void)
{
    App a;
    appInit(&a);
    a.selected = 2;
    event(&a, KEY_A, 0, 0);
    assert(a.screen == PLAY);
    for (int frame = 0; frame < 20; frame++)
        appUpdate(&a, 0, KEY_A, (circlePosition){0, 0}, (touchPosition){0, 0}, .016f, 42);
    for (int i = 0; i < BULLETS; i++)
        assert(a.rescue.bullets[i].life <= 0 || a.rescue.bullets[i].hostile);
    event(&a, 0, 0, 0);
    event(&a, KEY_A, 0, 0);
    assert(a.rescue.bullets[0].forward && !a.rescue.bullets[0].hostile);
    event(&a, KEY_B, 0, 0);
    assert(a.rescue.bullets[1].life > 0 && !a.rescue.bullets[1].forward && !a.rescue.bullets[1].hostile);
    a.rescue.gunCooldown = 0;
    event(&a, KEY_R, 0, 0);
    assert(a.rescue.bullets[2].forward);
}

static void testIntroSkip(void)
{
    const u32 keys[] = {KEY_A,
                        KEY_B,
                        KEY_X,
                        KEY_Y,
                        KEY_L,
                        KEY_R,
                        KEY_UP,
                        KEY_DOWN,
                        KEY_LEFT,
                        KEY_RIGHT,
                        KEY_START,
                        KEY_SELECT,
                        KEY_TOUCH};
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
        App a;
        appInit(&a);
        event(&a, KEY_A, 0, 0);
        assert(a.screen == DELTA_INTRO && a.introElapsed == 0);
        event(&a, keys[i], 150, 220);
        assert(a.screen == DELTA_INTRO && deltaIntroComplete(a.introElapsed));
        assert(!a.exitDialog && !a.quit);
        appUpdate(&a, 0, keys[i], (circlePosition){0, 0}, (touchPosition){150, 220}, .016f, 42);
        assert(a.screen == DELTA_INTRO);
        event(&a, 0, 0, 0);
        event(&a, KEY_A, 0, 0);
        assert(a.screen == PLAY);
    }
}

static void testStartRelease(void)
{
    for (int game = 0; game < 2; game++)
        for (int route = 0; route < 2; route++)
            for (int touch = 0; touch < 2; touch++)
            {
                /* Score lists use A; touch launch covers the menu and Delta intro. */
                if (route && touch)
                    continue;
                App a;
                appInit(&a);
                a.mode = game;
                a.selected = game;
                a.screen = route ? SCORE_LIST : MENU;
                u32 key = touch ? KEY_TOUCH : KEY_A;
                if (route)
                    event(&a, key, 0, 0);
                else if (game == 0)
                {
                    event(&a, KEY_A, 0, 0);
                    a.introElapsed = 100;
                    event(&a, key, 150, 220);
                }
                else
                    event(&a, key, 151, 110);
                assert(a.screen == PLAY);
                a.delta.shield = 10;
                a.sky.spawn = 100;
                int ammo = a.sky.ammo[0] + a.sky.ammo[1] + a.sky.ammo[2];
                touchPosition target = game ? (touchPosition){150, 110} : (touchPosition){200, 205};
                for (int i = 0; i < 30; i++)
                {
                    appUpdate(&a, 0, key, (circlePosition){0, 0}, target, .016f, 42);
                    if (game)
                        assert(a.sky.ammo[0] + a.sky.ammo[1] + a.sky.ammo[2] == ammo);
                    else
                        assert(!(a.delta.soundEvents & DELTA_SOUND_PLAYER) && a.delta.cooldown <= 0);
                }
                appUpdate(&a, 0, 0, (circlePosition){0, 0}, target, .016f, 42);
                assert(a.blockedPlayKeys == 0);
                appUpdate(&a, key, key, (circlePosition){0, 0}, target, .016f, 42);
                if (game)
                    assert(a.sky.ammo[0] + a.sky.ammo[1] + a.sky.ammo[2] == ammo - 1);
                else
                    assert((a.delta.soundEvents & DELTA_SOUND_PLAYER) && a.delta.cooldown > 0);
            }
}

int main(void)
{
    mkdir("sdmc:", 0777);
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/RetroArcade", 0777);
    mkdir("sdmc:/3ds/retro-arcade", 0777);
    remove("sdmc:/3ds/RetroArcade/scores-v2.txt.tmp");
    remove("sdmc:/3ds/RetroArcade/scores-v2.txt.bak");
    remove(savePath);
    FILE *legacy = fopen("sdmc:/3ds/RetroArcade/highscores.txt", "w");
    assert(legacy);
    fputs("100 200 300\n", legacy);
    fclose(legacy);
    const char *expected = "IN THE YEAR 2187, THE DELTA SYSTEM WAS CREATED TO PROTECT THE COLONIES "
                           "OF MANKIND. CONTROLLED BY AN ADVANCED ARTIFICIAL INTELLIGENCE, ITS "
                           "DEFENSE FORCES WERE THOUGHT INVINCIBLE. BUT DELTA HAS TURNED AGAINST "
                           "ITS MAKERS. NOW ONLY THE BRAVEST PILOTS DARE TO ENTER THE COMBAT ZONE. "
                           "AGAINST ENDLESS WAVES OF DEADLY MACHINES, ONE PILOT MUST SURVIVE. THE "
                           "FATE OF MANKIND RESTS UPON SKILL, COURAGE AND SPEED. THE FINAL BATTLE "
                           "HAS BEEN GIVEN A CODE NAME... DELTA-RACE";
    char normalized[1024];
    int at = 0;
    for (const char *p = deltaIntroText; *p; p++)
    {
        char c = *p == '\n' ? ' ' : *p;
        if (c == ' ' && at && normalized[at - 1] == ' ')
            continue;
        normalized[at++] = c;
    }
    normalized[at] = 0;
    assert(!strcmp(expected, normalized));
    assert(deltaIntroVisible(0) == 0 && deltaIntroVisible(1) == 35 && !deltaIntroComplete(1) &&
           deltaIntroComplete(100));
    assert(strstr(deltaIntroText, "DELTA-RACE") && deltaIntroStartHit(150, 220) && !deltaIntroStartHit(150, 190));
    assert(scoreKeyHit(10, 65) == 0 && scoreKeyHit(202, 161) == 36 && scoreKeyHit(38, 65) == -1 &&
           scoreKeyHit(319, 180) == -1);
    App a;
    appInit(&a);
    assert(scoresBest(&a.scores, 0) == 100 && scoresBest(&a.scores, 2) == 300);
    assert(!fopen(savePath, "r"));
    FILE *f = fopen("../data/splash.bin", "rb");
    assert(f);
    assert(fread(splash_bin, 1, sizeof(splash_bin), f) == sizeof(splash_bin));
    fclose(f);
    gfxInitDefault();
    testWallRendering();
    testRescueScenery();
    testMountainParallax();
    testGroundAnimation();
    testHomecomingAnimation();
    testCrashAnimation();
    testFenceAnimation();
    testBurningHouseAnimation();
    testRescueWaveFlow();
    testAirCombatAnimation();
    testBombRendering();
    for (int game = 0; game < 3; game++)
    {
        a.screen = MENU;
        a.mode = -1;
        a.selected = game;
        event(&a, KEY_A, 0, 0);
        if (game == 0)
        {
            assert(a.screen == DELTA_INTRO && a.introElapsed == 0);
            render(&a, "delta-intro-empty");
            for (int px = 0; px < 400 * 240 * 3; px++)
                assert(topBuffer[16 + px] == 0);
            for (int px = 0; px < 320 * 240 * 3; px++)
                assert(bottomBuffer[16 + px] == 0);
            for (int i = 0; i < 150; i++)
                event(&a, 0, 0, 0);
            render(&a, "delta-intro-partial");
            for (int px = 0; px < 320 * 240 * 3; px++)
                assert(bottomBuffer[16 + px] == 0);
            for (int i = 0; i < 1000; i++)
                event(&a, 0, 0, 0);
            assert(deltaIntroComplete(a.introElapsed) && a.screen == DELTA_INTRO);
            render(&a, "delta-intro-ready");
            for (int px = 0; px < 400 * 240 * 3; px++)
                assert(topBuffer[16 + px] == 0 || topBuffer[16 + px] == 255);
            event(&a, KEY_TOUCH, 150, 220);
        }
        assert(a.mode == game && a.screen == PLAY);
        render(&a, game == 0 ? "delta" : game == 1 ? "sky" : "rescue");
        if (game == 0)
        {
            a.delta.score = 1500;
            a.delta.playing = false;
        }
        if (game == 1)
        {
            a.sky.score = 2500;
            a.sky.over = true;
        }
        if (game == 2)
        {
            a.rescue.score = 3500;
            a.rescue.over = true;
        }
        event(&a, 0, 0, 0);
        if (game == 0)
            assert(!fopen(savePath, "r"));
        assert(a.screen == ENTER_NAME && a.pendingRank == 0 && a.cursor == 0);
        char prefix[32];
        snprintf(prefix, sizeof(prefix), "entry-%d", game);
        render(&a, prefix);
        event(&a, KEY_TOUCH, 10 + 9 * 30 + 12, 65 + 2 * 28 + 12); // 3
        event(&a, KEY_TOUCH, 10 + 6 * 30 + 12, 65 + 3 * 28 + 12); // space
        event(&a, KEY_TOUCH, 10 + 5 * 30 + 12, 65 + 2 * 28 + 12); // Z
        assert(!strcmp(a.initials, "3 Z") && a.cursor == 0);
        event(&a, KEY_B, 0, 0);
        assert(!strcmp(a.initials, "3  ") && a.cursor == 2);
        event(&a, KEY_TOUCH, 10 + 5 * 30 + 12, 65 + 2 * 28 + 12);
        event(&a, KEY_Y, 0, 0);
        assert(a.screen == SCORE_LIST && !a.dirty && !a.saveFailed);
        assert(!strcmp(a.scores.entries[game][0].name, "3 Z"));
        int count = a.scores.count[game];
        for (int i = 0; i < 30; i++)
            event(&a, 0, 0, 0);
        assert(a.screen == SCORE_LIST && a.scores.count[game] == count);
        snprintf(prefix, sizeof(prefix), "scores-%d", game);
        render(&a, prefix);
        event(&a, KEY_SELECT, 0, 0);
        assert(a.screen == MENU);
        event(&a, KEY_TOUCH, 50 + game * 101, 110);
        if (game == 0)
        {
            assert(a.screen == DELTA_INTRO);
            a.introElapsed = 100;
        }
        else
            assert(a.screen == SCORE_LIST);
        event(&a, KEY_A, 0, 0);
        assert(a.screen == PLAY);
        assert((game == 0 ? a.delta.score : game == 1 ? a.sky.score : a.rescue.score) == 0);
    }
    // Incoming trails stay on the main screen; the touch pad never reveals
    // threats.
    a.mode = 1;
    a.screen = PLAY;
    shieldStart(&a.sky, 42, 0);
    a.sky.remaining = 0;
    a.sky.missiles[0] = (Missile){100, 110, 100, 215, 25, true};
    a.sky.trailStartX[0] = 100;
    render(&a, "sky-trails");
    int mainPoint = (100 * 240 + 239 - 50) * 3, radarPoint = (85 * 240 + 239 - 75) * 3;
    assert(topBuffer[16 + mainPoint] == 0x7b && topBuffer[16 + mainPoint + 1] == 0x54 &&
           topBuffer[16 + mainPoint + 2] == 0xff);
    assert(bottomBuffer[16 + radarPoint] != 0x7b);
    unsigned char hiddenThreats[320 * 240 * 3];
    memcpy(hiddenThreats, bottomBuffer + 16, sizeof(hiddenThreats));
    a.sky.missiles[0].x = 250;
    a.sky.missiles[0].y = 90;
    a.sky.blasts[1] = (Blast){140, 130, .3f, true};
    appRender(&a, topBuffer + 16, bottomBuffer + 16);
    guards();
    assert(!memcmp(hiddenThreats, bottomBuffer + 16, sizeof(hiddenThreats)));
    a.sky.missiles[0].x = 100;
    a.sky.missiles[0].y = 110;
    memset(a.sky.blasts, 0, sizeof(a.sky.blasts));
    a.sky.blasts[0] = (Blast){100, 110, .3f, true};
    shieldStep(&a.sky, .01f);
    assert(!a.sky.missiles[0].alive);
    render(&a, "sky-trail-exploded");
    assert(topBuffer[16 + mainPoint] != 0x7b && bottomBuffer[16 + radarPoint] != 0x7b);
    shieldStart(&a.sky, 42, 0);
    a.sky.remaining = 1;
    a.sky.spawn = 100;
    unsigned char cityStages[6] = {CITY_DOMED, CITY_EXPOSED, CITY_RUIN, CITY_GONE, CITY_DOMED, CITY_EXPOSED};
    memcpy(a.sky.city, cityStages, sizeof(cityStages));
    render(&a, "sky-city-states");
    a.sky.remaining = 0;
    a.sky.ammo[0] = 10;
    a.sky.ammo[1] = 8;
    a.sky.ammo[2] = 9;
    shieldStep(&a.sky, .01f);
    assert(a.sky.tally && a.sky.score == 0);
    render(&a, "sky-tally-zero");
    for (int frame = 0; frame < 240; frame++)
    {
        shieldStep(&a.sky, .01f);
    }
    assert(a.sky.score > 0 && a.sky.score < 5770);
    render(&a, "sky-tally-counting");
    while (a.sky.tallyRow < 4)
        shieldStep(&a.sky, .01f);
    assert(a.sky.score == 5770 && a.sky.bonusPaid == 5770);
    render(&a, "sky-wave-tally");
    App reloaded;
    appInit(&reloaded);
    assert(reloaded.scores.count[0] == 2 && reloaded.scores.entries[2][0].score == 3500);
    // A full list rejects a tied last-place score; rejected runs never modify it.
    for (int i = 0; i < 12; i++)
        scoresInsert(&a.scores, 0, 10000 + i * 100, "TOP");
    a.mode = 0;
    Scores before = a.scores;
    appFinish(&a, 1);
    assert(a.screen == SCORE_LIST && !memcmp(&before, &a.scores, sizeof(before)));
    render(&a, "scores-full");
    // SELECT commits the visible three characters, and save failures can be
    // retried.
    a.mode = 1;
    appFinish(&a, 9000);
    memcpy(a.initials, "A 9", 4);
    const char *normal = savePath;
    savePath = "missing-dir/scores.txt";
    event(&a, KEY_SELECT, 0, 0);
    assert(a.screen == MENU && a.dirty && a.saveFailed);
    int n = a.scores.count[1];
    savePath = normal;
    event(&a, KEY_Y, 0, 0);
    assert(!a.dirty && a.scores.count[1] == n);
    // Exit dialog freezes play, can be cancelled, and saves pending initials.
    a.mode = 2;
    appFinish(&a, 8000);
    memcpy(a.initials, "END", 4);
    event(&a, KEY_START, 0, 0);
    assert(a.exitDialog && !a.quit);
    event(&a, KEY_B, 0, 0);
    assert(!a.exitDialog && a.screen == ENTER_NAME);
    event(&a, KEY_START, 0, 0);
    savePath = "missing-dir/scores.txt";
    event(&a, KEY_A, 0, 0);
    assert(!a.quit && a.exitDialog && a.saveFailed && a.screen == SCORE_LIST);
    int savedCount = a.scores.count[2];
    savePath = normal;
    event(&a, KEY_A, 0, 0);
    assert(a.quit && !a.dirty && a.scores.count[2] == savedCount);
    appInit(&reloaded);
    assert(!strcmp(reloaded.scores.entries[2][0].name, "END"));
    testAutomaticBaseInputs();
    testRescueWeaponInputs();
    testIntroSkip();
    testStartRelease();
    frame = 0;
    assert(arcadeAppMain() == 0 && frame == 11);
    puts("PASS: all three end-of-game flows, shared initials keyboard, list "
         "persistence, restart, SELECT commit, save/reload/retry, rejected "
         "ranks, exit/cancel, real main input routing, screen guards.");
}
