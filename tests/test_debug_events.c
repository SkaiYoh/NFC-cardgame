/*
 * Unit tests for src/core/debug_events.c ring buffer.
 *
 * Self-contained: includes debug_events.c directly with a minimal
 * Vector2 stub. Compiles with `make test_debug_events` using only -lm.
 */

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

/* ---- Prevent heavy headers ---- */
#define RAYLIB_H
#define NFC_CARDGAME_DEBUG_EVENTS_H

/* ---- No Raylib dependencies — debug_events uses float posX/posY ---- */

/* ---- Debug event types (must match debug_events.h) ---- */
#define DEBUG_EVENT_CAPACITY 32

typedef enum {
    DEBUG_EVT_STATE_CHANGE = 0,
    DEBUG_EVT_HIT          = 1,
    DEBUG_EVT_DEATH_FINISH = 2,
} DebugEventType;

typedef struct {
    float posX, posY;
    DebugEventType type;
    float remaining;
    float duration;
} DebugFlash;

void debug_event_emit_xy(float x, float y, DebugEventType type);
void debug_events_tick(float dt);
const DebugFlash *debug_events_buffer(void);
void debug_events_clear(void);

/* ---- Include production code ---- */
#include "../src/core/debug_events.c"

/* ---- Test helpers ---- */
static int count_active(void) {
    const DebugFlash *buf = debug_events_buffer();
    int n = 0;
    for (int i = 0; i < DEBUG_EVENT_CAPACITY; i++) {
        if (buf[i].remaining > 0.0f) n++;
    }
    return n;
}

/* ---- Test harness ---- */
static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(fn) do { \
    debug_events_clear(); \
    printf("  "); \
    fn(); \
    tests_run++; \
    tests_passed++; \
    printf("PASS: %s\n", #fn); \
} while(0)

/* ---- Tests ---- */

static void test_emit_one(void) {
    debug_event_emit_xy(10.0f, 20.0f, DEBUG_EVT_HIT);
    assert(count_active() == 1);

    const DebugFlash *buf = debug_events_buffer();
    // Find the active one
    int found = -1;
    for (int i = 0; i < DEBUG_EVENT_CAPACITY; i++) {
        if (buf[i].remaining > 0.0f) { found = i; break; }
    }
    assert(found >= 0);
    assert(buf[found].type == DEBUG_EVT_HIT);
    assert(fabsf(buf[found].posX - 10.0f) < 0.001f);
    assert(buf[found].remaining > 0.0f);
    assert(buf[found].duration > 0.0f);
}

static void test_tick_expires(void) {
    debug_event_emit_xy(0, 0, DEBUG_EVT_HIT);
    assert(count_active() == 1);

    // Tick past the duration (HIT = 0.2s)
    debug_events_tick(0.3f);
    assert(count_active() == 0);
}

static void test_tick_partial(void) {
    debug_event_emit_xy(0, 0, DEBUG_EVT_STATE_CHANGE); // 0.3s
    debug_events_tick(0.1f);
    assert(count_active() == 1); // still active

    debug_events_tick(0.25f);
    assert(count_active() == 0); // expired
}

static void test_multiple_events(void) {
    debug_event_emit_xy(1, 1, DEBUG_EVT_HIT);
    debug_event_emit_xy(2, 2, DEBUG_EVT_STATE_CHANGE);
    debug_event_emit_xy(3, 3, DEBUG_EVT_DEATH_FINISH);
    assert(count_active() == 3);
}

static void test_wrap_around(void) {
    // Fill entire buffer + 1 to test wrap
    for (int i = 0; i < DEBUG_EVENT_CAPACITY + 1; i++) {
        debug_event_emit_xy((float)i, 0, DEBUG_EVT_HIT);
    }
    // All 32 slots should be active (the 33rd overwrote the 1st)
    assert(count_active() == DEBUG_EVENT_CAPACITY);
}

static void test_overflow_overwrites_oldest(void) {
    // Emit one, then fill the rest + one more to overwrite it
    debug_event_emit_xy(999.0f, 0, DEBUG_EVT_STATE_CHANGE);

    for (int i = 0; i < DEBUG_EVENT_CAPACITY; i++) {
        debug_event_emit_xy((float)i, 0, DEBUG_EVT_HIT);
    }

    // The 999.0f event should be overwritten
    const DebugFlash *buf = debug_events_buffer();
    bool found999 = false;
    for (int i = 0; i < DEBUG_EVENT_CAPACITY; i++) {
        if (buf[i].remaining > 0.0f && fabsf(buf[i].posX - 999.0f) < 0.001f) {
            found999 = true;
        }
    }
    assert(!found999);
}

static void test_clear(void) {
    debug_event_emit_xy(0, 0, DEBUG_EVT_HIT);
    debug_event_emit_xy(0, 0, DEBUG_EVT_HIT);
    assert(count_active() == 2);

    debug_events_clear();
    assert(count_active() == 0);
}

static void test_durations_by_type(void) {
    debug_event_emit_xy(0, 0, DEBUG_EVT_STATE_CHANGE);
    debug_event_emit_xy(1, 1, DEBUG_EVT_HIT);
    debug_event_emit_xy(2, 2, DEBUG_EVT_DEATH_FINISH);

    const DebugFlash *buf = debug_events_buffer();
    for (int i = 0; i < DEBUG_EVENT_CAPACITY; i++) {
        if (buf[i].remaining <= 0.0f) continue;
        switch (buf[i].type) {
            case DEBUG_EVT_STATE_CHANGE:
                assert(fabsf(buf[i].duration - 0.3f) < 0.001f);
                break;
            case DEBUG_EVT_HIT:
                assert(fabsf(buf[i].duration - 0.2f) < 0.001f);
                break;
            case DEBUG_EVT_DEATH_FINISH:
                assert(fabsf(buf[i].duration - 0.4f) < 0.001f);
                break;
        }
    }
}

/* ---- Main ---- */
int main(void) {
    printf("Running debug_events tests...\n");

    RUN_TEST(test_emit_one);
    RUN_TEST(test_tick_expires);
    RUN_TEST(test_tick_partial);
    RUN_TEST(test_multiple_events);
    RUN_TEST(test_wrap_around);
    RUN_TEST(test_overflow_overwrites_oldest);
    RUN_TEST(test_clear);
    RUN_TEST(test_durations_by_type);

    printf("\nAll %d tests passed!\n", tests_passed);
    return 0;
}
