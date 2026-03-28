---
phase: 1
reviewers: [gemini, codex]
reviewed_at: 2026-03-28
plans_reviewed: [01-00-PLAN.md, 01-01-PLAN.md, 01-02-PLAN.md]
---

# Cross-AI Plan Review — Phase 1

## Gemini Review

This review evaluates the three-wave implementation plan for **Phase 1: Lane Pathfinding** in the NFC Card Game project.

### 1. Summary
The implementation plan is exceptionally well-structured, prioritizing testability and visual verification. By decoupling the pathfinding logic from the Raylib engine in Wave 0, the plan ensures that the core mathematical "bow" logic is sound before any sprites are rendered. The use of a sine-based curve for lane geometry is an elegant, low-overhead solution that fits the "parenthesis" shape requirement. The progression from data structures to movement logic, concluding with a debug overlay, follows a logical "bottom-up" development pattern that minimizes integration risk.

### 2. Strengths
* **Test-Driven Foundation:** Creating a standalone test suite (`test_pathfinding.c`) that ignores heavy dependencies (Raylib/Postgres) allows for rapid iteration on the movement algorithms.
* **Visual Debugging:** Including a dedicated debug overlay (F1 toggle) for lane paths is critical for a project where "feel" and "shape" are key requirements.
* **Symmetry by Design:** The use of `bow_offset()` and shared waypoint generation ensures that the three lanes remain perfectly symmetric, which is vital for competitive balance.
* **State Management:** Transitioning to `ESTATE_IDLE` with a random jitter at the destination provides a polished "end-of-path" behavior that prevents troops from simply vanishing or stacking perfectly on a single pixel.

### 3. Concerns
* **Opponent Directionality (MEDIUM):** The plan mentions symmetry but does not explicitly detail how waypoints are generated for Player 2 (who sits across the board). If waypoints are generated in "world space," Player 2's troops must move in the negative Y direction. The waypoint generation logic needs to account for the `playerID` or starting Y-coordinate to ensure troops march *toward* each other rather than off the same side of the screen.
* **Waypoint "Overshooting" (MEDIUM):** In `entity_update()`, if a troop moves at a constant speed, it might "skip" over the exact coordinates of a waypoint in a single frame. The logic should check if the distance to the waypoint is less than the movement step to prevent "orbiting" or jittery backtracking.
* **Struct Desynchronization (LOW):** Wave 0 redefines "minimal struct stubs." While efficient for testing, there is a risk that if `types.h` changes, the tests will pass against outdated mocks while the real game crashes.
* **Sprite Flipping Hysteresis (LOW):** "Dominant axis" sprite direction switching can cause rapid "flickering" between `DIR_SIDE` and `DIR_UP` if the path is exactly 45 degrees or if floating-point errors occur.

### 4. Suggestions
* **Directional Inversion:** In `lane_generate_waypoints()`, multiply the Y-increment by a `direction` variable (1 for Player 1, -1 for Player 2) based on the player's side of the board.
* **Arrival Threshold:** Use a `WAYPOINT_THRESHOLD` (e.g., 2.0-5.0 pixels) in the movement logic. If `Distance(entity, target) < threshold`, immediately snap to the waypoint and increment `waypointIndex`.
* **Frame-Rate Independence:** Ensure the movement logic in Wave 2 utilizes `GetFrameTime()` (delta time) to ensure troops move at the same speed regardless of the hardware's refresh rate.
* **Coordinate Space Clarification:** Ensure `LANE_BOW_INTENSITY` is defined clearly (e.g., as a percentage of screen width or a fixed pixel value) to avoid scaling issues on different resolutions.

### 5. Risk Assessment: LOW
The plan is low-risk because it relies on simple trigonometry (sine waves) and linear interpolation, both of which are computationally inexpensive and easy to debug. The inclusion of a standalone test runner and a visual debug path overlay provides two layers of validation (mathematical and visual) before the phase is considered complete.

---

## Codex Review

### Plan 01-00

**Summary**
Good instinct to add tests before implementation, and the plan is trying to keep them lightweight and CI-friendly. The main problem is that the proposed tests are pointed at `pathfinding.c`, while the actual movement behavior this phase cares about still lives in `src/entities/entities.c`, so several tests would only validate a hand-built model rather than production behavior.

**Strengths**
- Keeps test runtime cheap by avoiding Raylib windowing and DB setup.
- Covers the core geometry decisions: center lane straight, outer lane bow, endpoint idle, sprite facing.
- Calls out the spawn-snap issue explicitly and tests for it.
- Adds a `make test` path, which the repo currently lacks.

**Concerns**
- HIGH: The tests include `pathfinding.c` directly, but the phase's actual waypoint stepping and facing logic is planned for `src/entities/entities.c`. That creates false confidence.
- HIGH: The "movement step" and "idle at last waypoint" tests are described as manual simulations, not calls into production code. Those can pass while the game still behaves incorrectly.
- MEDIUM: Predefining header guards and re-stubbing project structs is brittle. Any new include or type use in `pathfinding.c` can silently break the test harness.
- MEDIUM: The `-lm` only constraint is tighter than necessary and may fight the codebase instead of testing it.
- LOW: Edge cases are missing: invalid lane, zero-distance waypoint, and waypoint index already out of range.

**Suggestions**
- Either move the stepping/direction logic into a pure helper in `src/logic/pathfinding.c`, or narrow Wave 0 to geometry-only tests and add real movement tests later against production helpers.
- Add tests for invalid lane/index handling and for the spawn-at-waypoint-zero case.
- Prefer compiling a small production unit with explicit seams over direct `.c` inclusion plus duplicated structs.

**Risk Assessment: MEDIUM**

### Plan 01-01

**Summary**
This is the strongest of the three plans. Precomputing per-player lane waypoints in init is the right level of complexity for this phase, and reusing `player_lane_pos()` matches the existing codebase well. The main gap is specification precision: the plan needs to define the exact depth schedule and bow normalization more clearly so waypoint 0 matches spawn without distorting the intended `( | )` taper.

**Strengths**
- Correctly keeps waypoint generation out of the frame loop.
- Uses existing lane geometry from `src/systems/player.c`.
- Stores lane data on `Player`, which is the right ownership model.
- Adds tuning knobs in `src/core/config.h` instead of hardcoding values.
- Integrates after card slot init, which is the correct dependency order.

**Concerns**
- HIGH: Cross-wave mismatch remains: Wave 0 tests focus on `pathfinding.c`, but Wave 2 movement stays in `src/entities/entities.c`.
- MEDIUM: "Waypoint[0] equals spawn" and "sine taper to zero at the ends" conflict unless the depth mapping is explicitly normalized or waypoint 0 is special-cased.
- MEDIUM: The plan does not mention null/defensive checks for `lane_generate_waypoints(Player *p)`.
- MEDIUM: It hardcodes `3` lanes conceptually instead of leaning on `NUM_CARD_SLOTS`.
- LOW: Visual symmetry across the rotated split-screen setup should be verified explicitly.

**Suggestions**
- Specify the exact depth rule: `waypoint[0] = slot`, remaining waypoints interpolate from spawn depth to final depth, with bow normalized over that range.
- Keep a small pure helper API in `src/logic/pathfinding.c` for later movement tests.
- Use `NUM_CARD_SLOTS` for loops and add a null guard for generator entry.
- Add one acceptance check that left/right lanes stay within reasonable lane bounds.

**Risk Assessment: MEDIUM**

### Plan 01-02

**Summary**
This plan delivers the visible gameplay change the phase needs, and the scope is still disciplined. The main risks are around edge handling and visual correctness: the current repo still forces crossed entities to `DIR_DOWN` in `src/core/game.c`, and the proposed movement code has a few cases where troops can freeze, pause at spawn, or show the wrong facing on waypoint transitions.

**Strengths**
- Replaces the current straight-line movement in the correct place.
- Removes the old off-screen despawn behavior, which matches the phase goal.
- Uses dominant-axis facing, which fits the existing 3-direction sprite system.
- Adds a debug overlay and explicit visual verification.
- Avoids over-engineering with splines or runtime curve evaluation.

**Concerns**
- HIGH: Invalid `lane` just `break`s, leaving the entity in `ESTATE_WALKING` but frozen forever.
- HIGH: Facing is computed from the pre-advance `diff`; after snapping to a waypoint, direction can lag or be wrong at bends.
- HIGH: `waypointIndex == 0` targeting `waypoint[0] == spawn` creates a zero-distance first target and can cause a visible one-frame pause.
- MEDIUM: Random jitter on both axes can push the final idle position backward/across the border.
- MEDIUM: The repo still overrides crossed entities to `DIR_DOWN` in `src/core/game.c`, which may conflict with lane-facing goals in the opponent viewport.
- LOW: Performance impact is negligible for current entity counts.

**Suggestions**
- On invalid lane/index, transition to `ESTATE_IDLE` or mark removal, and log once.
- Consume zero-distance waypoints immediately, or spawn with `waypointIndex = 1`.
- Recompute facing after waypoint advancement using the new target.
- Clamp endpoint jitter so it stays near the lane endpoint and out of the border-crossing seam.
- Decide explicitly whether direction correctness is required in both owner and opponent viewports.

**Risk Assessment: MEDIUM-HIGH**

### Cross-Plan Note

The biggest issue across all three plans is alignment: Wave 0 tests are designed around `src/logic/pathfinding.c`, while the actual runtime movement change is designed for `src/entities/entities.c`. If that is not corrected, the phase can finish with passing tests and still have broken on-screen behavior.

---

## Consensus Summary

### Agreed Strengths
- **Test-first approach** — both reviewers praise the standalone test infrastructure (Gemini: "exceptionally well-structured", Codex: "good instinct")
- **Visual debug overlay** — both agree F1 toggle is critical for tuning curve shape
- **Pre-computed waypoints at init** — both confirm this is the right complexity level
- **Sine bow curve** — both agree it's elegant and fits the requirements

### Agreed Concerns
1. **Player 2 directionality (MEDIUM)** — Gemini flags that P2 waypoint generation may need direction inversion. Codex flags the `DIR_DOWN` mirror override in game.c may conflict with lane-facing. Both point at the same gap: behavior correctness in the opponent's viewport.
2. **Test/production code alignment (HIGH — Codex)** — Codex raises that CORE-01d/01e tests simulate movement logic inline rather than testing production code in entities.c. The tests prove the math but don't verify the actual game behavior. Gemini didn't flag this but noted struct desync risk.
3. **Zero-distance first waypoint (MEDIUM-HIGH)** — Codex flags that spawning at waypoint[0] creates a zero-distance target causing a one-frame pause. The plan does handle `dist < 1.0f` but this edge case needs explicit attention.
4. **Sprite flicker at 45-degree angles (LOW)** — Gemini flags dominant-axis switching could flicker. Codex flags facing computed from pre-advance diff can lag at bends.
5. **Struct stub brittleness (MEDIUM)** — Both note the test stubs could desync from types.h over time.

### Divergent Views
- **Overall risk level**: Gemini rates LOW overall, Codex rates MEDIUM to MEDIUM-HIGH. Codex is more concerned about the test-to-production alignment gap and edge cases in movement logic.
- **Test scope**: Gemini sees the test approach as a clear strength. Codex sees it as partially misaligned since stepping logic lives in entities.c not pathfinding.c.

---

*Review completed: 2026-03-28*
*Reviewers: Gemini CLI, Codex CLI*
