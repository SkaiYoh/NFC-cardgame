# Balance Sheet

Runtime balance reference for the current rebalance pass.

Source of truth:
- `cardgame.db` for live card values
- `sqlite/seed.sql` for seed parity
- `src/entities/troop.c` for runtime combat profiles
- `src/entities/building.c` for base HP
- `src/systems/progression.h` for energy and King scaling
- `src/logic/combat.c` for targeting and air-damage rules

Important:
- `cardgame.db` and `sqlite/seed.sql` are now aligned for this pass.
- Knight is the baseline card. Every other unit is offset from it for role, mobility, range, air access, healing, or economy value.
- Bird is airborne. Only Fishfing and King/base burst can damage it.

## Troop Cards

`APS` = attacks per second. `Interval` = `1 / APS`. `DPS` = `attack * APS`. `HPS` = `healAmount * APS`.

| Card | ID | Cost | Resource | HP | Attack | Heal | APS | Interval (s) | DPS / HPS | Range | Move | Targeting | Engagement | Delivery | Projectile | Proj Speed | Hit Radius | Splash Radius | Body Radius | Notes |
|---|---|---:|---|---:|---:|---:|---:|---:|---|---:|---:|---|---|---|---|---:|---:|---:|---:|---|
| Knight | `KNIGHT_01` | 4 | energy | 180 | 20 | 0 | 0.95 | 1.05 | 19.0 DPS | 52 | 60 | nearest | contact | instant | none | 0 | 0 | 0 | 14 | Baseline melee attacker |
| Brute | `BRUTE_01` | 6 | energy | 320 | 16 | 0 | 0.60 | 1.67 | 9.6 DPS | 56 | 38 | nearest | contact | instant | none | 0 | 0 | 0 | 18 | Frontline tank, high soak, low output |
| Assassin | `ASSASSIN_01` | 4 | energy | 140 | 16 | 0 | 1.55 | 0.65 | 24.8 DPS | 38 | 92 | farmer first / lowest HP | contact | instant | none | 0 | 0 | 0 | 12 | Fast farmer hunter and cleanup unit |
| Healer | `HEALER_01` | 5 | energy | 200 | 8 | 18 | 0.70 | 1.43 | 5.6 DPS / 12.6 HPS | 135 | 46 | nearest injured ally in range, else enemy in range | direct range | projectile | healer blob | 240 | 14 | 0 | 14 | Backline healer, does not chase for offense |
| Farmer | `FARMER_01` | 3 | energy | 150 | 0 | 0 | 0.00 | n/a | n/a | 0 | 58 | none | free-goal | none | none | 0 | 0 | 0 | 14 | Economy core; combat disabled at runtime |
| Bird | `BIRD_01` | 5 | energy | 150 | 20 | 0 | 0.75 | 1.33 | 15.0 DPS | 145 | 72 | nearest valid ground target | direct range | projectile | bird bomb | 220 | 12 | 40 | 14 | Airborne splash harasser |
| Fishfing | `FISHFING_01` | 4 | energy | 110 | 24 | 0 | 0.90 | 1.11 | 21.6 DPS | 170 | 54 | anti-air first | direct range | projectile | fish | 300 | 12 | 0 | 14 | Primary Bird counter and ranged damage dealer |

## Base And King

| Entity / Card | Cost | HP | Damage | Radius / Range | Move | Body Radius | Important Details |
|---|---:|---:|---:|---|---:|---:|---|
| Base | n/a | 4000 | 0 normal attack | King burst radius: 160 | 0 | 16 | Static objective, level starts at 1 |
| King card | 8 | n/a | 48 to 75 burst damage by base level | 160 burst radius | n/a | uses owning base | Expensive emergency burst, queued through the live base |

King burst damage by base level:

| Base Level | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Burst Damage | 48 | 51 | 54 | 57 | 60 | 63 | 66 | 69 | 72 | 75 |

## Economy And Progression

| System | Value | Notes |
|---|---|---|
| Max energy | `10.0` | Players start full |
| Energy regen | `1.0` to `2.0` per second | Scales linearly from base level 1 to 10 |
| Level threshold | `10` sustenance per level | `1 + lifetime sustenance / 10`, clamped to level 10 |
| Farmer gather value | `1` sustenance | Default node value |
| Farmer node durability | `1` | One gather clears a default node |
| Base deposit slots | `4` primary, `6` queue | Controls farmer throughput at home base |

## Targeting And Air Rules

| Unit | Rule |
|---|---|
| Knight | Targets nearest valid ground enemy |
| Brute | Targets nearest valid ground enemy; no building bias |
| Assassin | Prioritizes enemy Farmers, then lowest-HP valid ground troop, then nearest valid enemy |
| Healer | Picks the lowest-health-ratio injured ally in range; only damages enemies as a fallback and does not pursue them |
| Farmer | No combat targeting |
| Bird | Targets valid ground enemies only |
| Fishfing | Prioritizes enemy Birds first, then nearest valid enemy |
| King/base burst | Hits enemy ground and air targets in the burst radius |

## Runtime Notes That Matter For Balance

- Melee `attackRange` is not literal stand-off distance. Contact units still stop at contact shells with only a small extra slack.
- Bird air immunity is enforced across direct hits, projectile impacts, and splash. Non-anti-air attackers cannot bypass it with AoE.
- Healer can still heal friendly Birds. The air restriction is hostile-damage-only.
- Farmer stats come from the card record for HP and movement, but runtime combat values are zeroed in `troop_spawn()`.
