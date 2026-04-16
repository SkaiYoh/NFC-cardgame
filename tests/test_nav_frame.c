/*
 * Unit tests for src/logic/nav_frame.c.
 *
 * Self-contained: stubs a minimal Battlefield, suppresses the real
 * battlefield.h / battlefield_math.h chain, and #includes nav_frame.c
 * directly. Compiles with `-lm` only.
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Prevent the real battlefield headers from pulling in raylib / sqlite3. */
#define NFC_CARDGAME_BATTLEFIELD_H
#define NFC_CARDGAME_BATTLEFIELD_MATH_H
#define VECTOR2_DEFINED

/* Redefine the small set of battlefield.h types that nav_frame.c reads from.
 * Must agree byte-for-byte with the real declarations for the fields nav_frame
 * touches (laneWaypoints access patterns). */
typedef struct { float x; float y; } Vector2;
typedef struct { Vector2 v; } CanonicalPos;
typedef enum { SIDE_BOTTOM, SIDE_TOP } BattleSide;

#define LANE_WAYPOINT_COUNT 8
#define NUM_CARD_SLOTS 3
#define MAX_ENTITIES 64
#define BOARD_WIDTH  1080
#define BOARD_HEIGHT 1920
#define SEAM_Y       960

typedef struct Battlefield {
    float boardWidth;
    float boardHeight;
    float seamY;
    CanonicalPos laneWaypoints[2][3][LANE_WAYPOINT_COUNT];
    CanonicalPos slotSpawnAnchors[2][NUM_CARD_SLOTS];
} Battlefield;

/* Stub out the config.h include chain by pre-defining the header guard and
 * supplying the constants nav_frame.c needs. This mirrors how
 * test_pathfinding.c isolates pathfinding.c from its heavy dependency chain. */

/* ------ Now pull in nav_frame.c ------
 * Include the .c directly; nav_frame.h still gets included from nav_frame.c,
 * but its only external dep is config.h. The test intentionally shares
 * config.h with production so constant drift fails the build. */
#include "../src/logic/nav_frame.c"

/* ---- Helpers ---- */

static void lane_waypoints_linear(Battlefield *bf, int side, int lane,
                                  float x, float yStart, float yEnd) {
    for (int i = 0; i < LANE_WAYPOINT_COUNT; ++i) {
        float t = (float)i / (float)(LANE_WAYPOINT_COUNT - 1);
        bf->laneWaypoints[side][lane][i].v.x = x;
        bf->laneWaypoints[side][lane][i].v.y = yStart + t * (yEnd - yStart);
    }
}

static void make_default_battlefield(Battlefield *bf) {
    memset(bf, 0, sizeof(*bf));
    bf->boardWidth = BOARD_WIDTH;
    bf->boardHeight = BOARD_HEIGHT;
    bf->seamY = SEAM_Y;
    /* Three straight vertical lanes on each side. SIDE_BOTTOM spawns near
     * y = BOARD_HEIGHT and marches up to y = 0; SIDE_TOP does the opposite. */
    float laneX[3] = { 180.0f, 540.0f, 900.0f };
    for (int lane = 0; lane < 3; ++lane) {
        lane_waypoints_linear(bf, SIDE_BOTTOM, lane, laneX[lane],
                              BOARD_HEIGHT - 120.0f, 120.0f);
        lane_waypoints_linear(bf, SIDE_TOP, lane, laneX[lane],
                              120.0f, BOARD_HEIGHT - 120.0f);
    }
}

static NavFreeGoalRequest make_free_goal_request(float goalX, float goalY,
                                                 float stopRadius,
                                                 int perspectiveSide) {
    NavFreeGoalRequest request = {
        .goalX = goalX,
        .goalY = goalY,
        .stopRadius = stopRadius,
        .perspectiveSide = (int16_t)perspectiveSide,
        .carveTargetId = -1,
        .carveCenterX = 0.0f,
        .carveCenterY = 0.0f,
        .carveInnerRadius = 0.0f,
    };
    return request;
}

/* ---- Tests ---- */

static void test_grid_constants(void) {
    /* 1080 / 32 = 33.75 -> 34 cols; 1920 / 32 = 60 rows. */
    assert(NAV_COLS == 34);
    assert(NAV_ROWS == 60);
    assert(NAV_CELLS == 34 * 60);
    printf("  [ok] grid constants\n");
}

static void test_coord_helpers(void) {
    /* Cell (0,0) center is (16,16). */
    float x, y;
    nav_cell_center(0, &x, &y);
    assert(fabsf(x - 16.0f) < 1e-3f);
    assert(fabsf(y - 16.0f) < 1e-3f);

    /* World (16,16) maps back to cell 0. */
    assert(nav_cell_index_for_world(16.0f, 16.0f) == 0);

    /* World (47,16) still lands in column 1, row 0. */
    int32_t idx = nav_cell_index_for_world(47.0f, 16.0f);
    NavCellCoord c = nav_cell_coord(idx);
    assert(c.col == 1);
    assert(c.row == 0);

    /* Out-of-bounds coordinates clamp to the nearest edge cell. */
    int32_t negIdx = nav_cell_index_for_world(-500.0f, -500.0f);
    assert(negIdx == 0);
    int32_t farIdx = nav_cell_index_for_world(100000.0f, 100000.0f);
    c = nav_cell_coord(farIdx);
    assert(c.col == NAV_COLS - 1);
    assert(c.row == NAV_ROWS - 1);

    printf("  [ok] coordinate helpers\n");
}

static void test_begin_frame_clears_state(void) {
    NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);

    nav_begin_frame(&nav, &bf);
    assert(nav.initialized);
    assert(nav.frameCounter == 1);
    assert(nav.targetCacheSize == 0);
    assert(nav.freeGoalCacheSize == 0);
    for (int s = 0; s < 2; ++s) {
        for (int l = 0; l < 3; ++l) {
            assert(!nav.laneFields[s][l].built);
        }
    }

    /* Second begin increments the counter and keeps state consistent. */
    nav_begin_frame(&nav, &bf);
    assert(nav.frameCounter == 2);
    printf("  [ok] nav_begin_frame clears caches\n");
}

static void test_lane_field_monotone_toward_seed(void) {
    NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    const NavField *field = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 1);
    assert(field != NULL);
    assert(field->built);
    assert(field->kind == NAV_GOAL_KIND_LANE_MARCH);

    /* Seed sits at the final authored waypoint near (540, 120). Distance
     * there should be 0 or minimal; distance much further away along the
     * same lane (near the spawn at y ~= 1800) should be strictly greater. */
    int32_t seedCell = nav_cell_index_for_world(540.0f, 120.0f);
    int32_t farCell  = nav_cell_index_for_world(540.0f, 1800.0f);
    assert(field->distance[seedCell] == 0);
    assert(field->distance[farCell] != NAV_DIST_UNREACHABLE);
    assert(field->distance[farCell] > field->distance[seedCell]);

    /* Cells progressively closer to the seed should have strictly lower
     * distance along a straight vertical line. */
    int32_t prev = NAV_DIST_UNREACHABLE;
    for (int y = 1800; y >= 120; y -= 64) {
        int32_t idx = nav_cell_index_for_world(540.0f, (float)y);
        int32_t d = field->distance[idx];
        assert(d != NAV_DIST_UNREACHABLE);
        if (prev != NAV_DIST_UNREACHABLE) {
            assert(d <= prev);
        }
        prev = d;
    }
    printf("  [ok] lane field monotone toward seed\n");
}

static void test_lane_field_home_corridor_masks_out_far_cells(void) {
    NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    /* Center lane for SIDE_BOTTOM lives at x=540. A cell at x=50 (way
     * outside the 96 px corridor) but still in the home half (y >= SEAM_Y)
     * must be hard-blocked in this lane's field. */
    const NavField *field = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 1);
    assert(field != NULL);

    int32_t farSideCell = nav_cell_index_for_world(50.0f, 1600.0f);
    assert(field->hardBlocked[farSideCell]);
    assert(field->distance[farSideCell] == NAV_DIST_UNREACHABLE);

    /* A cell tightly inside the corridor (x within ~96 of 540) must not be
     * hard-blocked and must have a finite distance. */
    int32_t insideCell = nav_cell_index_for_world(540.0f + 32.0f, 1600.0f);
    assert(!field->hardBlocked[insideCell]);
    assert(field->distance[insideCell] != NAV_DIST_UNREACHABLE);

    /* In the away half (y < SEAM_Y) the corridor mask does not apply --
     * a cell at (50, 400) should be reachable. */
    int32_t awayCell = nav_cell_index_for_world(50.0f, 400.0f);
    assert(!field->hardBlocked[awayCell]);
    assert(field->distance[awayCell] != NAV_DIST_UNREACHABLE);
    printf("  [ok] lane field home corridor masks far-side cells\n");
}

static void test_lane_field_cached_across_lookups(void) {
    NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    const NavField *first = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 0);
    const NavField *second = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 0);
    assert(first == second);
    assert(first->built);

    /* Rebuild after a fresh frame should clear the cache and give a valid
     * field again without asserting. */
    nav_begin_frame(&nav, &bf);
    assert(!nav.laneFields[SIDE_BOTTOM][0].built);
    const NavField *afterReset = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 0);
    assert(afterReset != NULL);
    assert(afterReset->built);
    printf("  [ok] lane field cached across lookups and rebuilt per frame\n");
}

static void test_lane_field_top_side_symmetry(void) {
    NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    /* SIDE_TOP seeds near y=1800; home-half is y < SEAM_Y. A cell at
     * (540, 100) (deep in SIDE_TOP's own half) should be unblocked, and a
     * cell at (50, 100) (out of corridor) should be blocked. */
    const NavField *field = nav_get_or_build_lane_field(&nav, &bf, SIDE_TOP, 1);
    assert(field != NULL);
    int32_t insideCell = nav_cell_index_for_world(540.0f, 100.0f);
    int32_t outsideCell = nav_cell_index_for_world(50.0f, 100.0f);
    assert(!field->hardBlocked[insideCell]);
    assert(field->hardBlocked[outsideCell]);
    /* Seed reachable at the far end. */
    int32_t seed = nav_cell_index_for_world(540.0f, BOARD_HEIGHT - 120.0f);
    assert(field->distance[seed] == 0);
    printf("  [ok] lane field top-side symmetry\n");
}

static void test_lane_field_requires_begin_frame(void) {
    NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    /* No nav_begin_frame: lookups must return NULL rather than fire on a
     * stale cache. */
    const NavField *field = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 0);
    assert(field == NULL);
    printf("  [ok] lane field lookup requires nav_begin_frame\n");
}

static void test_heap_preserves_order_under_mixed_distances(void) {
    NavFrame nav;
    nav_frame_init(&nav);
    /* Push a pathological sequence and verify nav_heap_pop returns in
     * non-decreasing order. */
    int seq[16] = { 47, 2, 91, 3, 18, 18, 0, 34, 76, 12, 9, 9, 50, 5, 1, 22 };
    for (int i = 0; i < 16; ++i) {
        nav_heap_push(&nav, i, seq[i]);
    }
    int32_t prev = -1;
    for (int i = 0; i < 16; ++i) {
        NavHeapNode n = nav_heap_pop(&nav);
        assert(n.dist >= prev);
        prev = n.dist;
    }
    assert(nav_heap_empty(&nav));
    printf("  [ok] min-heap ordering under mixed distances\n");
}

static void test_heap_capacity_bounds_stress(void) {
    /* Stress: push NAV_CELLS * 8 stale entries to cover the worst case of
     * lazy-deletion Dijkstra on the 8-neighbor grid plus NAV_CELLS seeds,
     * then drain. The capacity is 9 * NAV_CELLS; if the bound were wrong
     * we would either OOB or trip the assert. */
    static NavFrame nav;
    nav_frame_init(&nav);
    const int32_t pushCount = NAV_CELLS * 9 - 1;  /* one short of capacity */
    for (int32_t i = 0; i < pushCount; ++i) {
        /* Alternate distances so the heap actually has to swap. */
        nav_heap_push(&nav, i % NAV_CELLS, (i * 37) % 1000);
    }
    assert(nav.heapSize == pushCount);
    int32_t prev = -1;
    int32_t drained = 0;
    while (!nav_heap_empty(&nav)) {
        NavHeapNode n = nav_heap_pop(&nav);
        assert(n.dist >= prev);
        prev = n.dist;
        drained++;
    }
    assert(drained == pushCount);
    printf("  [ok] heap capacity stress (9 * NAV_CELLS drain)\n");
}

static void test_static_blockers_seal_outer_ring(void) {
    NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    /* Every cell on the outer border must be hard-blocked so flow fields
     * cannot legally path a unit into cells that clip the canonical board. */
    for (int32_t col = 0; col < NAV_COLS; ++col) {
        assert(nav_cell_is_static_blocked(&nav, nav_index(col, 0)));
        assert(nav_cell_is_static_blocked(&nav, nav_index(col, NAV_ROWS - 1)));
    }
    for (int32_t row = 0; row < NAV_ROWS; ++row) {
        assert(nav_cell_is_static_blocked(&nav, nav_index(0, row)));
        assert(nav_cell_is_static_blocked(&nav, nav_index(NAV_COLS - 1, row)));
    }
    /* Interior cell is unblocked. */
    assert(!nav_cell_is_static_blocked(&nav, nav_index(NAV_COLS / 2, NAV_ROWS / 2)));

    /* Lane field inherits the outer-ring mask, so the far edges must be
     * unreachable even when they sit inside the corridor geometry. */
    const NavField *field = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 1);
    assert(field->hardBlocked[nav_index(0, NAV_ROWS / 2)]);
    assert(field->hardBlocked[nav_index(NAV_COLS - 1, NAV_ROWS / 2)]);
    assert(field->distance[nav_index(0, NAV_ROWS / 2)] == NAV_DIST_UNREACHABLE);
    printf("  [ok] static blockers seal outer ring\n");
}

static void lane_waypoints_bow(Battlefield *bf, int side, int lane,
                                float yStart, float yEnd,
                                float xBase, float bowX) {
    /* Authored waypoints on a sinusoidal bow: the curve pushes laterally to
     * xBase + bowX at the midpoint and returns to xBase at the endpoints.
     * Exercises point-to-polyline distance and the corridor mask on a
     * non-trivial lane shape similar to what bf_init generates. */
    for (int i = 0; i < LANE_WAYPOINT_COUNT; ++i) {
        float t = (float)i / (float)(LANE_WAYPOINT_COUNT - 1);
        float y = yStart + t * (yEnd - yStart);
        float bow = sinf(t * 3.14159265f) * bowX;
        bf->laneWaypoints[side][lane][i].v.x = xBase + bow;
        bf->laneWaypoints[side][lane][i].v.y = y;
    }
}

static void test_integration_refuses_corner_cut(void) {
    /* Build a minimal scratch field with a manual hard-blocker L-shape and
     * assert that a diagonal step cannot squeeze through the shared corner
     * of two orthogonal blockers. */
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    NavField *field = &nav.laneFields[SIDE_BOTTOM][1];
    for (int32_t i = 0; i < NAV_CELLS; ++i) {
        field->distance[i] = NAV_DIST_UNREACHABLE;
        field->hardBlocked[i] = nav.staticBlockers.blocked[i];
    }
    /* Pick an interior anchor well away from the outer moat. */
    int32_t ac = NAV_COLS / 2;
    int32_t ar = NAV_ROWS / 2;
    int32_t anchor = nav_index(ac, ar);
    /* Block the two orthogonal neighbors to the NE of the anchor
     * (N cell and E cell) to form an L-shaped corner. */
    int32_t nCell = nav_index(ac,     ar - 1);
    int32_t eCell = nav_index(ac + 1, ar);
    int32_t neCell = nav_index(ac + 1, ar - 1);
    field->hardBlocked[nCell] = 1;
    field->hardBlocked[eCell] = 1;
    /* NE cell stays open. */
    field->distance[anchor] = 0;
    nav_integrate_field(&nav, field);
    /* With corner-cut prevention, NE cannot be reached from the anchor via
     * the diagonal step because both N and E are blocked. With every other
     * route around the two blockers at least 4 orthogonal steps long
     * (dist >= 40), reaching NE via diagonal would yield dist = 14. */
    assert(field->distance[neCell] != NAV_DIST_UNREACHABLE); /* still reachable, but not via the cut */
    assert(field->distance[neCell] >= 40);
    field->built = true;
    printf("  [ok] integration refuses diagonal corner cut\n");
}

static void test_seed_in_blocker_preserves_mask(void) {
    /* Plant a hard blocker in the center of the seed neighborhood on an
     * otherwise-default battlefield, then rebuild the lane field. The
     * seed helper must widen outward past the blocker, leave the blocker
     * cell hard-blocked, and still produce a reachable distance field. */
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    int32_t seedCell = nav_cell_index_for_world(540.0f, 120.0f);
    NavCellCoord c = nav_cell_coord(seedCell);
    /* Paint the seed cell as hard-blocked directly on the static mask, so
     * the subsequent lane-field build inherits it. */
    nav.staticBlockers.blocked[seedCell] = 1;

    const NavField *field = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 1);
    assert(field != NULL);
    assert(field->hardBlocked[seedCell]); /* not cleared */
    assert(field->distance[seedCell] == NAV_DIST_UNREACHABLE);
    /* An unblocked neighbor of the anchor must be a seed (distance == 0). */
    bool foundSeed = false;
    for (int dc = -1; dc <= 1 && !foundSeed; ++dc) {
        for (int dr = -1; dr <= 1 && !foundSeed; ++dr) {
            if (dc == 0 && dr == 0) continue;
            int32_t idx = nav_index(c.col + dc, c.row + dr);
            if (!field->hardBlocked[idx] && field->distance[idx] == 0) {
                foundSeed = true;
            }
        }
    }
    assert(foundSeed);
    /* Far cells along the lane remain reachable through the seed ring. */
    int32_t farCell = nav_cell_index_for_world(540.0f, 1600.0f);
    assert(field->distance[farCell] != NAV_DIST_UNREACHABLE);
    printf("  [ok] seed-in-blocker widens ring without clearing mask\n");
}

static void test_stamp_static_blocker_disk(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    /* Stamp a disk of radius 48 centered on a cell-center grid point. At
     * (528, 528) the center cell is col=16, row=16. All cells whose center
     * lies within 48 px must be blocked; cells further away must not be. */
    nav_stamp_static_blocker_disk(&nav, 528.0f, 528.0f, 48.0f);
    int32_t centerCell = nav_index(16, 16);
    assert(nav_cell_is_static_blocked(&nav, centerCell));
    /* One cell east: center (560, 528); dist = 32 < 48. Inside. */
    assert(nav_cell_is_static_blocked(&nav, nav_index(17, 16)));
    /* One cell north: center (528, 496); dist = 32 < 48. Inside. */
    assert(nav_cell_is_static_blocked(&nav, nav_index(16, 15)));
    /* Two cells east: center (592, 528); dist = 64 > 48. Outside. */
    assert(!nav_cell_is_static_blocked(&nav, nav_index(18, 16)));
    /* Diagonal at (17, 15): center (560, 496); dist = sqrt(32^2 + 32^2) =
     * ~45.3 < 48. Inside. */
    assert(nav_cell_is_static_blocked(&nav, nav_index(17, 15)));
    /* Diagonal two steps: center (592, 496); dist ~72 > 48. Outside. */
    assert(!nav_cell_is_static_blocked(&nav, nav_index(18, 15)));
    printf("  [ok] nav_stamp_static_blocker_disk\n");
}

static void test_stamp_density_counts_per_side(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    nav_stamp_density(&nav, 0, 540.0f, 540.0f);
    nav_stamp_density(&nav, 0, 540.0f, 540.0f);
    nav_stamp_density(&nav, 1, 540.0f, 540.0f);

    int32_t cell = nav_cell_index_for_world(540.0f, 540.0f);
    assert(nav.density[0][cell] == 2);
    assert(nav.density[1][cell] == 1);

    /* Begin next frame clears density. */
    nav_begin_frame(&nav, &bf);
    assert(nav.density[0][cell] == 0);
    assert(nav.density[1][cell] == 0);
    printf("  [ok] nav_stamp_density per-side counts and per-frame reset\n");
}

static void test_cell_flow_direction_points_downhill(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);
    const NavField *field = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 1);
    assert(field != NULL);

    /* A cell halfway down the lane should have a flow direction pointing
     * toward lower y (toward the seed at y=120). */
    int32_t midCell = nav_cell_index_for_world(540.0f, 960.0f);
    int dc, dr;
    nav_cell_flow_direction(field, midCell, &dc, &dr);
    /* The best neighbor should be the one with lower distance. Our seed is
     * upward (lower y); the flow direction's drow must be negative. */
    assert(dr < 0);
    printf("  [ok] cell flow direction points downhill toward seed\n");
}

static void test_sample_flow_bilinear_between_cells(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);
    const NavField *field = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 1);
    assert(field != NULL);

    /* Sample at a point inside the lane. The blended direction must be
     * non-zero and point broadly upward (dy < 0) toward the seed. */
    float fdx = 0.0f, fdy = 0.0f;
    nav_sample_flow(field, 540.0f, 960.0f, &fdx, &fdy);
    assert(fabsf(fdx) + fabsf(fdy) > 1e-4f);
    assert(fdy < 0.0f);

    /* Sample near the seed itself: the blended vector should still be
     * finite but can be small. */
    nav_sample_flow(field, 540.0f, 140.0f, &fdx, &fdy);
    /* Direction toward seed (dy<=0) or zero when at seed. No NaN. */
    assert(isfinite(fdx));
    assert(isfinite(fdy));
    printf("  [ok] nav_sample_flow bilinear blend\n");
}

static void test_sample_flow_at_blocker_is_zero(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);
    const NavField *field = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 1);
    assert(field != NULL);

    /* Far outside the lane corridor, flow should be zero because all four
     * sampled corners are hard-blocked (distance == UNREACHABLE). */
    float fdx = 1.0f, fdy = 1.0f;
    nav_sample_flow(field, 50.0f, 1600.0f, &fdx, &fdy);
    assert(fdx == 0.0f);
    assert(fdy == 0.0f);
    printf("  [ok] nav_sample_flow zeroes inside fully-blocked region\n");
}

static void test_target_field_static_arc(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    /* Stamp the target as a static blocker disk so its footprint is hard-
     * blocked and the arc ribbon has to live outside it. */
    nav_stamp_static_blocker_disk(&nav, 540.0f, 540.0f, 48.0f);

    NavTargetGoal goal = {
        .kind = NAV_GOAL_KIND_STATIC_ATTACK,
        .targetX = 540.0f,
        .targetY = 540.0f,
        .outerRadius = 80.0f,
        .arcCenterDeg = 90.0f,    /* downward (toward +y) */
        .arcHalfDeg = 80.0f,      /* 160-degree total arc */
        .targetId = 42,
        .perspectiveSide = 0,
    };
    const NavField *field = nav_get_or_build_target_field(&nav, &bf, &goal);
    assert(field != NULL);
    assert(field->built);
    assert(field->kind == NAV_GOAL_KIND_STATIC_ATTACK);

    /* A cell in the +y arc inside the new tight ribbon
     * [outerR - 2*half, outerR] = [48, 80]. Cell (16,18) has center
     * (528, 592), distance ~53 from target, bearing ~103 deg (within
     * the 160-deg arc centered at 90 deg). Must be a seed. */
    int32_t belowCell = nav_cell_index_for_world(540.0f, 592.0f);
    assert(field->distance[belowCell] == 0);

    /* A cell directly above the target (-y, OUTSIDE the 160-deg arc which
     * centers on +y) must NOT be a seed. It may still be reachable via
     * Dijkstra routing around, but it must not be distance 0. */
    int32_t aboveCell = nav_cell_index_for_world(540.0f, 540.0f - 80.0f);
    assert(field->distance[aboveCell] != 0);

    /* Cache hit on a second lookup returns the same field. */
    const NavField *again = nav_get_or_build_target_field(&nav, &bf, &goal);
    assert(again == field);
    printf("  [ok] target field static-arc ribbon + cache hit\n");
}

static void test_target_field_melee_ring(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    /* Use a radius that is already a bucket midpoint so the snap is a
     * no-op and the probe cell below lands squarely inside the ring. */
    /* Ring = [outerR - NAV_CELL_SIZE, outerR] = [64, 96] for outerR=96.
     * A cell whose center lies in that band is a seed. */
    NavTargetGoal goal = {
        .kind = NAV_GOAL_KIND_MELEE_RING,
        .targetX = 540.0f,
        .targetY = 540.0f,
        .outerRadius = 96.0f,
        .targetId = 7,
        .perspectiveSide = 0,
    };
    const NavField *field = nav_get_or_build_target_field(&nav, &bf, &goal);
    assert(field != NULL);
    /* Cell at (620, 540) -> col 19, row 16, center (624, 528),
     * distance ~85 from (540, 540) -- inside [64, 96]. */
    int32_t ringCell = nav_cell_index_for_world(620.0f, 540.0f);
    assert(field->distance[ringCell] == 0);
    int32_t offRing = nav_cell_index_for_world(540.0f + 200.0f, 540.0f);
    assert(field->distance[offRing] != NAV_DIST_UNREACHABLE);
    assert(field->distance[offRing] > 0);
    printf("  [ok] target field melee ring\n");
}

static void test_target_field_direct_range(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    NavTargetGoal goal = {
        .kind = NAV_GOAL_KIND_DIRECT_RANGE,
        .targetX = 540.0f,
        .targetY = 540.0f,
        .outerRadius = 128.0f,
        .targetId = 13,
        .perspectiveSide = 0,
    };
    const NavField *field = nav_get_or_build_target_field(&nav, &bf, &goal);
    assert(field != NULL);
    /* Every unblocked cell inside the disk is a seed (distance 0). */
    int32_t centerCell = nav_cell_index_for_world(540.0f, 540.0f);
    assert(field->distance[centerCell] == 0);
    int32_t insideCell = nav_cell_index_for_world(540.0f + 96.0f, 540.0f);
    assert(field->distance[insideCell] == 0);
    /* Outside the disk: not a seed, but reachable. */
    int32_t outsideCell = nav_cell_index_for_world(540.0f + 200.0f, 540.0f);
    assert(field->distance[outsideCell] != 0);
    assert(field->distance[outsideCell] != NAV_DIST_UNREACHABLE);
    printf("  [ok] target field direct-range disk\n");
}

static void test_free_goal_field_and_caching(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    NavFreeGoalRequest request = make_free_goal_request(300.0f, 600.0f, 48.0f, 0);
    const NavField *first = nav_get_or_build_free_goal_field(&nav, &bf, &request);
    assert(first != NULL);
    int32_t goalCell = nav_cell_index_for_world(300.0f, 600.0f);
    assert(first->distance[goalCell] == 0);

    /* Same goal + radius bucket -> cache hit. */
    const NavField *again = nav_get_or_build_free_goal_field(&nav, &bf, &request);
    assert(again == first);

    /* Different side -> new slot. */
    NavFreeGoalRequest otherRequest = make_free_goal_request(300.0f, 600.0f, 48.0f, 1);
    const NavField *other = nav_get_or_build_free_goal_field(&nav, &bf, &otherRequest);
    assert(other != NULL);
    assert(other != first);

    /* Frame reset drops the cache. */
    nav_begin_frame(&nav, &bf);
    assert(nav.freeGoalCacheSize == 0);
    printf("  [ok] free-goal field cache by (goalCell, radius, side)\n");
}

static void test_free_goal_carve_opens_target_owned_outer_band_only(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    const int targetId = 77;
    nav_snapshot_entity_position(&nav, targetId, 540.0f, 540.0f);
    nav_stamp_static_entity(&nav, targetId, 540.0f, 540.0f, 80.0f);

    NavFreeGoalRequest blockedReq = make_free_goal_request(592.0f, 592.0f, 10.0f, 0);
    const NavField *blockedField = nav_get_or_build_free_goal_field(&nav, &bf, &blockedReq);
    assert(blockedField != NULL);

    int32_t goalCell = nav_cell_index_for_world(blockedReq.goalX, blockedReq.goalY);
    assert(blockedField->hardBlocked[goalCell]);
    assert(blockedField->distance[goalCell] == NAV_DIST_UNREACHABLE);

    NavFreeGoalRequest carvedReq = blockedReq;
    carvedReq.carveTargetId = targetId;
    carvedReq.carveCenterX = 500.0f; /* deliberately stale; snapshot must win */
    carvedReq.carveCenterY = 500.0f;
    carvedReq.carveInnerRadius = BASE_NAV_RADIUS;

    const NavField *carvedField = nav_get_or_build_free_goal_field(&nav, &bf, &carvedReq);
    assert(carvedField != NULL);
    assert(carvedField != blockedField);
    assert(!carvedField->hardBlocked[goalCell]);
    assert(carvedField->distance[goalCell] == 0);

    int32_t innerCell = nav_cell_index_for_world(560.0f, 560.0f);
    assert(carvedField->hardBlocked[innerCell]);
    assert(carvedField->distance[innerCell] == NAV_DIST_UNREACHABLE);
    printf("  [ok] free-goal carve opens owned outer band without clearing core\n");
}

static void test_stamp_static_entity_cell_marks_owned_target(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    const int ownerId = 91;
    nav_stamp_static_entity_cell(&nav, ownerId, 604.0f, 540.0f);

    int32_t rightCell = nav_cell_index_for_world(604.0f, 540.0f);
    assert(nav.staticBlockers.blocked[rightCell]);
    assert(nav.staticBlockers.blockerSrc[rightCell] == ownerId);

    int32_t centerCell = nav_cell_index_for_world(540.0f, 540.0f);
    assert(!nav.staticBlockers.blocked[centerCell]);
    assert(nav.staticBlockers.blockerSrc[centerCell] == NAV_BLOCKER_SRC_NONE);
    printf("  [ok] stamp_static_entity_cell marks one owned blocker cell\n");
}

static void test_density_cost_shapes_integration(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);

    /* Build one lane field with no density, measure a reference distance. */
    nav_begin_frame(&nav, &bf);
    const NavField *baseField = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 1);
    int32_t sampleCell = nav_cell_index_for_world(540.0f, 960.0f);
    int32_t baseDist = baseField->distance[sampleCell];
    assert(baseDist != NAV_DIST_UNREACHABLE);

    /* Rebuild the frame and stamp a wall of enemy density across the lane
     * between the sample cell and the seed. The distance from the sample
     * cell to the seed must grow because the kernel now pays enemy-cost
     * on every step through the wall. */
    nav_begin_frame(&nav, &bf);
    for (float y = 500.0f; y <= 550.0f; y += (float)NAV_CELL_SIZE) {
        for (float x = 540.0f - 64.0f; x <= 540.0f + 64.0f;
             x += (float)NAV_CELL_SIZE) {
            nav_stamp_density(&nav, 1 /* enemy side */, x, y);
        }
    }
    const NavField *slowField = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 1);
    int32_t slowDist = slowField->distance[sampleCell];
    assert(slowDist != NAV_DIST_UNREACHABLE);
    /* Must be strictly slower -- at least one enemy occupancy of +18. */
    assert(slowDist > baseDist);
    printf("  [ok] density cost shapes integrated distances\n");
}

static void test_sample_flow_is_normalized(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);
    const NavField *field = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 1);
    assert(field != NULL);

    /* Sample several interior points; each result must have unit magnitude
     * or be exactly zero (no valid flow neighborhood). */
    const float xs[4] = { 540.0f, 560.0f, 530.0f, 580.0f };
    const float ys[4] = { 960.0f, 800.0f, 1200.0f, 480.0f };
    for (int i = 0; i < 4; ++i) {
        float fdx, fdy;
        nav_sample_flow(field, xs[i], ys[i], &fdx, &fdy);
        float mag = sqrtf(fdx * fdx + fdy * fdy);
        assert(mag < 1e-4f || fabsf(mag - 1.0f) < 1e-4f);
    }
    printf("  [ok] nav_sample_flow returns unit magnitude or zero\n");
}

static void test_target_field_blocked_arc_falls_back(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    /* Smother the entire arc radius with static blockers: one big disk
     * covering the target + the intended ring. */
    nav_stamp_static_blocker_disk(&nav, 540.0f, 540.0f, 160.0f);

    NavTargetGoal goal = {
        .kind = NAV_GOAL_KIND_STATIC_ATTACK,
        .targetX = 540.0f,
        .targetY = 540.0f,
        .outerRadius = 80.0f,
        .arcCenterDeg = 90.0f,
        .arcHalfDeg = 80.0f,
        .targetId = 101,
        .perspectiveSide = 0,
    };
    const NavField *field = nav_get_or_build_target_field(&nav, &bf, &goal);
    assert(field != NULL);
    assert(field->built);
    int32_t outsideCell = nav_cell_index_for_world(540.0f + 200.0f, 540.0f);
    assert(field->distance[outsideCell] != NAV_DIST_UNREACHABLE);
    printf("  [ok] target field blocked-arc falls back to widened seed\n");
}

static void test_target_field_blocked_ring_falls_back(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    nav_stamp_static_blocker_disk(&nav, 540.0f, 540.0f, 160.0f);

    NavTargetGoal goal = {
        .kind = NAV_GOAL_KIND_MELEE_RING,
        .targetX = 540.0f,
        .targetY = 540.0f,
        .outerRadius = 48.0f,
        .targetId = 102,
        .perspectiveSide = 0,
    };
    const NavField *field = nav_get_or_build_target_field(&nav, &bf, &goal);
    assert(field != NULL);
    int32_t outsideCell = nav_cell_index_for_world(540.0f + 200.0f, 540.0f);
    assert(field->distance[outsideCell] != NAV_DIST_UNREACHABLE);
    printf("  [ok] target field blocked-ring falls back to widened seed\n");
}

static void test_target_cache_exact_radius_key(void) {
    /* Two builds with the same exact outerRadius share a slot; a build
     * with a different outerRadius goes into a new slot. No bucket
     * snapping: the field is seeded at the exact caller radius so it
     * always agrees with combat_in_range. */
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    NavTargetGoal goalA = {
        .kind = NAV_GOAL_KIND_MELEE_RING,
        .targetX = 540.0f,
        .targetY = 540.0f,
        .outerRadius = 70.0f,
        .targetId = 55,
        .perspectiveSide = 0,
    };
    NavTargetGoal goalB = goalA;  /* identical */

    const NavField *a = nav_get_or_build_target_field(&nav, &bf, &goalA);
    const NavField *b = nav_get_or_build_target_field(&nav, &bf, &goalB);
    assert(a != NULL && b != NULL);
    assert(a == b);
    /* Stored stopRadius must equal the exact caller value. */
    assert(fabsf(nav_goal_region_stop_radius(a) - 70.0f) < 1e-3f);

    /* Any radius different by more than 0.25 px lands in a new slot. */
    NavTargetGoal goalC = goalA;
    goalC.outerRadius = 79.0f;
    const NavField *c = nav_get_or_build_target_field(&nav, &bf, &goalC);
    assert(c != NULL);
    assert(c != a);
    assert(fabsf(nav_goal_region_stop_radius(c) - 79.0f) < 1e-3f);
    printf("  [ok] target cache keys on exact radius\n");
}

static void test_free_goal_cache_exact_radius_key(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    NavFreeGoalRequest req44 = make_free_goal_request(300.0f, 600.0f, 44.0f, 0);
    const NavField *a = nav_get_or_build_free_goal_field(&nav, &bf, &req44);
    const NavField *b = nav_get_or_build_free_goal_field(&nav, &bf, &req44);
    assert(a != NULL && b != NULL);
    assert(a == b);
    assert(fabsf(nav_goal_region_stop_radius(a) - 44.0f) < 1e-3f);
    NavFreeGoalRequest req80 = make_free_goal_request(300.0f, 600.0f, 80.0f, 0);
    const NavField *c = nav_get_or_build_free_goal_field(&nav, &bf, &req80);
    assert(c != NULL);
    assert(c != a);
    assert(fabsf(nav_goal_region_stop_radius(c) - 80.0f) < 1e-3f);
    printf("  [ok] free-goal cache keys on exact radius\n");
}

static void test_target_field_uses_position_snapshot(void) {
    /* Snapshot a target at an authoritative position. Call the target
     * builder with a stale (wrong) position in the goal struct and
     * assert the field was seeded against the snapshot, not the stale
     * goal coordinates. */
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    const int targetId = 17;
    nav_snapshot_entity_position(&nav, targetId, 700.0f, 700.0f);

    NavTargetGoal goal = {
        .kind = NAV_GOAL_KIND_MELEE_RING,
        .targetX = 100.0f,    /* deliberately wrong */
        .targetY = 100.0f,    /* deliberately wrong */
        .outerRadius = 64.0f,
        .targetId = targetId,
        .perspectiveSide = 0,
    };
    const NavField *field = nav_get_or_build_target_field(&nav, &bf, &goal);
    assert(field != NULL);
    /* Field anchor must match the snapshot, not the stale goal coords. */
    float ax, ay;
    nav_goal_region_anchor(field, &ax, &ay);
    assert(fabsf(ax - 700.0f) < 1e-3f);
    assert(fabsf(ay - 700.0f) < 1e-3f);

    /* A ring cell around the SNAPSHOT position must be a seed. Ring at
     * radius 64 with thickness 16 -> seeds in [48, 80]. A cell 64 px east
     * of (700, 700) at col=23 row=21 has center (23*32+16, 21*32+16) =
     * (752, 688), distance ~55 from (700, 700) -- in-ring. */
    int32_t ringCell = nav_index(23, 21);
    assert(field->distance[ringCell] == 0);

    /* And a cell near the stale goal (100, 100) should NOT be a seed. */
    int32_t stale = nav_cell_index_for_world(100.0f + 64.0f, 100.0f);
    assert(field->distance[stale] != 0);
    printf("  [ok] target field reads from frame position snapshot\n");
}

static void test_target_field_snapshot_survives_large_entity_ids(void) {
    /* Long matches produce entity ids well beyond MAX_ENTITIES*2 because
     * s_nextEntityID never resets. The hash-based snapshot must accept
     * and retrieve such ids without aliasing or dropping them. */
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    const int bigId = 9999;  /* far past MAX_ENTITIES*2 = 128 */
    nav_snapshot_entity_position(&nav, bigId, 612.0f, 900.0f);

    NavTargetGoal goal = {
        .kind = NAV_GOAL_KIND_MELEE_RING,
        .targetX = 0.0f,    /* stale */
        .targetY = 0.0f,
        .outerRadius = 48.0f,
        .targetId = bigId,
        .perspectiveSide = 0,
    };
    const NavField *field = nav_get_or_build_target_field(&nav, &bf, &goal);
    assert(field != NULL);
    float ax, ay;
    nav_goal_region_anchor(field, &ax, &ay);
    /* Anchor must reflect the snapshot at the big-id slot, not the
     * stale (0, 0) in the goal struct. */
    assert(fabsf(ax - 612.0f) < 1e-3f);
    assert(fabsf(ay - 900.0f) < 1e-3f);
    printf("  [ok] snapshot survives entity ids past the live cap\n");
}

static void test_free_goal_cache_distinguishes_intra_cell_goals(void) {
    /* Two free-goal points inside the same 32 px cell but further apart
     * than 0.25 px must produce distinct cached fields with distinct
     * seed sets. */
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    /* (512, 512) and (528, 528) both map to different cells for col/row
     * math but let me pick two points inside one cell instead: col 16
     * spans x in [512, 544). (520, 530) and (540, 538) both sit inside
     * col 16 row 16. */
    NavFreeGoalRequest reqA = make_free_goal_request(520.0f, 530.0f, 16.0f, 0);
    NavFreeGoalRequest reqB = make_free_goal_request(540.0f, 538.0f, 16.0f, 0);
    const NavField *a = nav_get_or_build_free_goal_field(&nav, &bf, &reqA);
    const NavField *b = nav_get_or_build_free_goal_field(&nav, &bf, &reqB);
    assert(a != NULL && b != NULL);
    assert(a != b);

    /* Stored anchors must match the exact caller positions. */
    float ax, ay, bx, by;
    nav_goal_region_anchor(a, &ax, &ay);
    nav_goal_region_anchor(b, &bx, &by);
    assert(fabsf(ax - 520.0f) < 1e-3f);
    assert(fabsf(ay - 530.0f) < 1e-3f);
    assert(fabsf(bx - 540.0f) < 1e-3f);
    assert(fabsf(by - 538.0f) < 1e-3f);
    printf("  [ok] free-goal cache separates intra-cell goal points\n");
}

static void test_target_field_snapshot_determinism_across_callers(void) {
    /* Two callers pursuing the same target from different vantage points
     * pass different stale targetX/Y, but both must receive the exact
     * same cached field (identity comparison). Without the snapshot this
     * used to depend on who called first. */
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    const int targetId = 21;
    nav_snapshot_entity_position(&nav, targetId, 540.0f, 540.0f);

    NavTargetGoal g1 = {
        .kind = NAV_GOAL_KIND_MELEE_RING,
        .targetX = 530.0f,
        .targetY = 538.0f,
        .outerRadius = 64.0f,
        .targetId = targetId,
        .perspectiveSide = 0,
    };
    NavTargetGoal g2 = g1;
    g2.targetX = 549.0f;
    g2.targetY = 541.0f;

    const NavField *f1 = nav_get_or_build_target_field(&nav, &bf, &g1);
    const NavField *f2 = nav_get_or_build_target_field(&nav, &bf, &g2);
    assert(f1 == f2);
    printf("  [ok] target field identical across callers with stale positions\n");
}

static void test_goal_region_anchor_exposes_pivot(void) {
    static NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    const NavField *lane = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 1);
    float lx, ly;
    nav_goal_region_anchor(lane, &lx, &ly);
    assert(fabsf(lx - 540.0f) < 1e-3f);
    assert(fabsf(ly - 120.0f) < 1e-3f);

    NavTargetGoal goal = {
        .kind = NAV_GOAL_KIND_MELEE_RING,
        .targetX = 700.0f,
        .targetY = 800.0f,
        .outerRadius = 48.0f,
        .targetId = 9,
        .perspectiveSide = 0,
    };
    const NavField *target = nav_get_or_build_target_field(&nav, &bf, &goal);
    float tx, ty;
    nav_goal_region_anchor(target, &tx, &ty);
    assert(fabsf(tx - 700.0f) < 1e-3f);
    assert(fabsf(ty - 800.0f) < 1e-3f);

    NavFreeGoalRequest freeReq = make_free_goal_request(400.0f, 1200.0f, 32.0f, 1);
    const NavField *freef = nav_get_or_build_free_goal_field(&nav, &bf, &freeReq);
    float fx, fy;
    nav_goal_region_anchor(freef, &fx, &fy);
    assert(fabsf(fx - 400.0f) < 1e-3f);
    assert(fabsf(fy - 1200.0f) < 1e-3f);
    printf("  [ok] nav_goal_region_anchor returns correct pivot\n");
}

static void test_lane_field_on_bowed_polyline(void) {
    NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    memset(&bf, 0, sizeof(bf));
    bf.boardWidth = BOARD_WIDTH;
    bf.boardHeight = BOARD_HEIGHT;
    bf.seamY = SEAM_Y;
    /* Straight lanes 0 and 2; lane 1 bows out to x=700 at the midpoint. */
    lane_waypoints_linear(&bf, SIDE_BOTTOM, 0, 180.0f, BOARD_HEIGHT - 120.0f, 120.0f);
    lane_waypoints_bow   (&bf, SIDE_BOTTOM, 1,
                          BOARD_HEIGHT - 120.0f, 120.0f,
                          540.0f, 160.0f);
    lane_waypoints_linear(&bf, SIDE_BOTTOM, 2, 900.0f, BOARD_HEIGHT - 120.0f, 120.0f);

    nav_begin_frame(&nav, &bf);
    const NavField *field = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 1);
    assert(field != NULL);
    assert(field->built);

    /* A cell that tracks the bow (x ~= 700, home-half y) must be inside the
     * corridor; a cell that hugs the straight center (x = 540) at the same
     * y may fall outside the 96 px envelope of the bowed polyline. */
    int32_t onBow = nav_cell_index_for_world(700.0f, 1440.0f);
    assert(!field->hardBlocked[onBow]);
    assert(field->distance[onBow] != NAV_DIST_UNREACHABLE);

    /* Symmetric probe on the opposite side of the bow must be hard-blocked
     * because it leaves the home-half corridor. */
    int32_t offBow = nav_cell_index_for_world(380.0f, 1440.0f);
    assert(field->hardBlocked[offBow]);

    /* The seed (final waypoint) is at (540, 120); that cell must have
     * distance 0 and be reachable from the bow. */
    int32_t seedCell = nav_cell_index_for_world(540.0f, 120.0f);
    assert(field->distance[seedCell] == 0);
    assert(field->distance[onBow] > field->distance[seedCell]);
    printf("  [ok] lane field on bowed polyline\n");
}

static void test_find_accessors_return_cached_fields_only(void) {
    NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    assert(nav_find_lane_field(&nav, SIDE_BOTTOM, 1) == NULL);
    assert(nav_find_target_field(&nav, &(NavTargetGoal){
        .kind = NAV_GOAL_KIND_MELEE_RING,
        .targetId = 88,
        .outerRadius = 48.0f,
        .perspectiveSide = 0
    }) == NULL);
    NavFreeGoalRequest req48 = make_free_goal_request(300.0f, 600.0f, 48.0f, 0);
    assert(nav_find_free_goal_field(&nav, &req48) == NULL);

    const NavField *lane = nav_get_or_build_lane_field(&nav, &bf, SIDE_BOTTOM, 1);
    assert(nav_find_lane_field(&nav, SIDE_BOTTOM, 1) == lane);

    NavTargetGoal targetGoal = {
        .kind = NAV_GOAL_KIND_MELEE_RING,
        .targetX = 700.0f,
        .targetY = 800.0f,
        .outerRadius = 48.0f,
        .targetBodyRadius = 14.0f,
        .innerRadiusMin = 30.0f,
        .targetId = 88,
        .perspectiveSide = 0,
    };
    const NavField *target = nav_get_or_build_target_field(&nav, &bf, &targetGoal);
    assert(nav_find_target_field(&nav, &targetGoal) == target);

    const NavField *freef = nav_get_or_build_free_goal_field(&nav, &bf, &req48);
    assert(nav_find_free_goal_field(&nav, &req48) == freef);
    printf("  [ok] nav_find_* accessors match built caches\n");
}

static void test_goal_region_geometry_metadata_matches_built_fields(void) {
    NavFrame nav;
    nav_frame_init(&nav);
    Battlefield bf;
    make_default_battlefield(&bf);
    nav_begin_frame(&nav, &bf);

    NavTargetGoal staticGoal = {
        .kind = NAV_GOAL_KIND_STATIC_ATTACK,
        .targetX = 540.0f,
        .targetY = 240.0f,
        .outerRadius = 86.0f,
        .arcCenterDeg = 90.0f,
        .arcHalfDeg = 80.0f,
        .targetBodyRadius = 28.0f,
        .innerRadiusMin = 44.0f,
        .targetId = 501,
        .perspectiveSide = 0,
    };
    nav_stamp_static_entity(&nav, staticGoal.targetId, staticGoal.targetX,
                            staticGoal.targetY, 56.0f);
    const NavField *staticField = nav_get_or_build_target_field(&nav, &bf, &staticGoal);
    assert(staticField != NULL);
    assert(fabsf(nav_goal_region_stop_radius(staticField) - 86.0f) < 1e-3f);
    assert(fabsf(nav_goal_region_inner_radius(staticField) - 44.0f) < 1e-3f);
    assert(fabsf(nav_goal_region_arc_center_deg(staticField) - 90.0f) < 1e-3f);
    assert(fabsf(nav_goal_region_arc_half_deg(staticField) - 80.0f) < 1e-3f);
    assert(fabsf(nav_goal_region_target_body_radius(staticField) - 28.0f) < 1e-3f);

    NavTargetGoal meleeGoal = {
        .kind = NAV_GOAL_KIND_MELEE_RING,
        .targetX = 700.0f,
        .targetY = 800.0f,
        .outerRadius = 48.0f,
        .targetBodyRadius = 14.0f,
        .innerRadiusMin = 30.0f,
        .targetId = 77,
        .perspectiveSide = 0,
    };
    const NavField *meleeField = nav_get_or_build_target_field(&nav, &bf, &meleeGoal);
    assert(meleeField != NULL);
    assert(fabsf(nav_goal_region_stop_radius(meleeField) - 48.0f) < 1e-3f);
    assert(fabsf(nav_goal_region_inner_radius(meleeField) - 30.0f) < 1e-3f);
    assert(fabsf(nav_goal_region_arc_half_deg(meleeField)) < 1e-3f);

    NavFreeGoalRequest freeReq = make_free_goal_request(400.0f, 1200.0f, 32.0f, 1);
    const NavField *freeField = nav_get_or_build_free_goal_field(&nav, &bf, &freeReq);
    assert(freeField != NULL);
    assert(fabsf(nav_goal_region_stop_radius(freeField) - 32.0f) < 1e-3f);
    assert(fabsf(nav_goal_region_inner_radius(freeField)) < 1e-3f);
    assert(fabsf(nav_goal_region_arc_center_deg(freeField)) < 1e-3f);
    printf("  [ok] nav goal-region metadata matches built geometry\n");
}

int main(void) {
    printf("test_nav_frame\n");
    test_grid_constants();
    test_coord_helpers();
    test_begin_frame_clears_state();
    test_lane_field_monotone_toward_seed();
    test_lane_field_home_corridor_masks_out_far_cells();
    test_lane_field_cached_across_lookups();
    test_lane_field_top_side_symmetry();
    test_lane_field_requires_begin_frame();
    test_heap_preserves_order_under_mixed_distances();
    test_heap_capacity_bounds_stress();
    test_static_blockers_seal_outer_ring();
    test_integration_refuses_corner_cut();
    test_seed_in_blocker_preserves_mask();
    test_stamp_static_blocker_disk();
    test_stamp_density_counts_per_side();
    test_cell_flow_direction_points_downhill();
    test_sample_flow_bilinear_between_cells();
    test_sample_flow_at_blocker_is_zero();
    test_sample_flow_is_normalized();
    test_target_field_static_arc();
    test_target_field_melee_ring();
    test_target_field_direct_range();
    test_free_goal_field_and_caching();
    test_free_goal_carve_opens_target_owned_outer_band_only();
    test_stamp_static_entity_cell_marks_owned_target();
    test_density_cost_shapes_integration();
    test_target_field_blocked_arc_falls_back();
    test_target_field_blocked_ring_falls_back();
    test_target_cache_exact_radius_key();
    test_free_goal_cache_exact_radius_key();
    test_target_field_uses_position_snapshot();
    test_target_field_snapshot_survives_large_entity_ids();
    test_free_goal_cache_distinguishes_intra_cell_goals();
    test_target_field_snapshot_determinism_across_callers();
    test_goal_region_anchor_exposes_pivot();
    test_lane_field_on_bowed_polyline();
    test_find_accessors_return_cached_fields_only();
    test_goal_region_geometry_metadata_matches_built_fields();
    printf("all nav_frame tests passed\n");
    return 0;
}
