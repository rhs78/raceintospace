#ifndef ENDGAME_H
#define ENDGAME_H

void FakeWin(char win);
void NewEnd(char win, char loc, bool landing = true);
int PrestigeTotal(int plr);
int FinalScoreWinner();
void FinalScoreEnd();
void PlayFirst(char plr, char first);
void PlayAllFirsts(char plr);
void SpecialEnd();

extern char PF[29][40];

#endif // ENDGAME_H
