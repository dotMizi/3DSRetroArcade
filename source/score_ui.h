#ifndef SCORE_UI_H
#define SCORE_UI_H
#include "scores.h"
/* Keyboard indexes 0..36: A-Z, 0-9, space. */
extern const char scoreKeys[];
int scoreKeyHit(int x,int y);
void drawScoreList(const Scores*s,int game,int highlight,bool entering);
void drawScoreKeyboard(int rank,const char name[4],int score,int cursor,int key);
void drawScoreActions(bool saveFailed);
void drawExitDialog(bool saveFailed);
#endif
