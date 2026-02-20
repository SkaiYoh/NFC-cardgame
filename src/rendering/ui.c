//
// Created by Nathan Davis on 2/16/26.
//

#include "ui.h"

// TODO: UI system is completely unimplemented. This file is empty.
// TODO: The game has no in-game HUD — no energy bars, no card hand display, no health indicators,
// TODO: no win/lose screen, and no pregame lobby UI. Implement at minimum:
//   ui_draw_energy_bar()  — show each player's current / max energy
//   ui_draw_card_hand()   — render the active card hand above each viewport
//   ui_draw_health()      — show base HP for each player
//   ui_draw_game_over()   — display winner text when win_trigger() fires
// TODO: UI draws should happen after EndMode2D (in screen space) and before EndDrawing.