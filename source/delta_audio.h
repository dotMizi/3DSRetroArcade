#ifndef DELTA_AUDIO_H
#define DELTA_AUDIO_H
#include <stdbool.h>
#include "game.h"
#include "arcade.h"
bool deltaAudioInit(void);
bool deltaAudioAvailable(void);
void deltaAudioUpdate(Game *g, bool active);
void shieldAudioUpdate(Shield *s, bool active);
void rescueAudioUpdate(Rescue *r, bool active, float dt);
void deltaAudioExit(void);
#endif
