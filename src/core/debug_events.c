//
// Debug event ring buffer implementation.
//

#include "debug_events.h"
#include <string.h>

static DebugFlash s_events[DEBUG_EVENT_CAPACITY];
static int s_writeHead = 0;

// Duration per event type
static const float s_durations[] = {
    [DEBUG_EVT_STATE_CHANGE] = 0.3f,
    [DEBUG_EVT_HIT]          = 0.2f,
    [DEBUG_EVT_DEATH_FINISH] = 0.4f,
};

void debug_event_emit_xy(float x, float y, DebugEventType type) {
    float dur = (type >= 0 && type <= DEBUG_EVT_DEATH_FINISH)
        ? s_durations[type] : 0.3f;

    s_events[s_writeHead] = (DebugFlash){
        .posX = x,
        .posY = y,
        .type = type,
        .remaining = dur,
        .duration = dur,
    };
    s_writeHead = (s_writeHead + 1) % DEBUG_EVENT_CAPACITY;
}

void debug_events_tick(float dt) {
    for (int i = 0; i < DEBUG_EVENT_CAPACITY; i++) {
        if (s_events[i].remaining > 0.0f) {
            s_events[i].remaining -= dt;
        }
    }
}

const DebugFlash *debug_events_buffer(void) {
    return s_events;
}

void debug_events_clear(void) {
    memset(s_events, 0, sizeof(s_events));
    s_writeHead = 0;
}
