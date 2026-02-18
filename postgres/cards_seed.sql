-- Card seed data
-- Insert all base cards. Run after schema is in place.
-- Safe to re-run: uses ON CONFLICT DO UPDATE.

INSERT INTO cards (card_id, name, cost, type, rules_text, data) VALUES

-- ─────────────────────────────────────────────
-- KNIGHT
-- Heavy armored melee troop. Slow but durable.
-- Targets nearest enemy.
-- ─────────────────────────────────────────────
(
  'knight_01',
  'Knight',
  4,
  'knight',
  'A heavily armored warrior that charges toward the nearest enemy.',
  '{
    "visual": {
      "border_color":       "gray",
      "show_border":        true,
      "bg_style":           "paper",
      "show_bg":            true,
      "banner_color":       "blue",
      "show_banner":        true,
      "corner_color":       "yellow",
      "show_corner":        true,
      "container_color":    "gray",
      "container_variant":  1,
      "show_container":     false,
      "description_style":  "paper",
      "show_description":   true,
      "innercorner_style":  "yellow",
      "show_innercorner":   true,
      "gem_color":          "blue",
      "show_gem":           true,
      "socket_color":       "gray",
      "show_socket":        true,
      "energy_top_color":   "blue",
      "show_energy_top":    false,
      "energy_bot_color":   "blue",
      "show_energy_bot":    false
    },
    "hp":          220,
    "maxHP":       220,
    "attack":      28,
    "attackSpeed": 0.75,
    "attackRange": 50.0,
    "moveSpeed":   38.0,
    "targeting":   "nearest",
    "targetType":  null
  }'
),

-- ─────────────────────────────────────────────
-- ASSASSIN
-- Fast, fragile, high single-hit damage.
-- Bypasses tanks to reach the backline.
-- ─────────────────────────────────────────────
(
  'assassin_01',
  'Assassin',
  3,
  'assassin',
  'A swift killer that strikes hard and fades before the enemy can react.',
  '{
    "visual": {
      "border_color":       "purple",
      "show_border":        true,
      "bg_style":           "black",
      "show_bg":            true,
      "banner_color":       "purple",
      "show_banner":        true,
      "corner_color":       "black",
      "show_corner":        true,
      "container_color":    "purple",
      "container_variant":  2,
      "show_container":     false,
      "description_style":  "black",
      "show_description":   true,
      "innercorner_style":  "black",
      "show_innercorner":   true,
      "gem_color":          "magenta",
      "show_gem":           true,
      "socket_color":       "purple",
      "show_socket":        true,
      "energy_top_color":   "magenta",
      "show_energy_top":    false,
      "energy_bot_color":   "magenta",
      "show_energy_bot":    false
    },
    "hp":          75,
    "maxHP":       75,
    "attack":      42,
    "attackSpeed": 1.8,
    "attackRange": 32.0,
    "moveSpeed":   100.0,
    "targeting":   "nearest",
    "targetType":  null
  }'
),

-- ─────────────────────────────────────────────
-- BRUTE
-- Massive, slow, devastating against structures.
-- Ignores troops — walks straight for buildings.
-- ─────────────────────────────────────────────
(
  'brute_01',
  'Brute',
  6,
  'brute',
  'An unstoppable force of destruction. Nothing stands between it and your towers.',
  '{
    "visual": {
      "border_color":       "red",
      "show_border":        true,
      "bg_style":           "brown",
      "show_bg":            true,
      "banner_color":       "red",
      "show_banner":        true,
      "corner_color":       "brown",
      "show_corner":        true,
      "container_color":    "red",
      "container_variant":  3,
      "show_container":     false,
      "description_style":  "brown",
      "show_description":   true,
      "innercorner_style":  "brown",
      "show_innercorner":   true,
      "gem_color":          "red",
      "show_gem":           true,
      "socket_color":       "brown",
      "show_socket":        true,
      "energy_top_color":   "red",
      "show_energy_top":    true,
      "energy_bot_color":   "red",
      "show_energy_bot":    true
    },
    "hp":          380,
    "maxHP":       380,
    "attack":      55,
    "attackSpeed": 0.5,
    "attackRange": 48.0,
    "moveSpeed":   28.0,
    "targeting":   "building",
    "targetType":  null
  }'
),

-- ─────────────────────────────────────────────
-- FARMER
-- Cheap, expendable melee troop.
-- Weak in a fight but useful for soaking damage
-- or distracting enemies.
-- ─────────────────────────────────────────────
(
  'farmer_01',
  'Farmer',
  2,
  'farmer',
  'A determined peasant armed with little more than a pitchfork and stubbornness.',
  '{
    "visual": {
      "border_color":       "green",
      "show_border":        true,
      "bg_style":           "paper",
      "show_bg":            true,
      "banner_color":       "brown",
      "show_banner":        true,
      "corner_color":       "green",
      "show_corner":        true,
      "container_color":    "brown",
      "container_variant":  1,
      "show_container":     false,
      "description_style":  "paper",
      "show_description":   true,
      "innercorner_style":  "brown",
      "show_innercorner":   true,
      "gem_color":          "green",
      "show_gem":           true,
      "socket_color":       "brown",
      "show_socket":        true,
      "energy_top_color":   "green",
      "show_energy_top":    false,
      "energy_bot_color":   "green",
      "show_energy_bot":    false
    },
    "hp":          90,
    "maxHP":       90,
    "attack":      8,
    "attackSpeed": 1.1,
    "attackRange": 38.0,
    "moveSpeed":   58.0,
    "targeting":   "nearest",
    "targetType":  null
  }'
),

-- ─────────────────────────────────────────────
-- HEALER
-- Slow support troop with long attack range.
-- Low damage — value comes from sustaining allies.
-- ─────────────────────────────────────────────
(
  'healer_01',
  'Healer',
  3,
  'healer',
  'A battlefield medic that keeps allies fighting long after they should have fallen.',
  '{
    "visual": {
      "border_color":       "white",
      "show_border":        true,
      "bg_style":           "paper",
      "show_bg":            true,
      "banner_color":       "green",
      "show_banner":        true,
      "corner_color":       "white",
      "show_corner":        true,
      "container_color":    "white",
      "container_variant":  1,
      "show_container":     false,
      "description_style":  "paper",
      "show_description":   true,
      "innercorner_style":  "white",
      "show_innercorner":   true,
      "gem_color":          "green",
      "show_gem":           true,
      "socket_color":       "white",
      "show_socket":        true,
      "energy_top_color":   "green",
      "show_energy_top":    false,
      "energy_bot_color":   "green",
      "show_energy_bot":    false
    },
    "hp":          120,
    "maxHP":       120,
    "attack":      5,
    "attackSpeed": 0.5,
    "attackRange": 85.0,
    "moveSpeed":   44.0,
    "targeting":   "nearest",
    "targetType":  null
  }'
),

-- ─────────────────────────────────────────────
-- FIREBALL
-- Instant spell. Deals heavy fire damage to
-- troops and buildings in the target area.
-- ─────────────────────────────────────────────
(
  'fireball_01',
  'Fireball',
  4,
  'spell',
  'Calls down a blazing fireball that scorches everything it touches.',
  '{
    "visual": {
      "border_color":       "red",
      "show_border":        true,
      "bg_style":           "black",
      "show_bg":            true,
      "banner_color":       "red",
      "show_banner":        true,
      "corner_color":       "yellow",
      "show_corner":        true,
      "container_color":    "red",
      "container_variant":  2,
      "show_container":     false,
      "description_style":  "black",
      "show_description":   true,
      "innercorner_style":  "yellow",
      "show_innercorner":   true,
      "gem_color":          "red",
      "show_gem":           true,
      "socket_color":       "yellow",
      "show_socket":        true,
      "energy_top_color":   "red",
      "show_energy_top":    true,
      "energy_bot_color":   "yellow",
      "show_energy_bot":    false
    },
    "damage":  120,
    "element": "fire",
    "targets": ["troops", "buildings"]
  }'
)

ON CONFLICT (card_id) DO UPDATE SET
  name       = EXCLUDED.name,
  cost       = EXCLUDED.cost,
  type       = EXCLUDED.type,
  rules_text = EXCLUDED.rules_text,
  data       = EXCLUDED.data;
