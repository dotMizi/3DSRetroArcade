#ifndef SCORES_H
#define SCORES_H
#include <stdbool.h>
#define SCORE_GAMES 3
#define SCORE_LIMIT 10

typedef struct
{
    char name[4];
    int score;
} ScoreEntry;

typedef struct
{
    ScoreEntry entries[SCORE_GAMES][SCORE_LIMIT];
    int count[SCORE_GAMES];
} Scores;

void scoresInit(Scores *s);
bool scoreCharacter(char c);
int scoresRank(const Scores *s, int game, int score);
int scoresInsert(Scores *s, int game, int score, const char name[4]);
int scoresBest(const Scores *s, int game);
bool scoresLoadRecover(Scores *s, const char *path);
bool scoresLoad(Scores *s, const char *path);
const char *scoresSaveError(void);
void scoresSetSaveError(const char *stage, int error);
bool scoresSave(const Scores *s, const char *path);
#endif
