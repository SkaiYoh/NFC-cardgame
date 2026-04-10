//
// Assault slot reservation for static combat targets.
// See assault_slots.h for the public contract.
//

#include "assault_slots.h"
#include "../core/config.h"
#include <assert.h>
#include <math.h>

void assault_slots_release_for_entity(Entity *base, int entityId);

#define ASSAULT_SLOTS_PI 3.14159265358979323846f

static Vector2 assault_slots_rotate(Vector2 v, float degrees) {
    float rad = degrees * ASSAULT_SLOTS_PI / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);
    return (Vector2){ v.x * c - v.y * s, v.x * s + v.y * c };
}

static float assault_slots_dist_sq(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static float assault_slots_contact_radius(const Entity *base) {
    if (!base) return 0.0f;

    float navR = (base->navRadius > 0.0f) ? base->navRadius : base->bodyRadius;
    float combatRadius = navR - COMBAT_BUILDING_MELEE_INSET;
    if (combatRadius < base->bodyRadius) combatRadius = base->bodyRadius;
    return combatRadius;
}

static void assault_slots_build_arc(AssaultSlot *slots, int count,
                                    Vector2 origin, Vector2 forward,
                                    float radius, float arcDegrees) {
    if (count <= 0) return;

    float half = arcDegrees * 0.5f;
    for (int i = 0; i < count; i++) {
        float t = (count == 1)
            ? 0.0f
            : ((float)i / (float)(count - 1)) * 2.0f - 1.0f;
        float angle = t * half;
        Vector2 dir = assault_slots_rotate(forward, angle);
        slots[i].worldPos.x = origin.x + dir.x * radius;
        slots[i].worldPos.y = origin.y + dir.y * radius;
        slots[i].claimedByEntityId = -1;
    }
}

void assault_slots_build_for_base(Entity *base) {
    if (!base) return;

    AssaultSlotRing *ring = &base->assaultSlots;
    float contactRadius = assault_slots_contact_radius(base);
    float primaryRadius = contactRadius + DEFAULT_MELEE_BODY_RADIUS + BASE_ASSAULT_SLOT_GAP;
    float queueRadius = primaryRadius + BASE_ASSAULT_QUEUE_RADIAL_OFFSET;

    Vector2 forward = (base->presentationSide == SIDE_TOP)
        ? (Vector2){ 0.0f, 1.0f }
        : (Vector2){ 0.0f, -1.0f };

    assault_slots_build_arc(ring->primary, BASE_ASSAULT_PRIMARY_SLOT_COUNT,
                            base->position, forward, primaryRadius,
                            BASE_ASSAULT_PRIMARY_ARC_DEGREES);
    assault_slots_build_arc(ring->queue, BASE_ASSAULT_QUEUE_SLOT_COUNT,
                            base->position, forward, queueRadius,
                            BASE_ASSAULT_QUEUE_ARC_DEGREES);

    ring->initialized = true;

    for (int i = 0; i < BASE_ASSAULT_PRIMARY_SLOT_COUNT; i++) {
        assert(ring->primary[i].worldPos.x >= 0.0f);
        assert(ring->primary[i].worldPos.x <= (float)BOARD_WIDTH);
        assert(ring->primary[i].worldPos.y >= 0.0f);
        assert(ring->primary[i].worldPos.y <= (float)BOARD_HEIGHT);
    }
    for (int i = 0; i < BASE_ASSAULT_QUEUE_SLOT_COUNT; i++) {
        assert(ring->queue[i].worldPos.x >= 0.0f);
        assert(ring->queue[i].worldPos.x <= (float)BOARD_WIDTH);
        assert(ring->queue[i].worldPos.y >= 0.0f);
        assert(ring->queue[i].worldPos.y <= (float)BOARD_HEIGHT);
    }
}

static int assault_slots_closest_free_index(const AssaultSlot *slots, int count,
                                            Vector2 fromPos) {
    int bestIdx = -1;
    float bestDistSq = INFINITY;
    for (int i = 0; i < count; i++) {
        if (slots[i].claimedByEntityId != -1) continue;
        float d = assault_slots_dist_sq(slots[i].worldPos, fromPos);
        if (d < bestDistSq) {
            bestDistSq = d;
            bestIdx = i;
        }
    }
    return bestIdx;
}

AssaultSlotKind assault_slots_reserve_for(Entity *base, int entityId,
                                          Vector2 fromPos, int *outSlotIndex) {
    if (outSlotIndex) *outSlotIndex = -1;
    if (!base || !base->assaultSlots.initialized || !outSlotIndex) {
        return ASSAULT_SLOT_NONE;
    }

    assault_slots_release_for_entity(base, entityId);

    AssaultSlotRing *ring = &base->assaultSlots;
    int idx = assault_slots_closest_free_index(
        ring->primary, BASE_ASSAULT_PRIMARY_SLOT_COUNT, fromPos);
    if (idx >= 0) {
        ring->primary[idx].claimedByEntityId = entityId;
        *outSlotIndex = idx;
        return ASSAULT_SLOT_PRIMARY;
    }

    idx = assault_slots_closest_free_index(
        ring->queue, BASE_ASSAULT_QUEUE_SLOT_COUNT, fromPos);
    if (idx >= 0) {
        ring->queue[idx].claimedByEntityId = entityId;
        *outSlotIndex = idx;
        return ASSAULT_SLOT_QUEUE;
    }

    return ASSAULT_SLOT_NONE;
}

bool assault_slots_try_promote(Entity *base, int entityId, int queueSlotIndex,
                               int *outPrimarySlotIndex) {
    if (!base || !base->assaultSlots.initialized || !outPrimarySlotIndex) {
        return false;
    }
    if (queueSlotIndex < 0 || queueSlotIndex >= BASE_ASSAULT_QUEUE_SLOT_COUNT) {
        return false;
    }

    AssaultSlotRing *ring = &base->assaultSlots;
    if (ring->queue[queueSlotIndex].claimedByEntityId != entityId) {
        return false;
    }

    Vector2 queuePos = ring->queue[queueSlotIndex].worldPos;
    int idx = assault_slots_closest_free_index(
        ring->primary, BASE_ASSAULT_PRIMARY_SLOT_COUNT, queuePos);
    if (idx < 0) return false;

    ring->primary[idx].claimedByEntityId = entityId;
    ring->queue[queueSlotIndex].claimedByEntityId = -1;
    *outPrimarySlotIndex = idx;
    return true;
}

void assault_slots_release(Entity *base, AssaultSlotKind kind, int slotIndex,
                           int entityId) {
    if (!base || !base->assaultSlots.initialized) return;

    AssaultSlotRing *ring = &base->assaultSlots;
    if (kind == ASSAULT_SLOT_PRIMARY &&
        slotIndex >= 0 && slotIndex < BASE_ASSAULT_PRIMARY_SLOT_COUNT) {
        if (ring->primary[slotIndex].claimedByEntityId == entityId) {
            ring->primary[slotIndex].claimedByEntityId = -1;
        }
        return;
    }
    if (kind == ASSAULT_SLOT_QUEUE &&
        slotIndex >= 0 && slotIndex < BASE_ASSAULT_QUEUE_SLOT_COUNT) {
        if (ring->queue[slotIndex].claimedByEntityId == entityId) {
            ring->queue[slotIndex].claimedByEntityId = -1;
        }
    }
}

void assault_slots_release_for_entity(Entity *base, int entityId) {
    if (!base || !base->assaultSlots.initialized) return;

    AssaultSlotRing *ring = &base->assaultSlots;
    for (int i = 0; i < BASE_ASSAULT_PRIMARY_SLOT_COUNT; i++) {
        if (ring->primary[i].claimedByEntityId == entityId) {
            ring->primary[i].claimedByEntityId = -1;
        }
    }
    for (int i = 0; i < BASE_ASSAULT_QUEUE_SLOT_COUNT; i++) {
        if (ring->queue[i].claimedByEntityId == entityId) {
            ring->queue[i].claimedByEntityId = -1;
        }
    }
}

Vector2 assault_slots_get_position(const Entity *base, AssaultSlotKind kind,
                                   int slotIndex) {
    Vector2 zero = { 0.0f, 0.0f };
    if (!base || !base->assaultSlots.initialized) return zero;

    const AssaultSlotRing *ring = &base->assaultSlots;
    if (kind == ASSAULT_SLOT_PRIMARY &&
        slotIndex >= 0 && slotIndex < BASE_ASSAULT_PRIMARY_SLOT_COUNT) {
        return ring->primary[slotIndex].worldPos;
    }
    if (kind == ASSAULT_SLOT_QUEUE &&
        slotIndex >= 0 && slotIndex < BASE_ASSAULT_QUEUE_SLOT_COUNT) {
        return ring->queue[slotIndex].worldPos;
    }
    return zero;
}

int assault_slots_primary_count(const Entity *base) {
    (void)base;
    return BASE_ASSAULT_PRIMARY_SLOT_COUNT;
}

int assault_slots_queue_count(const Entity *base) {
    (void)base;
    return BASE_ASSAULT_QUEUE_SLOT_COUNT;
}

const AssaultSlot *assault_slots_primary_at(const Entity *base, int slotIndex) {
    if (!base || slotIndex < 0 || slotIndex >= BASE_ASSAULT_PRIMARY_SLOT_COUNT) {
        return NULL;
    }
    return &base->assaultSlots.primary[slotIndex];
}

const AssaultSlot *assault_slots_queue_at(const Entity *base, int slotIndex) {
    if (!base || slotIndex < 0 || slotIndex >= BASE_ASSAULT_QUEUE_SLOT_COUNT) {
        return NULL;
    }
    return &base->assaultSlots.queue[slotIndex];
}
