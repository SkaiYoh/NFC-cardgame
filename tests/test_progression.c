/*
 * Unit tests for src/systems/progression.c.
 *
 * Self-contained: stubs just enough GameState/Player/Entity to cover
 * progression_sync_player without pulling in the full types.h include chain.
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ---- Prevent heavy include chains ---- */
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_PLAYER_H
#define NFC_CARDGAME_PROGRESSION_H

/* ---- Minimal type stubs ---- */
typedef struct { float x; float y; } Vector2;

typedef struct Entity {
    int baseLevel;
    bool alive;
    bool markedForRemoval;
} Entity;

typedef struct Player {
    float energyRegenRate;
    float baseEnergyRegenRate;
    float energyRegenBoostMultiplier;
    float energyRegenBoostRemaining;
    Entity *base;
    int sustenanceBank;
    int sustenanceCollected;
} Player;

typedef struct GameState {
    Player players[2];
} GameState;

/* Pull the constants and function prototypes from the header without using
 * the include guard (we already set NFC_CARDGAME_PROGRESSION_H above). */
#define PROGRESSION_MAX_LEVEL            10
#define PROGRESSION_REGEN_LEVEL1         0.5f
#define PROGRESSION_REGEN_LEVEL_MAX      2.0f
#define PROGRESSION_KING_DMG_LEVEL1      220
#define PROGRESSION_KING_DMG_LEVEL_MAX   355
#define PROGRESSION_KING_BURST_RADIUS    280.0f

int   progression_level_from_sustenance(int sustenance);
float progression_regen_rate_for_level(int level);
int   progression_king_burst_damage_for_level(int level);
void  progression_sync_player(GameState *gs, int playerIndex);

void player_set_base_energy_regen_rate(Player *p, float baseRate) {
    if (!p) return;
    if (baseRate < 0.0f) baseRate = 0.0f;
    p->baseEnergyRegenRate = baseRate;
    if (p->energyRegenBoostRemaining > 0.0f && p->energyRegenBoostMultiplier > 1.0f) {
        p->energyRegenRate = baseRate * p->energyRegenBoostMultiplier;
    } else {
        p->energyRegenRate = baseRate;
    }
}

/* ---- Production code under test ---- */
#include "../src/systems/progression.c"

/* ---- Helpers ---- */
static bool approx_eq(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

#define RUN_TEST(fn) do { \
    printf("  "); \
    fn(); \
    printf("PASS: %s\n", #fn); \
} while (0)

/* ---- Tests ---- */
static void test_level_thresholds(void) {
    assert(progression_level_from_sustenance(0) == 1);
    assert(progression_level_from_sustenance(6) == 1);
    assert(progression_level_from_sustenance(7) == 2);
    assert(progression_level_from_sustenance(20) == 2);
    assert(progression_level_from_sustenance(21) == 3);
    assert(progression_level_from_sustenance(58) == 4);
    assert(progression_level_from_sustenance(59) == 5);
    assert(progression_level_from_sustenance(167) == 8);
    assert(progression_level_from_sustenance(168) == 9);
    assert(progression_level_from_sustenance(200) == 10);
    assert(progression_level_from_sustenance(9999) == 10); /* clamp */
    assert(progression_level_from_sustenance(-50) == 1);   /* defensive */
}

static void test_regen_rate_curve(void) {
    assert(approx_eq(progression_regen_rate_for_level(1),
                     PROGRESSION_REGEN_LEVEL1, 0.0001f));
    assert(approx_eq(progression_regen_rate_for_level(10),
                     PROGRESSION_REGEN_LEVEL_MAX, 0.0001f));

    float lv5 = progression_regen_rate_for_level(5);
    assert(lv5 > PROGRESSION_REGEN_LEVEL1 && lv5 < PROGRESSION_REGEN_LEVEL_MAX);
    /* level 5 sits at (5-1)/9 = 0.4444 of the span. */
    assert(approx_eq(lv5,
                     PROGRESSION_REGEN_LEVEL1 +
                     (PROGRESSION_REGEN_LEVEL_MAX - PROGRESSION_REGEN_LEVEL1) *
                     (4.0f / 9.0f),
                     0.0001f));

    /* Clamp below/above bounds. */
    assert(approx_eq(progression_regen_rate_for_level(0),
                     PROGRESSION_REGEN_LEVEL1, 0.0001f));
    assert(approx_eq(progression_regen_rate_for_level(99),
                     PROGRESSION_REGEN_LEVEL_MAX, 0.0001f));
}

static void test_king_burst_damage_curve(void) {
    int expected[11] = { 0, 220, 235, 250, 265, 280, 295, 310, 325, 340, 355 };
    for (int lv = 1; lv <= 10; lv++) {
        assert(progression_king_burst_damage_for_level(lv) == expected[lv]);
    }
    /* Clamp. */
    assert(progression_king_burst_damage_for_level(0) == 220);
    assert(progression_king_burst_damage_for_level(42) == 355);
}

static void test_sync_player_updates_base_level_and_regen(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    Entity base = { .alive = true, .markedForRemoval = false, .baseLevel = 1 };
    gs.players[0].base = &base;
    gs.players[0].sustenanceCollected = 0;

    progression_sync_player(&gs, 0);
    assert(base.baseLevel == 1);
    assert(approx_eq(gs.players[0].energyRegenRate,
                     PROGRESSION_REGEN_LEVEL1, 0.0001f));

    gs.players[0].sustenanceCollected = 59;
    progression_sync_player(&gs, 0);
    assert(base.baseLevel == 5);
    assert(approx_eq(gs.players[0].energyRegenRate,
                     PROGRESSION_REGEN_LEVEL1 +
                     (PROGRESSION_REGEN_LEVEL_MAX - PROGRESSION_REGEN_LEVEL1) *
                     (4.0f / 9.0f),
                     0.0001f));

    gs.players[0].sustenanceCollected = 200;
    progression_sync_player(&gs, 0);
    assert(base.baseLevel == 10);
    assert(approx_eq(gs.players[0].energyRegenRate,
                     PROGRESSION_REGEN_LEVEL_MAX, 0.0001f));
}

static void test_sync_player_ignores_spendable_sustenance_bank(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    Entity base = { .alive = true, .markedForRemoval = false, .baseLevel = 1 };
    gs.players[0].base = &base;
    gs.players[0].sustenanceCollected = 59;
    gs.players[0].sustenanceBank = 0;

    progression_sync_player(&gs, 0);
    assert(base.baseLevel == 5);
    assert(approx_eq(gs.players[0].energyRegenRate,
                     progression_regen_rate_for_level(5), 0.0001f));
}

static void test_sync_player_without_live_base_still_updates_regen(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.players[1].base = NULL;
    gs.players[1].sustenanceCollected = 83;

    progression_sync_player(&gs, 1);
    assert(gs.players[1].base == NULL);
    assert(approx_eq(gs.players[1].energyRegenRate,
                     progression_regen_rate_for_level(6), 0.0001f));
}

static void test_sync_player_skips_dead_base(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    Entity base = { .alive = false, .markedForRemoval = false, .baseLevel = 4 };
    gs.players[0].base = &base;
    gs.players[0].sustenanceCollected = 200;

    progression_sync_player(&gs, 0);
    assert(base.baseLevel == 4);          /* stale level preserved; base is dead */
    /* Regen still reflects current sustenance. */
    assert(approx_eq(gs.players[0].energyRegenRate,
                     PROGRESSION_REGEN_LEVEL_MAX, 0.0001f));
}

static void test_sync_player_ignores_bad_index(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.players[0].energyRegenRate = 5.0f;
    progression_sync_player(&gs, -1);
    progression_sync_player(&gs, 2);
    assert(approx_eq(gs.players[0].energyRegenRate, 5.0f, 0.0001f));
}

int main(void) {
    printf("Running progression tests...\n");
    RUN_TEST(test_level_thresholds);
    RUN_TEST(test_regen_rate_curve);
    RUN_TEST(test_king_burst_damage_curve);
    RUN_TEST(test_sync_player_updates_base_level_and_regen);
    RUN_TEST(test_sync_player_ignores_spendable_sustenance_bank);
    RUN_TEST(test_sync_player_without_live_base_still_updates_regen);
    RUN_TEST(test_sync_player_skips_dead_base);
    RUN_TEST(test_sync_player_ignores_bad_index);
    printf("\nAll progression tests passed!\n");
    return 0;
}
