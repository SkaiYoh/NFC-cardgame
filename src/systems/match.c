//
// Created by Nathan Davis on 2/16/26.
//

void pregame_init(GameState *gs);
void pregame_update(GameState *gs);
void pregame_render(GameState *gs);
void pregame_handle_card_scan(GameState *gs, NFCEvent *event);
bool pregame_are_players_ready(GameState *gs);