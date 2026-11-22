/*
    Copyright (C) 2005 Michael K. McCarty & Fritz Bronner

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
// Interplay's BUZZ ALDRIN's RACE into SPACE
//
// Formerly -=> LiftOff : Race to the Moon :: IBM version MCGA
// Copyright 1991 by Strategic Visions, Inc.
// Designed by Fritz Bronner
// Programmed by Michael K McCarty
//
//Equipment Randomization
//Enabling this option will change game balance, and can possibly break the AI

/* Unit Weight is based in the previous equipment (e.g. Gemini's weight is Mercury's + random number)
    some units have fixed ranges, loosely based on the Basic Model
Max Safety is a random number between 80 and 98
Max RD is 2 to 10 lower than Max Safety
Initial Cost is a random number, loosely based on the Basic Model initial cost
Unit Cost is loosely based on the Basic Model, but depends on initial cost
RD cost is loosely based on the Basic Model
*/
#include "randomize.h"

#include <cassert>
#include <cctype>

#include "display/graphics.h"

#include "Buzz_inc.h"
#include "draw.h"
#include "game_main.h"
#include "gamedata.h"
#include "pace.h"
#include "scenario.h"
#include "sdlhelper.h"


char GetNewName(char name[20]);


/**
 * Randomize the equipment values of the game state.
 *
 * The rules (which program's values depend on which, the ranges) are part
 * of the scenario's "random" model, see ApplyRandomModel().  They address
 * programs by role, not by list position.
 */
void RandomizeEq(void)
{
    ApplyRandomModel(*Data, ScenarioOf(*Data));
}


void SetEquipName(char plr)
{
    char skip = 0;
    fill_rectangle(95, 50, 227, 96, 0);
    OutBox(96, 51, 226, 95);
    fill_rectangle(97, 52, 225, 94, 3);
    display::graphics.setForegroundColor(9);
    draw_string(103, 59, "EQUIPMENT NAME CHANGE");
    draw_string(123, 69, "(ESCAPE TO SKIP)");
    int n;

    // The first 3 probes, the first 7 manned programs and the first 4
    // rockets can be renamed (the classic hardware without boosters).
    for (n = 0; skip == 0 && n < 3 && n < (int)Data->P[plr].Probe.size(); n++) {
        skip = GetNewName(Data->P[plr].Probe[n].Name);
    }

    for (n = 0; skip == 0 && n < 7 && n < (int)Data->P[plr].Manned.size(); n++) {
        skip = GetNewName(Data->P[plr].Manned[n].Name);
    }

    for (n = 0; skip == 0 && n < 4 && n < (int)Data->P[plr].Rocket.size(); n++) {
        skip = GetNewName(Data->P[plr].Rocket[n].Name);
    }
}


char GetNewName(char old_name[20])
{
    char ch = 0, num = 0, name[20];
    int i;

    for (i = 0; i < 20; i++) {
        name[i] = 0x00;
    }

    fill_rectangle(100, 74, 223, 91, 3);
    display::graphics.setForegroundColor(6);
    draw_string(162 - 3 * strlen(old_name), 80, &old_name[0]);
    InBox(99, 84, 223, 92);
    fill_rectangle(100, 85, 222, 91, 0);
    display::graphics.setForegroundColor(1);
    draw_string(162 - 3 * strlen(old_name), 90, &old_name[0]);
    draw_character(0x14);
    av_sync();

    while (ch != K_ENTER && ch != K_ESCAPE) {
        ch = getch();

        if (ch != (ch & 0xff)) {
            ch = 0x00;
        }

        if (ch >= 'a' && ch <= 'z') {
            ch -= 0x20;
        }

        if (ch == 0x08 && num > 0) {
            name[--num] = 0x00;
        } else if (num < 10 && (isupper(ch) || isdigit(ch) || ch == 0x20 || ch == '-')) {
            name[num++] = ch;
        }

        fill_rectangle(100, 85, 222, 91, 0);
        display::graphics.setForegroundColor(1);
        draw_string(162 - 3 * strlen(name), 90, &name[0]);
        draw_character(0x14);
        av_sync();
    }

    if (ch == K_ESCAPE) {
        return 1;
    }

    if (ch == K_ENTER) {
        if (name[0] == '\0') {
            return 0;
        } else {
            strcpy(&old_name[0], &name[0]);
        }
    }

    delay(100);
    return 0;
}
