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
    for (int i = 0; i < PEOPLE; i++)
    {
        r->people[i] = (Person){400 + (i / 3) * 300 + (i % 3) * 18, 0};
        r->peopleHome[i] = r->people[i].x;
        HomeWalk *walk = &r->homeWalk[i];
        walk->rng = 0x9e3779b9u ^ ((unsigned)(i + 1) * 0x85ebca6bu) ^ (unsigned)best ^ ((unsigned)stage << 16);
        if (!walk->rng)
            walk->rng = 1;
        walk->speed = 12 + passengerRandom(walk) * 15;
        walk->facing = 1;
    }
    for (int i = 0; i < TANKS; i++)
    {
        r->tanks[i] = (Tank){280 + i * 240, 1.5f + i * .3f, true};
        r->tankVelocity[i] = (i % 2 ? -1 : 1) * (14 + i * 2);
    }
}

static void rescueHit(Rescue *r)
{
    if (r->shield > 0)
        return;
    r->lives--;
    for (int i = 0; i < PEOPLE; i++)
        if (r->people[i].state == 1)
            r->people[i].state = 0;
    r->onboard = 0;
    r->x = 80;
    r->y = 130;
    r->vx = r->vy = 0;
    r->shield = 3;
    if (!r->lives)
        r->over = true;
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

void rescueStep(Rescue *r, float dx, float dy, bool fire, bool dropBomb, float dt)
{
    if (r->over || r->win)
        return;
    dt = clamp(dt, 0, .034f);
    r->time += dt;
    for (int i = 0; i < 8; i++)
        r->bombImpacts[i].life = fmaxf(0, r->bombImpacts[i].life - dt);
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
    r->x = clamp(r->x + r->vx * dt, 20, 1580);
    r->y = clamp(r->y + r->vy * dt, 35, RESCUE_LAND_Y);
    if (r->y >= RESCUE_LAND_Y)
    {
        if (r->vy > 65)
            rescueHit(r);
        r->vy = 0;
    }
    if (r->x <= 20 || r->x >= 1580)
        r->vx = 0;
    r->camera = clamp(r->x - 200, 0, 1200);
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
        if (person->state != 0)
            continue;
        float target = r->peopleHome[i] + sinf(r->time * .8f + i * 1.7f) * 14;
        if (landed && r->x >= 165 && fabsf(person->x - r->x) < 70)
            target = r->x;
        person->x += clamp(target - person->x, -10 * dt, 10 * dt);
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
                    if (r->people[i].state == PERSON_WAITING && fabsf(r->people[i].x - r->x) < 45)
                    {
                        r->people[i].state = PERSON_ABOARD;
                        r->onboard++;
                        break;
                    }
        }
    }
    else
        r->boarding = 0;
    for (int i = 0; i < TANKS; i++)
    {
        Tank *t = &r->tanks[i];
        if (!t->alive)
            continue;
        t->x += r->tankVelocity[i] * dt;
        if (t->x < 230 || t->x > 1555)
        {
            t->x = clamp(t->x, 230, 1555);
            r->tankVelocity[i] = -r->tankVelocity[i];
        }
        t->fire -= dt;
        if (t->fire <= 0)
        {
            if (fabsf(t->x - r->x) < 340)
            {
                float a = atan2f(r->y - RESCUE_TANK_Y, r->x - t->x), speed = 75 + fminf(r->stage * 8, 65);
                bullet(r, t->x, RESCUE_TANK_Y, cosf(a) * speed, sinf(a) * speed, true, false);
            }
            t->fire = fmaxf(1, 3.5f - r->stage * .2f) + i * .13f;
        }
        if (fabsf(t->x - r->x) < 22 && r->y > RESCUE_LAND_Y - 14)
            rescueHit(r);
    }
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
        if (b->y > 219 || b->y < 10 || b->x < 0 || b->x > 1600)
        {
            if (!b->hostile && !b->forward && b->y > 219 && b->x >= 0 && b->x <= 1600)
                bombImpact(r, b->x, 218);
            b->life = 0;
            continue;
        }
        if (b->hostile)
        {
            if (hypotf(b->x - r->x, b->y - r->y) < 15)
            {
                b->life = 0;
                rescueHit(r);
            }
        }
        else
            for (int j = 0; j < TANKS; j++)
                if (r->tanks[j].alive && fabsf(b->x - r->tanks[j].x) < 15 && b->y >= RESCUE_TANK_Y - 9)
                {
                    r->tanks[j].alive = false;
                    bombImpact(r, r->tanks[j].x, RESCUE_TANK_Y);
                    b->life = 0;
                    r->score += 100;
                    break;
                }
    }
    if (r->score > r->best)
        r->best = r->score;
    if (r->rescued == PEOPLE)
    {
        bool allInside = true;
        for (int i = 0; i < PEOPLE; i++)
            allInside &= r->people[i].state == PERSON_INSIDE;
        r->win = allInside;
    }
}
