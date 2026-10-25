#ifndef DELTA_AUDIO_H
#define DELTA_AUDIO_H
#include <stdbool.h>
#include "game.h"
bool deltaAudioInit(void);
bool deltaAudioAvailable(void);
void deltaAudioUpdate(Game*g,bool active);
void deltaAudioExit(void);
#endif
