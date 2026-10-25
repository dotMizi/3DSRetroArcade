#ifndef DELTA_INTRO_H
#define DELTA_INTRO_H
#include <stdbool.h>
#include "scores.h"
extern const char deltaIntroText[];
int deltaIntroVisible(float elapsed);
bool deltaIntroComplete(float elapsed);
void drawDeltaIntro(float elapsed);
void drawDeltaIntroPanel(const Scores*s,bool ready);
bool deltaIntroStartHit(int x,int y);
#endif
