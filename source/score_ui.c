#include "score_ui.h"
#include "draw.h"
#include <stdio.h>
const char scoreKeys[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";
static const char *games[] = {"DELTA RACE", "SKY SHIELD", "RESCUE WING"};

int scoreKeyHit(int x, int y)
{
    if (x < 10 || y < 65)
        return -1;
    int col = (x - 10) / 30, row = (y - 65) / 28;
    if (col >= 10 || row >= 4 || (x - 10) % 30 >= 27 || (y - 65) % 28 >= 24)
        return -1;
    int key = row * 10 + col;
    return key < 37 ? key : -1;
}

void drawScoreList(const Scores *s, int game, int highlight, bool entering)
{
    char b[64];
    clear(0x080b20);
    text(16, 8, games[game], 0x58eaff, 2);
    text(280, 12, "TOP 10", 0xffa34f, 2);
    text(20, 38, "RANK", 0x9989b4, 1);
    text(92, 38, "NAME", 0x9989b4, 1);
    text(240, 38, "SCORE", 0x9989b4, 1);
    for (int i = 0; i < SCORE_LIMIT; i++)
    {
        int y = 54 + i * 16;
        unsigned c = i == highlight ? 0xffa34f : 0xe1e9ff;
        if (i == highlight)
            fill(12, y - 1, 376, 16, 0x30233d);
        snprintf(b, sizeof(b), "%02d", i + 1);
        text(20, y, b, c, 2);
        text(92, y, i < s->count[game] ? s->entries[game][i].name : "---", c, 2);
        if (i < s->count[game])
            snprintf(b, sizeof(b), "%10d", s->entries[game][i].score);
        else
            snprintf(b, sizeof(b), "         -");
        text(240, y, b, c, 2);
    }
    text(16,
         225,
         entering ? "ENTER INITIALS ON THE TOUCH SCREEN" : "A: NEW GAME   SELECT: MENU   START: EXIT",
         0x9989b4,
         1);
}

void drawScoreKeyboard(int rank, const char name[4], int score, int cursor, int key)
{
    char b[64];
    clear(0x080b20);
    text(46, 8, "ENTER YOUR INITIALS", 0x58eaff, 2);
    snprintf(b, sizeof(b), "%02d", rank + 1);
    text(15, 36, b, 0xffa34f, 2);
    for (int i = 0; i < 3; i++)
    {
        box(86 + i * 24, 30, 21, 24, i == cursor ? 0xffa34f : 0x384363);
        char c[2] = {name[i], 0};
        text(91 + i * 24, 35, c, 0xffffff, 2);
    }
    snprintf(b, sizeof(b), "%10d", score);
    text(188, 38, b, 0xffffff, 2);
    for (int i = 0; i < 37; i++)
    {
        int x = 10 + (i % 10) * 30, y = 65 + (i / 10) * 28;
        fill(x, y, 27, 24, i == key ? 0x28556c : 0x151d38);
        box(x, y, 26, 23, i == key ? 0x58eaff : 0x384363);
        char c[2] = {scoreKeys[i], 0};
        text(x + (i == 36 ? 7 : 10), y + 8, i == 36 ? "SP" : c, 0xffffff, 1);
    }
    text(12, 181, "D-PAD: KEY   A: TYPE   L/R: SLOT", 0x9989b4, 1);
    text(12, 194, "SELECT / EXIT: SAVE CURRENT NAME", 0x9989b4, 1);
    box(10, 207, 143, 26, 0xff5bbd);
    text(36, 217, "B: DELETE", 0xff5bbd, 1);
    box(166, 207, 143, 26, 0x58eaff);
    text(211, 217, "Y: SAVE", 0x58eaff, 1);
}

void drawScoreActions(bool saveFailed)
{
    clear(0x080b20);
    text(46, 25, "HIGH SCORES", 0x58eaff, 3);
    text(37, 84, "A: START A NEW GAME", 0xffffff, 2);
    text(40, 127, "SELECT: GAME SELECTION", 0xcbb7e8, 1);
    text(40, 149, "START: EXIT PROGRAM", 0xcbb7e8, 1);
    if (saveFailed)
    {
        text(22, 185, "COULD NOT SAVE SCORES TO SD CARD", 0xff547b, 1);
        text(22, 202, scoresSaveError(), 0xff547b, 1);
        text(82, 222, "Y: RETRY SAVING", 0xffa34f, 1);
    }
}

void drawExitDialog(bool saveFailed)
{
    fill(20, 45, 280, 155, 0x080b20);
    box(20, 45, 280, 155, 0x58eaff);
    text(52, 65, "EXIT RETROARCADE", 0xffffff, 2);
    text(52, 106, "A: EXIT   B: CANCEL", 0xcbb7e8, 1);
    if (saveFailed)
    {
        text(37, 135, "SD SAVE FAILED - A: RETRY", 0xff547b, 1);
        text(37, 154, scoresSaveError(), 0xff547b, 1);
        text(37, 177, "X: EXIT WITHOUT SAVING", 0xffa34f, 1);
    }
    else
        text(43, 145, "PENDING INITIALS WILL BE SAVED", 0x9989b4, 1);
}
