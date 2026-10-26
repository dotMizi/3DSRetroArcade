#include "arcade.h"
#include <math.h>
#include <string.h>

static float clamp(float x, float a, float b)
{
    return fmaxf(a, fminf(b, x));
}

static float rand01(Shield *s)
{
    s->rng ^= s->rng << 13;
    s->rng ^= s->rng >> 17;
    s->rng ^= s->rng << 5;
    return (s->rng & 0xffffff) / 16777216.f;
}

int arcadeHit(int x, int y)
{
    if (y < 58 || y >= 186)
        return -1;
    for (int i = 0; i < 3; i++)
        if (x >= 13 + i * 101 && x < 105 + i * 101)
            return i;
    return -1;
}

ShieldWaveConfig shieldWaveConfig(int wave)
{
    /* Rev. 3 ROM tables $6090/$60a3/$60b2/$60c1, decoded in:
     https://www.6502disassembly.com/va-missile-command/wave-guide.html */
    static const unsigned char missiles[19] = {
        12, 15, 18, 12, 16, 14, 17, 10, 13, 16, 19, 12, 14, 16, 18, 14, 17, 19, 22};
    static const unsigned char smart[19] = {0, 0, 0, 0, 0, 1, 1, 2, 3, 4, 4, 5, 5, 6, 6, 7, 7, 7, 7};
    static const unsigned short delay[15] = {1232, 736, 448, 264, 160, 96, 64, 32, 16, 10, 6, 4, 2, 1, 0};
    int index = wave < 1 ? 0 : wave > 19 ? 18 : wave - 1;
    int speedIndex = index < 15 ? index : 14;
    /* Original 8.8 frame-delay curve, mapped to continuous 60 Hz movement.
     Launch spacing is adapted for this touchscreen arena, capped at wave 19. */
    return (ShieldWaveConfig){
        missiles[index], smart[index], 60.f / (1.f + delay[speedIndex] / 256.f), fmaxf(.38f, 1.1f - index * .04f)};
}

static void shieldWave(Shield *s)
{
    s->wave++;
    s->soundEvents |= SHIELD_SOUND_WAVE;
    ShieldWaveConfig config = shieldWaveConfig(s->wave);
    s->remaining = config.missiles;
    s->smartRemaining = config.smartBombs;
    s->spawn = .8f;
    s->delay = 0;
    s->tally = false;
    for (int i = 0; i < 3; i++)
    {
        s->ammo[i] = 10;
        s->baseDestroyed[i] = false;
    }
}

void shieldStart(Shield *s, uint32_t seed, int best)
{
    memset(s, 0, sizeof(*s));
    s->rng = seed ? seed : 1;
    s->best = best;
    s->aimX = 200;
    s->aimY = 100;
    for (int i = 0; i < 6; i++)
        s->city[i] = CITY_DOMED;
    shieldWave(s);
}

float blastRadius(float age)
{
    return age < .35f ? age * 80 : age < .95f ? 28 : fmaxf(0, 28 - (age - .95f) * 56);
}

static void blast(Shield *s, float x, float y)
{
    for (int i = 0; i < BLASTS; i++)
        if (!s->blasts[i].alive)
        {
            s->blasts[i] = (Blast){x, y, 0, true};
            s->soundEvents |= SHIELD_SOUND_BLAST;
            return;
        }
}

bool shieldFire(Shield *s, float x, float y)
{
    if (s->over || s->tally || s->cooldown > 0)
        return false;
    x = clamp(x, 5, 395);
    y = clamp(y, 20, 199);
    int battery = -1;
    float nearest = 1e9f;
    for (int i = 0; i < 3; i++)
        if (!s->baseDestroyed[i] && s->ammo[i] > 0 && fabsf(x - (30 + i * 170)) < nearest)
        {
            nearest = fabsf(x - (30 + i * 170));
            battery = i;
        }
    if (battery < 0)
        return false;
    for (int i = 0; i < INTERCEPTORS; i++)
        if (!s->interceptors[i].alive)
        {
            s->interceptors[i] = (Missile){30 + battery * 170, 216, x, y, 260, true};
            s->ammo[battery]--;
            s->soundEvents |= SHIELD_SOUND_LAUNCH;
            s->cooldown = .16f;
            return true;
        }
    return false;
}

static bool travel(Missile *m, float dt)
{
    float dx = m->tx - m->x, dy = m->ty - m->y, d = hypotf(dx, dy);
    if (d <= m->speed * dt)
    {
        m->x = m->tx;
        m->y = m->ty;
        return true;
    }
    m->x += dx / d * m->speed * dt;
    m->y += dy / d * m->speed * dt;
    return false;
}

int shieldBonusTarget(const Shield *s, int row)
{
    return row == 0 ? s->bonusAmmo : row > 0 && row < 4 ? s->bonusCities[3 - row] : 0;
}

int shieldBonusRate(int row)
{
    static const int rates[4] = {10, 1500, 1000, 500};
    return row >= 0 && row < 4 ? rates[row] : 0;
}

static void shieldCountBonus(Shield *s, float dt)
{
    s->delay += dt;
    if (s->tallyRow >= 4)
    {
        if (s->delay >= 1.2f)
            shieldWave(s);
        return;
    }
    int row = s->tallyRow;
    float interval = row == 0 ? .045f : .28f;
    if (s->delay < interval)
        return;
    if (s->bonusShown[row] < shieldBonusTarget(s, row))
    {
        s->delay -= interval;
        s->bonusShown[row]++;
        s->soundEvents |= SHIELD_SOUND_BONUS;
        int points = shieldBonusRate(row);
        s->bonusPaid += points;
        s->score += points;
        if (s->score > s->best)
            s->best = s->score;
    }
    else
    {
        s->tallyRow++;
        s->delay = s->tallyRow == 4 ? 0 : -.22f;
    }
}

static void smartBombDodge(Shield *s, Missile *m, float dt)
{
    float nearest = 1e9f, dodgeX = 0, dodgeY = 0;
    for (int i = 0; i < BLASTS; i++)
    {
        const Blast *b = &s->blasts[i];
        if (!b->alive)
            continue;
        float dx = m->x - b->x, dy = m->y - b->y, distance = hypotf(dx, dy);
        float radius = blastRadius(b->age);
        if (distance > radius + 2 && distance < radius + 22 && distance < nearest)
        {
            nearest = distance;
            dodgeX = fabsf(dx) < 1 ? (m->tx < m->x ? -1.f : 1.f) : dx / distance;
            dodgeY = dy / distance;
        }
    }
    m->x = clamp(m->x + dodgeX * 62 * dt, 5, 395);
    /* From wave 9 the original prevents upward evasion. */
    if (s->wave >= 9)
        dodgeY = fmaxf(0, dodgeY);
    /* Allow the final approach all the way to the target. Clamping to 214
     reset a bomb on every frame before it could reach ground level 215. */
    m->y = clamp(m->y + dodgeY * 42 * dt, SHIELD_ENTRY_Y, m->ty);
}

void shieldStep(Shield *s, float dt)
{
    if (s->over)
        return;
    dt = clamp(dt, 0, .034f);
    if (s->tally)
    {
        shieldCountBonus(s, dt);
        return;
    }
    s->cooldown = fmaxf(0, s->cooldown - dt);
    s->spawn -= dt;
    if ((s->remaining > 0 || s->smartRemaining > 0) && s->spawn <= 0)
    {
        for (int i = 0; i < MISSILES; i++)
            if (!s->missiles[i].alive)
            {
                int target = (int)(rand01(s) * 9);
                float targetX;
                if (target < 6)
                    targetX = 55 + target * 58;
                else
                {
                    int base = target - 6;
                    targetX = !s->baseDestroyed[base] ? 30 + base * 170 : 55 + (int)(rand01(s) * 6) * 58;
                }
                ShieldWaveConfig config = shieldWaveConfig(s->wave);
                bool smart =
                    s->smartRemaining > 0 &&
                    (s->remaining == 0 || rand01(s) < s->smartRemaining / (float)(s->remaining + s->smartRemaining));
                s->smart[i] = smart;
                s->missiles[i] = (Missile){
                    10 + rand01(s) * 380, SHIELD_ENTRY_Y, targetX, 215, smart ? 28.f : config.missileSpeed, true};
                s->trailStartX[i] = s->missiles[i].x;
                if (smart)
                    s->smartRemaining--;
                else
                    s->remaining--;
                s->spawn = config.launchInterval;
                break;
            }
    }
    for (int i = 0; i < INTERCEPTORS; i++)
    {
        Missile *m = &s->interceptors[i];
        if (m->alive && travel(m, dt))
        {
            m->alive = false;
            blast(s, m->x, m->y);
        }
    }
    for (int i = 0; i < BLASTS; i++)
        if (s->blasts[i].alive)
        {
            s->blasts[i].age += dt;
            if (s->blasts[i].age > 1.45f)
                s->blasts[i].alive = false;
        }
    int active = 0;
    for (int i = 0; i < MISSILES; i++)
    {
        Missile *m = &s->missiles[i];
        if (!m->alive)
            continue;
        bool hit = false;
        for (int j = 0; j < BLASTS; j++)
            if (s->blasts[j].alive)
            {
                float radius = blastRadius(s->blasts[j].age);
                if (hypotf(m->x - s->blasts[j].x, m->y - s->blasts[j].y) < radius + 2)
                {
                    hit = true;
                    break;
                }
            }
        if (hit)
        {
            m->alive = false;
            blast(s, m->x, m->y);
            s->score += 10;
            if (s->score > s->best)
                s->best = s->score;
        }
        else
        {
            if (s->smart[i])
                smartBombDodge(s, m, dt);
            if (!travel(m, dt))
            {
                active++;
                continue;
            }
            m->alive = false;
            int base = (int)lroundf((m->tx - 30) / 170);
            if (base >= 0 && base < 3 && fabsf(m->tx - (30 + base * 170)) <= 14)
            {
                if (!s->baseDestroyed[base])
                    s->soundEvents |= SHIELD_SOUND_DAMAGE;
                s->baseDestroyed[base] = true;
                s->ammo[base] = 0;
            }
            else
            {
                int city = (int)lroundf((m->tx - 55) / 58);
                if (city >= 0 && city < 6 && s->city[city] > CITY_GONE)
                {
                    s->city[city]--;
                    s->soundEvents |= SHIELD_SOUND_DAMAGE;
                }
            }
        }
        if (m->alive)
            active++;
    }
    int cities = 0;
    for (int i = 0; i < 6; i++)
        cities += s->city[i] > CITY_GONE;
    if (!cities)
    {
        s->over = true;
        return;
    }
    if (s->remaining == 0 && s->smartRemaining == 0 && !active)
    {
        s->tally = true;
        s->delay = -.4f;
        s->tallyRow = 0;
        s->bonusPaid = 0;
        memset(s->bonusShown, 0, sizeof(s->bonusShown));
        s->bonusAmmo = s->ammo[0] + s->ammo[1] + s->ammo[2];
        memset(s->bonusCities, 0, sizeof(s->bonusCities));
        for (int i = 0; i < 6; i++)
            if (s->city[i] > CITY_GONE)
                s->bonusCities[s->city[i] - 1]++;
        s->bonusTotal =
            s->bonusAmmo * 10 + s->bonusCities[2] * 1500 + s->bonusCities[1] * 1000 + s->bonusCities[0] * 500;

        memset(s->interceptors, 0, sizeof(s->interceptors));
        memset(s->blasts, 0, sizeof(s->blasts));
    }
}

static float passengerRandom(HomeWalk *walk)
{
    walk->rng ^= walk->rng << 13;
    walk->rng ^= walk->rng >> 17;
    walk->rng ^= walk->rng << 5;
    return (walk->rng & 0xffffff) / 16777216.f;
}

static void passengersGoHome(Rescue *r, float dt)
{
    for (int i = 0; i < PEOPLE; i++)
    {
        Person *person = &r->people[i];
        if (person->state != PERSON_GOING_HOME)
            continue;
        HomeWalk *walk = &r->homeWalk[i];
        walk->timer -= dt;
        if (walk->action != HOME_WALK)
        {
            if (walk->action == HOME_WAVE)
                walk->facing = r->x < person->x ? -1 : 1;
            if (walk->timer > 0)
                continue;
            walk->action = HOME_WALK;
            walk->timer = .6f + passengerRandom(walk) * 1.6f;
        }
        else if (walk->timer <= 0 && walk->stops < 2)
        {
            walk->action = passengerRandom(walk) < .6f ? HOME_WAVE : HOME_PAUSE;
            walk->timer = .45f + passengerRandom(walk) * 1.2f;
            walk->stops++;
            if (walk->action == HOME_WAVE)
                walk->facing = r->x < person->x ? -1 : 1;
            continue;
        }
        float distance = RESCUE_HOME_DOOR_X - person->x;
        walk->facing = distance < 0 ? -1 : 1;
        float step = clamp(distance, -walk->speed * dt, walk->speed * dt);
        person->x += step;
        walk->phase += fabsf(step) * .42f;
        if (fabsf(RESCUE_HOME_DOOR_X - person->x) < 1)
            person->state = PERSON_INSIDE;
    }
}

RescueThreatConfig rescueThreatConfig(int stage)
{
    int level = stage < 1 ? 1 : stage > 30 ? 30 : stage;
    return (RescueThreatConfig){level < 2    ? 0
                                : level == 2 ? 1
                                             : (int)fminf(4, 2 + (level - 3) / 3),
                                level < 3 ? 0 : (int)fminf(6, 2 + (level - 3) / 2),
                                (int)fminf(4, 2 + (level - 1) / 4),
                                level >= 4};
}

void rescueStart(Rescue *r, int best, int stage, int score)
{
    memset(r, 0, sizeof(*r));
    r->x = 80;
    r->y = 130;
    r->facing = 1;
    r->yaw = 1;
    r->lives = 3;
    r->shield = 3;
    r->best = best;
    r->stage = stage;
    r->score = score;
    r->stageStartScore = score;
    r->soundEvents = RESCUE_SOUND_WAVE;
    for (int i = 0; i < RESCUE_HOUSES; i++)
        r->houses[i].x = RESCUE_FIRST_GROUP_X + i * RESCUE_GROUP_SPACING + 100;
    for (int i = 0; i < PEOPLE; i++)
    {
        r->people[i] =
            (Person){RESCUE_FIRST_GROUP_X + (i / RESCUE_PEOPLE_PER_HOUSE) * RESCUE_GROUP_SPACING + (i % 8) * 9, 0};
        int group = i / RESCUE_PEOPLE_PER_HOUSE, local = i % RESCUE_PEOPLE_PER_HOUSE;
        if (local >= RESCUE_PEOPLE_PER_HOUSE / 2)
        {
            r->people[i].state = PERSON_SHELTERED;
            r->people[i].x = r->houses[group].x;
            r->houses[group].residents++;
        }
        r->peopleHome[i] = r->people[i].x;
        HomeWalk *walk = &r->homeWalk[i];
        walk->rng = 0x9e3779b9u ^ ((unsigned)(i + 1) * 0x85ebca6bu) ^ (unsigned)best ^ ((unsigned)stage << 16);
        if (!walk->rng)
            walk->rng = 1;
        walk->speed = 12 + passengerRandom(walk) * 15;
        walk->facing = 1;
    }
    r->patrolRng = 0x72616365u ^ (unsigned)best ^ ((unsigned)stage * 0x9e3779b9u);
    if (!r->patrolRng)
        r->patrolRng = 1;
    r->tankSpawnDelay = 9;
    r->aircraftSpawnDelay = 10;
}

static float patrolRandom(Rescue *r)
{
    r->patrolRng ^= r->patrolRng << 13;
    r->patrolRng ^= r->patrolRng >> 17;
    r->patrolRng ^= r->patrolRng << 5;
    return (r->patrolRng & 0xffffff) / 16777216.f;
}

static void rescuePatrols(Rescue *r, float dt)
{
    int active = 0;
    for (int i = 0; i < TANKS; i++)
    {
        if (r->tanks[i].alive && r->tankLife[i] > 0)
        {
            r->tankLife[i] -= dt;
            if (r->tankLife[i] <= 0)
                r->tanks[i].alive = false;
        }
        active += r->tanks[i].alive;
    }
    if (r->x < RESCUE_FENCE_X)
        return;
    r->tankSpawnDelay -= dt;
    if (r->tankSpawnDelay > 0 || active >= rescueThreatConfig(r->stage).tanks)
        return;
    for (int i = 0; i < TANKS; i++)
        if (!r->tanks[i].alive)
        {
            bool fromRight = r->camera - 35 < RESCUE_TANK_MIN_X ||
                             (r->camera + 435 < RESCUE_WORLD_WIDTH - 30 && patrolRandom(r) < .6f);
            float x = fromRight ? r->camera + 435 : r->camera - 35;
            x = clamp(x, RESCUE_TANK_MIN_X, RESCUE_WORLD_WIDTH - 30);
            r->tanks[i] = (Tank){x, 2.5f, true};
            r->tankVelocity[i] = (fromRight ? -1 : 1) * (14 + patrolRandom(r) * 10);
            r->tankLife[i] = 32 + patrolRandom(r) * 18;
            r->tankSpawnDelay = fmaxf(6, 13 - (r->stage - 1) * .5f) + patrolRandom(r) * 11;
            break;
        }
}

static void rescueHouseEvacuation(Rescue *r, float dt)
{
    for (int house = 0; house < RESCUE_HOUSES; house++)
    {
        RescueHouse *home = &r->houses[house];
        if ((!home->burning && !home->evacuating) || home->residents <= 0)
            continue;
        home->escapeDelay -= dt;
        if (!home->burning && home->escapeDelay > 0)
            continue;
        for (int i = house * RESCUE_PEOPLE_PER_HOUSE; i < (house + 1) * RESCUE_PEOPLE_PER_HOUSE; i++)
            if (r->people[i].state == PERSON_SHELTERED)
            {
                float side = i % 2 ? -1 : 1;
                r->people[i].state = PERSON_FLEEING;
                r->people[i].x = home->x + side * 5;
                r->peopleHome[i] = home->x + side * (40 + i % 3 * 13);
                home->residents--;
                home->escapeDelay = 1.8f + patrolRandom(r) * 1.7f;
                if (!home->burning)
                    break;
            }
    }
}

static void rescueHit(Rescue *r)
{
    if (r->invincible || r->shield > 0 || r->crashTimer > 0)
        return;
    r->lives--;
    for (int i = 0; i < PEOPLE; i++)
        if (r->people[i].state == PERSON_ABOARD)
        {
            r->people[i].state = PERSON_LOST;
            r->lost++;
        }
    r->onboard = 0;
    r->vx = r->vy = 0;
    r->shield = 3;
    r->crashTimer = .9f;
    /* Keep the camera and impact position until the explosion has finished. */
}

static void bullet(Rescue *r, float x, float y, float vx, float vy, bool hostile, bool forward)
{
    for (int i = 0; i < BULLETS; i++)
        if (r->bullets[i].life <= 0)
        {
            r->bullets[i] = (Bullet){x, y, vx, vy, 3, hostile, forward};
            break;
        }
}

static void bombImpact(Rescue *r, float x, float y)
{
    for (int i = 0; i < 8; i++)
        if (r->bombImpacts[i].life <= 0)
        {
            r->bombImpacts[i] = (BombImpact){x, y, .24f};
            break;
        }
}

static void rescueAirCombat(Rescue *r, float dt)
{
    RescueThreatConfig config = rescueThreatConfig(r->stage);
    int jets = 0, drones = 0;
    for (int i = 0; i < RESCUE_AIRCRAFT; i++)
        if (r->aircraft[i].alive)
        {
            jets += r->aircraft[i].kind == RESCUE_JET;
            drones += r->aircraft[i].kind == RESCUE_DRONE;
        }
    if (r->x >= RESCUE_FENCE_X)
    {
        r->aircraftSpawnDelay -= dt;
        if (r->aircraftSpawnDelay <= 0 && (jets < config.jets || drones < config.drones))
            for (int i = 0; i < RESCUE_AIRCRAFT; i++)
                if (!r->aircraft[i].alive)
                {
                    int kind = jets < config.jets ? RESCUE_JET : RESCUE_DRONE;
                    float x = r->facing > 0 ? r->camera - 30 : r->camera + 430;
                    x = clamp(x, RESCUE_FENCE_X + 25, RESCUE_WORLD_WIDTH - 25);
                    r->aircraft[i] = (RescueAircraft){
                        x, 65 + patrolRandom(r) * 60, 3.5f, 5, kind, kind == RESCUE_JET ? 3 : 2, r->facing, true};
                    r->aircraftSpawnDelay = kind == RESCUE_JET ? 10 : 9;
                    break;
                }
    }
    for (int i = 0; i < RESCUE_AIRCRAFT; i++)
    {
        RescueAircraft *air = &r->aircraft[i];
        if (!air->alive)
            continue;
        bool jet = air->kind == RESCUE_JET;
        float targetY = clamp(r->y + sinf(r->time * .8f + i) * 12 - (jet ? 0 : 32), 40, 148);
        if (jet)
        {
            /* Fly through the scene at constant speed; turn only beyond the screen. */
            air->x += air->facing * (190 + fminf(r->stage, 15) * 3) * dt;
            if ((air->facing > 0 && air->x > r->camera + 1400) || (air->facing < 0 && air->x < r->camera - 1000))
            {
                air->facing = -air->facing;
                air->x = air->facing > 0 ? r->camera - 1000 : r->camera + 1400;
            }
        }
        else
        {
            float targetX = clamp(r->x - r->facing * (90 + i * 15), RESCUE_FENCE_X + 25, RESCUE_WORLD_WIDTH - 25);
            air->x += clamp(targetX - air->x, -55 * dt, 55 * dt);
            air->facing = r->x < air->x ? -1 : 1;
        }
        float climb = jet ? 8 : 16;
        float heightChange = (targetY - air->y) * (jet ? .35f : 1.f) * dt;
        air->y += clamp(heightChange, -climb * dt, climb * dt);
        air->fire -= dt;
        if (air->fire <= 0 && r->x >= RESCUE_FENCE_X && fabsf(r->x - air->x) < 360)
        {
            float angle = atan2f(r->y - air->y, r->x - air->x);
            angle += (patrolRandom(r) * 2 - 1) * fmaxf(.045f, .15f - r->stage * .008f);
            float speed = jet ? 115 : 75;
            bullet(r, air->x + air->facing * 12, air->y, cosf(angle) * speed, sinf(angle) * speed, true, false);
            air->fire = fmaxf(jet ? 2.8f : 3.5f, (jet ? 6.5f : 10.f) - r->stage * .25f) + patrolRandom(r);
        }
        air->mineDelay -= dt;
        if (!jet && config.mines && air->mineDelay <= 0 && r->x >= RESCUE_FENCE_X)
            for (int mine = 0; mine < RESCUE_MINES; mine++)
                if (!r->mines[mine].alive)
                {
                    r->mines[mine] = (RescueMine){air->x, air->y + 8, 16, true};
                    air->mineDelay = fmaxf(4, 9 - r->stage * .2f) + patrolRandom(r) * 3;
                    break;
                }
    }
    for (int i = 0; i < RESCUE_MINES; i++)
    {
        RescueMine *mine = &r->mines[i];
        if (!mine->alive)
            continue;
        mine->life -= dt;
        mine->y = fminf(RESCUE_LAND_Y - 8, mine->y + 25 * dt);
        if (mine->life <= 0)
            mine->alive = false;
        else if (hypotf(mine->x - r->x, mine->y - r->y) < 15)
        {
            mine->alive = false;
            bombImpact(r, mine->x, mine->y);
            rescueHit(r);
        }
    }
}

static void rescueCountWave(Rescue *r, float dt)
{
    r->time += dt;
    r->tallyDelay += dt;
    if (r->tallyRow >= 5)
    {
        if (r->tallyDelay >= 1.3f)
        {
            int score = r->score, best = r->best, lives = r->lives, next = r->stage + 1;
            bool invincible = r->invincible;
            rescueStart(r, best, next, score);
            r->invincible = invincible;
            r->lives = lives;
        }
        return;
    }
    int target[] = {r->rescued, r->lost, r->tanksDestroyed, r->jetsDestroyed, r->dronesDestroyed};
    const int value[] = {200, 0, 100, 300, 150};
    float interval = r->tallyRow == 2 ? .11f : .045f;
    if (r->tallyDelay < interval)
        return;
    int row = r->tallyRow;
    if (r->tallyShown[row] < target[row])
    {
        r->tallyShown[row]++;
        r->tallyScore += value[row];
        r->tallyDelay -= interval;
        r->soundEvents |= RESCUE_SOUND_TALLY;
    }
    else
    {
        r->tallyRow++;
        r->tallyDelay = r->tallyRow == 5 ? 0 : -.2f;
    }
}

void rescueStep(Rescue *r, float dx, float dy, bool fire, bool dropBomb, float dt)
{
    if (r->over)
        return;
    dt = clamp(dt, 0, .034f);
    if (r->tally)
    {
        rescueCountWave(r, dt);
        return;
    }
    if (r->win)
        return;
    r->time += dt;
    for (int i = 0; i < PEOPLE; i++)
        r->ouchLife[i] = fmaxf(0, r->ouchLife[i] - dt);
    for (int i = 0; i < 8; i++)
        r->bombImpacts[i].life = fmaxf(0, r->bombImpacts[i].life - dt);
    bool landedNearHouse = r->y >= RESCUE_LAND_Y - 3 && fabsf(r->vx) < 35 && fabsf(r->vy) < 25 && r->crashTimer <= 0;
    if (landedNearHouse)
        for (int i = 0; i < RESCUE_HOUSES; i++)
            if (fabsf(r->x - r->houses[i].x) < 110)
                r->houses[i].evacuating = true;
    rescueHouseEvacuation(r, dt);
    if (r->crashTimer > 0)
    {
        r->crashTimer = fmaxf(0, r->crashTimer - dt);
        passengersGoHome(r, dt);
        if (r->crashTimer <= 0)
        {
            if (r->lives <= 0)
                r->over = true;
            else
            {
                r->x = 80;
                r->y = 130;
                r->camera = 0;
                r->facing = 1;
                r->yaw = 1;
                r->tilt = r->frontHold = r->boarding = 0;
            }
        }
        return;
    }
    r->shield = fmaxf(0, r->shield - dt);
    r->cooldown -= dt;
    r->gunCooldown -= dt;
    if (fabsf(dx) > .1f)
        r->facing = dx > 0 ? 1 : -1;
    /* Nose-down lean follows active steering, not residual velocity. */
    float targetTilt = fabsf(dx) > .1f ? clamp(dx, -1, 1) * .22f : 0;
    float tiltStep = dt * 1.5f;
    if (r->tilt < targetTilt)
        r->tilt = fminf(targetTilt, r->tilt + tiltStep);
    else
        r->tilt = fmaxf(targetTilt, r->tilt - tiltStep);
    /* Two 0.18-second half-turns with a 0.16-second front-facing dwell. */
    float turnDt = dt;
    if (r->frontHold > 0)
    {
        float held = fminf(turnDt, r->frontHold);
        r->frontHold -= held;
        turnDt -= held;
    }
    float previous = r->yaw, turnSpeed = 2.f / .36f, turnStep = turnDt * turnSpeed;
    if (r->yaw < r->facing)
        r->yaw = fminf((float)r->facing, r->yaw + turnStep);
    else if (r->yaw > r->facing)
        r->yaw = fmaxf((float)r->facing, r->yaw - turnStep);
    bool crossed =
        (previous > 0 && r->facing < 0 && r->yaw <= .0001f) || (previous < 0 && r->facing > 0 && r->yaw >= -.0001f);
    if (crossed)
    {
        r->yaw = 0;
        r->frontHold = fmaxf(0, .16f - fmaxf(0, turnDt - fabsf(previous) / turnSpeed));
    }
    if (fabsf(r->yaw - r->facing) < .0001f)
        r->yaw = (float)r->facing;
    r->vx = clamp((r->vx + dx * 230 * dt) * powf(.3f, dt), -145, 145);
    r->vy = clamp((r->vy + dy * 230 * dt) * powf(.12f, dt), -100, 100);
    float previousY = r->y;
    float landingSpeed = r->vy;
    r->x = clamp(r->x + r->vx * dt, 20, RESCUE_WORLD_WIDTH - 20);
    r->y = clamp(r->y + r->vy * dt, 35, RESCUE_LAND_Y);
    if (previousY < RESCUE_LAND_Y && r->y >= RESCUE_LAND_Y && landingSpeed > 0)
        for (int i = 0; i < PEOPLE; i++)
            if ((r->people[i].state == PERSON_WAITING || r->people[i].state == PERSON_FLEEING) &&
                fabsf(r->people[i].x - r->x) < 19)
            {
                r->people[i].state = PERSON_LOST;
                r->lost++;
                r->ouchLife[i] = .85f;
                r->soundEvents |= RESCUE_SOUND_OUCH;
            }
    if (r->y >= RESCUE_LAND_Y)
    {
        if (r->vy > 65)
            rescueHit(r);
        if (r->crashTimer > 0)
            return;
        r->vy = 0;
    }
    if (r->x <= 20 || r->x >= RESCUE_WORLD_WIDTH - 20)
        r->vx = 0;
    r->camera = clamp(r->x - 200, 0, RESCUE_WORLD_WIDTH - 400);
    if (fire && r->gunCooldown <= 0)
    {
        float aimX = r->facing * cosf(r->tilt), aimY = r->facing * sinf(r->tilt);
        bullet(r, r->x + aimX * 23, r->y + 2 + aimY * 23, aimX * 290 + r->vx * .3f, aimY * 290, false, true);
        r->gunCooldown = .14f;
    }
    if (dropBomb && r->cooldown <= 0 && r->y < RESCUE_LAND_Y - 11)
    {
        /* Release from the belly with the aircraft's horizontal momentum. */
        bullet(r, r->x, r->y + 11, r->vx, fmaxf(20, r->vy), false, false);
        r->cooldown = .45f;
    }
    bool landed = r->y >= RESCUE_LAND_Y - 3 && fabsf(r->vx) < 35 && fabsf(r->vy) < 25;
    for (int i = 0; i < PEOPLE; i++)
    {
        Person *person = &r->people[i];
        if (person->state != PERSON_WAITING && person->state != PERSON_FLEEING)
            continue;
        bool fleeing = person->state == PERSON_FLEEING;
        float target = fleeing ? r->peopleHome[i] : r->peopleHome[i] + sinf(r->time * .8f + i * 1.7f) * 14;
        if (landed && r->x >= 165 && fabsf(person->x - r->x) < 70)
            target = r->x;
        float speed = fleeing ? 22 + i % 3 * 3 : 10;
        person->x += clamp(target - person->x, -speed * dt, speed * dt);
        if (fleeing && fabsf(person->x - r->peopleHome[i]) < 1)
            person->state = PERSON_WAITING;
    }
    passengersGoHome(r, dt);
    if (landed && r->x < 165)
    {
        r->boarding = 0;
        r->unloadDelay -= dt;
        if (r->unloadDelay <= 0)
            for (int i = 0; i < PEOPLE; i++)
                if (r->people[i].state == PERSON_ABOARD)
                {
                    Person *person = &r->people[i];
                    HomeWalk *walk = &r->homeWalk[i];
                    person->state = PERSON_GOING_HOME;
                    person->x = r->x;
                    walk->action = HOME_WALK;
                    walk->stops = 0;
                    walk->timer = .3f + passengerRandom(walk) * 1.3f;
                    r->unloadDelay = .16f + passengerRandom(walk) * .34f;
                    r->onboard--;
                    r->rescued++;
                    r->score += 200;
                    break;
                }
    }
    else if (landed)
    {
        r->boarding += dt;
        if (r->boarding > .35f)
        {
            r->boarding = 0;
            if (r->onboard < RESCUE_CAPACITY)
                for (int i = 0; i < PEOPLE; i++)
                    if ((r->people[i].state == PERSON_WAITING || r->people[i].state == PERSON_FLEEING) &&
                        fabsf(r->people[i].x - r->x) < 45)
                    {
                        r->people[i].state = PERSON_ABOARD;
                        r->onboard++;
                        break;
                    }
        }
    }
    else
        r->boarding = 0;
    rescuePatrols(r, dt);
    for (int i = 0; i < TANKS; i++)
    {
        Tank *t = &r->tanks[i];
        if (!t->alive)
            continue;
        t->x += r->tankVelocity[i] * dt;
        if (t->x < RESCUE_TANK_MIN_X || t->x > RESCUE_WORLD_WIDTH - 45)
        {
            t->x = clamp(t->x, RESCUE_TANK_MIN_X, RESCUE_WORLD_WIDTH - 45);
            r->tankVelocity[i] = -r->tankVelocity[i];
        }
        t->fire -= dt;
        if (t->fire <= 0)
        {
            int houseTarget = -1;
            bool visibleTank = t->x >= r->camera - 15 && t->x <= r->camera + 415;
            if (visibleTank)
                for (int house = 0; house < RESCUE_HOUSES; house++)
                {
                    const RescueHouse *home = &r->houses[house];
                    if (!home->burning && home->x >= r->camera + 24 && home->x <= r->camera + 376 &&
                        fabsf(home->x - t->x) < 280 && patrolRandom(r) < .55f)
                    {
                        houseTarget = house;
                        break;
                    }
                }
            if (houseTarget >= 0 || fabsf(t->x - r->x) < 340)
            {
                float tx = houseTarget >= 0 ? r->houses[houseTarget].x : r->x;
                float ty = houseTarget >= 0 ? RESCUE_HORIZON - 10 : r->y;
                float a = atan2f(ty - RESCUE_TANK_Y, tx - t->x);
                float speed = 65 + fminf(fmaxf(0, r->stage - 1) * 7, 70);
                if (houseTarget < 0)
                {
                    /* Early crews have wide aim scatter and slow reloads.
                       House fire remains reliable so residents can escape. */
                    float spread = fmaxf(.045f, .28f - fmaxf(0, r->stage - 1) * .026f);
                    a += (patrolRandom(r) * 2 - 1) * spread;
                }
                bullet(r, t->x, RESCUE_TANK_Y, cosf(a) * speed, sinf(a) * speed, true, false);
            }
            if (houseTarget >= 0)
                t->fire = 3.5f;
            else
                t->fire = fmaxf(1.8f, 4.8f - fmaxf(0, r->stage - 1) * .3f) + patrolRandom(r) * 1.2f;
        }
        if (fabsf(t->x - r->x) < 22 && r->y > RESCUE_LAND_Y - 14)
            rescueHit(r);
    }
    rescueAirCombat(r, dt);
    for (int i = 0; i < BULLETS; i++)
    {
        Bullet *b = &r->bullets[i];
        if (b->life <= 0)
            continue;
        b->life -= dt;
        if (b->life <= 0)
            continue;
        if (!b->hostile && !b->forward)
            b->vy += 150 * dt;
        b->x += b->vx * dt;
        b->y += b->vy * dt;
        if (b->y > 219 || b->y < 10 || b->x < 0 || b->x > RESCUE_WORLD_WIDTH)
        {
            if (!b->hostile && !b->forward && b->y > 219 && b->x >= 0 && b->x <= RESCUE_WORLD_WIDTH)
                bombImpact(r, b->x, 218);
            b->life = 0;
            continue;
        }
        if (b->hostile)
        {
            for (int house = 0; house < RESCUE_HOUSES; house++)
            {
                RescueHouse *home = &r->houses[house];
                if (fabsf(b->x - home->x) <= 24 && b->y >= RESCUE_HORIZON - 30 && b->y <= RESCUE_HORIZON)
                {
                    b->life = 0;
                    if (!home->burning)
                    {
                        home->burning = true;
                        home->escapeDelay = .15f;
                        r->soundEvents |= RESCUE_SOUND_HOUSE_HIT;
                    }
                    break;
                }
            }
            if (b->life <= 0)
                continue;
            if (hypotf(b->x - r->x, b->y - r->y) < 15)
            {
                b->life = 0;
                rescueHit(r);
            }
        }
        else
        {
            for (int j = 0; j < RESCUE_AIRCRAFT && b->life > 0; j++)
            {
                RescueAircraft *air = &r->aircraft[j];
                float hitX = (b->x - air->x) / (air->kind == RESCUE_JET ? 20.f : 11.f);
                float hitY = (b->y - air->y) / (air->kind == RESCUE_JET ? 7.f : 11.f);
                if (air->alive && hitX * hitX + hitY * hitY < 1)
                {
                    b->life = 0;
                    air->hp--;
                    if (air->hp <= 0)
                    {
                        air->alive = false;
                        bombImpact(r, air->x, air->y);
                        if (air->kind == RESCUE_JET)
                        {
                            r->jetsDestroyed++;
                            r->score += 300;
                        }
                        else
                        {
                            r->dronesDestroyed++;
                            r->score += 150;
                        }
                        r->soundEvents |= RESCUE_SOUND_HOUSE_HIT;
                        r->aircraftSpawnDelay = fmaxf(r->aircraftSpawnDelay, fmaxf(6, 14 - r->stage * .6f));
                    }
                }
            }
            for (int mine = 0; mine < RESCUE_MINES && b->life > 0; mine++)
                if (r->mines[mine].alive && hypotf(b->x - r->mines[mine].x, b->y - r->mines[mine].y) < 8)
                {
                    b->life = 0;
                    r->mines[mine].alive = false;
                    bombImpact(r, r->mines[mine].x, r->mines[mine].y);
                }
            for (int j = 0; j < TANKS && b->life > 0; j++)
                if (r->tanks[j].alive && fabsf(b->x - r->tanks[j].x) < 15 && b->y >= RESCUE_TANK_Y - 9)
                {
                    r->tanks[j].alive = false;
                    bombImpact(r, r->tanks[j].x, RESCUE_TANK_Y);
                    b->life = 0;
                    r->score += 100;
                    r->tanksDestroyed++;
                    break;
                }
        }
    }
    if (r->score > r->best)
        r->best = r->score;
    if (r->rescued + r->lost == PEOPLE && r->crashTimer <= 0)
    {
        bool allInside = true;
        for (int i = 0; i < PEOPLE; i++)
            allInside &=
                (r->people[i].state == PERSON_INSIDE || r->people[i].state == PERSON_LOST) && r->ouchLife[i] <= 0;
        r->win = allInside;
        if (allInside)
        {
            r->tally = true;
            r->tallyRow = 0;
            r->tallyDelay = -.4f;
            r->tallyScore = r->stageStartScore;
            memset(r->tallyShown, 0, sizeof(r->tallyShown));
        }
    }
}
