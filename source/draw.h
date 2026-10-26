#ifndef DRAW_H
#define DRAW_H
#include "game.h"
#include "arcade.h"
void canvas(unsigned char *pixels, int width);
void clear(unsigned color);
void pixel(int x, int y, unsigned color);
void line(int x, int y, int xx, int yy, unsigned color);
void box(int x, int y, int w, int h, unsigned color);
void fill(int x, int y, int w, int h, unsigned color);
void circle(int x, int y, int r, unsigned color);
void text(int x, int y, const char *s, unsigned color, int scale);
void drawDelta(Game *g);
void drawDeltaPanel(const Game *g, int best);
void drawShield(Shield *s);
void drawShieldTallyPanel(const Shield *s);
void drawRescue(Rescue *r);
void drawRescueTallyPanel(const Rescue *r);
void drawMenu(int selected);
void drawPanel(int game, int score, int best, const char *status);
#endif
