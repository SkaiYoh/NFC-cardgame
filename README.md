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
