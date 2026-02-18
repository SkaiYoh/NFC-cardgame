# NFC Tabletop Card Game

## Commands

```bash
# Build
make cardgame                # Main game binary → ./cardgame
make preview                 # Card preview tool → ./card_preview

# Run (starts PostgreSQL via Docker, then runs)
docker compose up -d
make run                     # Cleans, builds, and runs with default DB connection

# Card preview (no DB needed)
make preview-run

# Docker
docker compose up -d    # Start container
docker compose down     # Stop container
docker compose logs -f  # Follow logs
```

## Card Flow

```bash
Production flow (for reference)

NFC read  → cards_find(card_id) → g->currentPlayerIndex = playerIndex
          → card_action_play(card, g)
               → play_knight(card, state)
                    → spawn_troop_from_card(card, state)
                         → troop_create_data_from_card(card)   ← reads JSON data
                         → troop_spawn(player, data, pos, atlas)
                         → player_add_entity(player, e)
```
