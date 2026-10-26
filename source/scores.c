#include "scores.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
static char saveError[64];

const char *scoresSaveError(void)
{
    return saveError;
}

void scoresSetSaveError(const char *stage, int error)
{
    snprintf(saveError, sizeof(saveError), "%s ERR %d", stage, error ? error : EIO);
}

void scoresInit(Scores *s)
{
    memset(s, 0, sizeof(*s));
}

bool scoreCharacter(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ';
}

int scoresRank(const Scores *s, int game, int score)
{
    if (game < 0 || game >= SCORE_GAMES || score <= 0)
        return -1;
    for (int i = 0; i < s->count[game]; i++)
        if (score > s->entries[game][i].score)
            return i;
    return s->count[game] < SCORE_LIMIT ? s->count[game] : -1;
}

int scoresInsert(Scores *s, int game, int score, const char name[4])
{
    int rank = scoresRank(s, game, score);
    if (rank < 0)
        return -1;
    int count = s->count[game];
    if (count < SCORE_LIMIT)
        count++;
    for (int i = count - 1; i > rank; i--)
        s->entries[game][i] = s->entries[game][i - 1];
    ScoreEntry *e = &s->entries[game][rank];
    e->score = score;
    for (int i = 0; i < 3; i++)
        e->name[i] = scoreCharacter(name[i]) ? name[i] : ' ';
    e->name[3] = 0;
    s->count[game] = count;
    return rank;
}

int scoresBest(const Scores *s, int game)
{
    return s->count[game] ? s->entries[game][0].score : 0;
}

/* Keep the format shared by the primary, temporary and backup files. */
static bool writeScores(const Scores *s, const char *path, const char *stage)
{
    errno = 0;
    FILE *f = fopen(path, "w");
    if (!f)
    {
        scoresSetSaveError(stage, errno);
        return false;
    }
    bool ok = fputs("RETROARCADE_TOP10_V1\n", f) >= 0;
    for (int g = 0; g < SCORE_GAMES; g++)
    {
        if (fprintf(f, "%d\n", s->count[g]) < 0)
            ok = false;
        for (int i = 0; i < s->count[g]; i++)
        {
            const ScoreEntry *e = &s->entries[g][i];
            if (fprintf(f,
                        "%d %02X %02X %02X\n",
                        e->score,
                        (unsigned char)e->name[0],
                        (unsigned char)e->name[1],
                        (unsigned char)e->name[2]) < 0)
                ok = false;
        }
    }
    if (!ok)
        scoresSetSaveError(stage, errno);
    if (ok && fflush(f) != 0)
    {
        scoresSetSaveError(stage, errno);
        ok = false;
    }
    if (fclose(f) != 0 && ok)
    {
        scoresSetSaveError(stage, errno);
        ok = false;
    }
    return ok;
}

static bool verifyScores(const Scores *s, const char *path, const char *stage)
{
    Scores check;
    scoresInit(&check);
    errno = 0;
    if (!scoresLoad(&check, path))
    {
        scoresSetSaveError(stage, errno);
        return false;
    }
    for (int g = 0; g < SCORE_GAMES; g++)
    {
        if (check.count[g] != s->count[g])
        {
            scoresSetSaveError(stage, EIO);
            return false;
        }
        for (int i = 0; i < s->count[g]; i++)
            if (check.entries[g][i].score != s->entries[g][i].score ||
                strcmp(check.entries[g][i].name, s->entries[g][i].name))
            {
                scoresSetSaveError(stage, EIO);
                return false;
            }
    }
    return true;
}

bool scoresSave(const Scores *s, const char *path)
{
    saveError[0] = 0;
    char tmp[512], backup[512];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp) ||
        snprintf(backup, sizeof(backup), "%s.bak", path) >= (int)sizeof(backup))
    {
        scoresSetSaveError("PATH", ENAMETOOLONG);
        return false;
    }
    /* No rename: the device reports ENOENT at that step. Keep a validated
    pending copy and a previous valid list before opening the primary. */
    if (!writeScores(s, tmp, "WRITE TEMP") || !verifyScores(s, tmp, "VERIFY TEMP"))
        return false;
    Scores old;
    scoresInit(&old);
    if (scoresLoad(&old, path))
    {
        if (!writeScores(&old, backup, "WRITE BACKUP") || !verifyScores(&old, backup, "VERIFY BACKUP"))
            return false;
    }
    if (!writeScores(s, path, "WRITE SCORES") || !verifyScores(s, path, "VERIFY SCORES"))
        return false;
    remove(tmp);
    return true;
}

bool scoresLoadRecover(Scores *s, const char *path)
{
    if (scoresLoad(s, path))
        return true;
    char other[512];
    if (snprintf(other, sizeof(other), "%s.tmp", path) < (int)sizeof(other) && scoresLoad(s, other))
        return true;
    return snprintf(other, sizeof(other), "%s.bak", path) < (int)sizeof(other) && scoresLoad(s, other);
}

bool scoresLoad(Scores *s, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return false;
    Scores loaded;
    scoresInit(&loaded);
    char line[128], extra;
    bool ok = fgets(line, sizeof(line), f) && strcmp(line, "RETROARCADE_TOP10_V1\n") == 0;
    for (int g = 0; g < SCORE_GAMES && ok; g++)
    {
        long count;
        char *end;
        if (!fgets(line, sizeof(line), f))
        {
            ok = false;
            break;
        }
        count = strtol(line, &end, 10);
        if (end == line || (*end != '\n' && *end != 0) || count < 0 || count > SCORE_LIMIT)
        {
            ok = false;
            break;
        }
        for (int i = 0; i < count; i++)
        {
            long long score;
            unsigned a, b, c;
            if (!fgets(line, sizeof(line), f) || sscanf(line, "%lld %x %x %x %c", &score, &a, &b, &c, &extra) != 4 ||
                score <= 0 || score > INT_MAX || a > 127 || b > 127 || c > 127 || !scoreCharacter((char)a) ||
                !scoreCharacter((char)b) || !scoreCharacter((char)c))
            {
                ok = false;
                break;
            }
            char name[4] = {(char)a, (char)b, (char)c, 0};
            if (i > 0 && score > loaded.entries[g][i - 1].score)
            {
                ok = false;
                break;
            }
            scoresInsert(&loaded, g, (int)score, name);
        }
    }
    if (ok)
    {
        int ch;
        while ((ch = fgetc(f)) != EOF)
            if (ch != '\n' && ch != '\r' && ch != ' ' && ch != '\t')
            {
                ok = false;
                break;
            }
    }
    if (ferror(f))
        ok = false;
    fclose(f);
    if (ok)
        *s = loaded;
    return ok;
}
