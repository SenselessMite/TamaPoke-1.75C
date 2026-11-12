# TamaPoke — 1.75C fork

[![MakerWorld](https://img.shields.io/badge/MakerWorld-3D%20case-00AE42?logo=bambulab&logoColor=white)](https://makerworld.com/es/models/2937822-tamapoke-a-pokemon-pokeball-tamagotchi)
![Board](https://img.shields.io/badge/board-ESP32--S3--Touch--AMOLED--1.75C-E7352C?logo=espressif&logoColor=white)
![Also](https://img.shields.io/badge/also-AMOLED--1.75-999999?logo=espressif&logoColor=white)
![Code](https://img.shields.io/badge/code-MIT-blue)
![Languages](https://img.shields.io/badge/languages-8-FFCB05)

A gen-1-Pokémon-inspired tamagotchi for the
**Waveshare ESP32-S3-Touch-AMOLED-1.75C** (round 466×466 AMOLED, CO5300 driver
over QSPI, CST9217 touch over I2C) — and still for the original
**ESP32-S3-Touch-AMOLED-1.75**. Raise any of the 151, evolve it, train it, take
it for walks, battle wild Pokémon and complete them all (shinies included).

> **This is a fork of [socquique/TamaPoke](https://github.com/socquique/TamaPoke)**
> (all the original game, art pipeline, case and most of this README are theirs).
> What the fork adds — see **[What's new in this fork](#whats-new-in-this-fork)**:
> support for the **1.75C** board (no microSD, no RTC), **compressed sprites in the
> internal flash**, **motion + microphone** interactions (shake, pedometer, pick-up
> to wake, clap, blow), **timing-based battles** with types, wild encounters
> while walking and a trainer rank, a **settings menu** on the BOOT button,
> **four new minigames** and a few balance changes (weight, age display).

> **Personal, non-commercial fan project.** Code is MIT; the sprites are from
> PMD SpriteCollab (CC BY-NC, Pokémon © Nintendo/Game Freak), and the 3D case is
> CC BY-NC-SA. See **[License](#license)** and **Credits**.

🔴 **3D-printed Pokéball case + print profiles → [on MakerWorld](https://makerworld.com/es/models/2937822-tamapoke-a-pokemon-pokeball-tamagotchi)**
(designed for the original 1.75; check the fit before printing it for the 1.75C).
The upstream **[web installer](https://socquique.github.io/TamaPoke/web/)** flashes the
**upstream firmware for the 1.75 only**: for the 1.75C build this fork yourself
(see **[IDE setup / build](#ide-setup--build)**).

## What's new in this fork

Based on **upstream v1.18** ([socquique/TamaPoke](https://github.com/socquique/TamaPoke),
see `CHANGELOG.md` for everything up to that version). Everything below is what
this fork **adds** or **changes** compared with that repository.

### Added

| Area | What |
|---|---|
| **Board** | Default target is the **ESP32-S3-Touch-AMOLED-1.75C**: LCD reset on GPIO1, touch reset on GPIO2, audio MCLK on GPIO16 (from Waveshare's official 1.75C repo and schematic). The original 1.75 is still one `#define` away (`BOARD_AMOLED_175` in `pin_config.h`); CI compiles both. |
| **Sprites without microSD** | The 1.75C has no card slot. The sprites (~41 MB raw) are mostly transparent pixels, so they are now stored **zlib-compressed** (`TPKZ` wrapper, ~1.2 MB for all 151 + shinies) and inflated with the ESP32-S3 ROM's `tinfl`. On the 1.75C they live in the **FAT partition of the internal flash**; on the 1.75 they still go to the SD (old uncompressed files keep working). Loading them over USB now takes seconds instead of ~10 minutes. |
| **Clock without RTC** | The 1.75C has no PCF85063: time runs on the ESP32 system clock (kept through light sleep) and is re-seeded from the last saved time after a power-off — same behaviour the 1.75 already had without a backup cell. Set the time with a swipe down. |
| **Motion (QMI8658)** | Shake = short play (the egg gets rocked), hardware **pedometer** (walks reward joy/bond, carrying the egg hatches it), pick it up to wake a dimmed screen. |
| **Microphone (ES7210)** | Clap or call → it comes to you; blow during the bath → the foam pops. |
| **Battles** | Timing-based duels with Gen-1 types, on demand (5/day) or **wild Pokémon met while walking**, trainer rank on the card. |
| **BOOT button + settings** | The board's BOOT button (unused before) opens a **settings menu** (volume, brightness, screen timeout, sensors on/off, info page, reset) and works as **back** everywhere; long press = sound on/off. |
| **More minigames** | **Play** opens a game picker: the ball game, **Who's that Pokémon?**, **Berry catch** (tilt), **Memory** (four colours and tones), **Tilt maze**, and battles. |
| **Step counter on screen** | When the pedometer reports new steps, a pill with the total shows on the main screen for 8 s (also on the card's battle page and in the menu's info page). |
| **Serial commands** | `SENS` (live IMU/mic levels and raw step count), `BATTLE [dex]` (start a duel), `SENSOR ...` lines logged on every motion/mic event. |
| **Strings** | ~40 new UI strings (menu, minigames, battles, sensors) translated into all 8 languages. |

### Changed from upstream

| What | Upstream v1.18 | This fork |
|---|---|---|
| Default board | ESP32-S3-Touch-AMOLED-1.75 | **1.75C** (`BOARD_AMOLED_175` for the old one) |
| Sprite storage | raw `.bin` on microSD (~41 MB) | `TPKZ` zlib-compressed (~1.2 MB): FAT partition of the flash on the 1.75C, SD on the 1.75 (raw files still read) |
| Partition scheme | default | **16M Flash (3MB APP / 9.9MB FATFS)** (`app3M_fat9M_16MB`), needed for the sprites on the 1.75C |
| Clock | PCF85063 RTC | RTC if present, otherwise ESP32 system clock (time while powered off is not counted on the 1.75C) |
| **Play** button | short play menu (ball minigame) | full-screen **game picker** with 6 games |
| BOOT button (GPIO0) | unused after boot | menu / back, long press = sound on/off |
| Weight | only candy added weight (+12), −1 every 3 min: almost always 0 | berries add **+3** (**+8** if already ≥ 80 FOOD), candy +12, −1 every **10 min** awake (half as fast asleep) |
| "Fit" medal | weight = 0 | weight **≤ 20** (eating no longer leaves it at exactly 0) |
| Age on the card | whole days (0, 1, 2 over a 3-day life) | **days + hours** (`AGE 1d 5h`) |
| Volume, brightness, screen timeout | fixed in code | set from the menu, saved in NVS (namespace `tpcfg`, survives "new game") |
| Sprite upload tools | `send_sd.py` raw files | `pack_bundle.py` / `send_sd.py` compress by default (`--raw` for the old format); `--port` required on Windows |
| `dex.h` | no types | one Gen-1 **type** per species (generated by `tools/gen_dex.py`) for battles |

### Fixes and workarounds

- **No SensorLib for the IMU.** SensorLib 0.5.x ships two different classes
  named `SensorQMI8658`; mixing them crashed at boot (`InstrFetchProhibited`,
  boot loop). The QMI8658 is driven by `motion.cpp` with direct register access.
- **`KEY_BOOT_PIN`, not `BOOT_PIN`:** the ESP32 core 3.3.x already declares
  `BOOT_PIN` in `esp32-hal.h`, so a macro with that name breaks the build.

### Files

- **New:** `motion.h/.cpp` (QMI8658 driver, gestures, pedometer),
  `mic.h/.cpp` (ES7210, clap/blow), `battle.h/.cpp` (types, damage, wild picks),
  `settings.h/.cpp` (persistent settings), `menu.ino` (BOOT button + settings menu),
  `games.ino` (game picker + new minigames), `tools/tpkz.py` (sprite compression),
  `test/test_battle.cpp`, `test/test_settings.cpp`.
- **Modified:** `TamaPoke.ino`, `pet.h/.cpp`, `pin_config.h`, `sdmon.h/.cpp`,
  `rtcbat.cpp`, `audio.h/.cpp`, `i18n.h/.cpp`, `dex.h`, `tools/gen_dex.py`,
  `tools/pack_bundle.py`, `tools/send_sd.py`, `tools/build_web.sh`, `web/sprites.pak`,
  `web/index.html`, the CI workflow (builds both boards), `test/Makefile` and the
  existing tests, this README and `web/README.md`.
- **Not updated yet:** `web/firmware/tamapoke.bin` is still the upstream 1.75
  build, and `CHANGELOG.md` stops at upstream v1.18.

Both sensors and the mic are **detected at boot**: on a board without them the
game simply works as before. The new game logic has host tests
(`test/test_battle.cpp`, `test/test_settings.cpp`, sensor and weight rules in
`test/test_pet.cpp`); the hardware side (sensor thresholds, mic, battle timing)
still has to be tuned on a real board.

## Status

Running on hardware (upstream, 1.75). Implemented: the 151 + shinies animated
from microSD or internal flash, full life cycle (egg by rarity → evolution →
farewell/release/runaway, each gated behind a decision dialog), bred-Pokédex
with gallery, battle stats (genes + training), **timing-based battles + wild
encounters**, **motion & mic interactions**, retention hooks (streak / bond /
medals / name), biome + real-time backgrounds, ball minigame, training bag,
animated bath, offline progression, battery (AXP2101) and PWR button,
anti-burn-in dimming, **sound (ES8311)**, **8 UI languages (English default)**,
**starter choice on first run**, and a one-click **web installer**.

Pending: testing the fork's additions on a real 1.75C (sensor thresholds,
battle timing), soak test. See **Roadmap**.

## Game manual (the actual numbers)

A quick reference to how the game really works (values straight from the code).

### Time & leveling
- **1 real minute = 1 in-game minute.** Your Pokémon gains **+1 level every hour**
  of real time. Leveling is purely time-based — caring well doesn't speed it up,
  but neglect *delays evolution*.
- It keeps **aging while powered off** (the RTC runs), catching up to **2 weeks** max.
  On the **1.75C** (no RTC) a power-off can't be measured: the clock resumes from
  the last saved time, so the hours it was off are skipped (nothing breaks, it just
  doesn't age meanwhile). Light sleep and screen-off do keep the time.

### The four stats (0–100)
Needs: **FOOD**, **JOY**, **ENE** (energy), **HYG** (hygiene). Start 80 / 80 / 80 / 100.
While **awake**, per minute:

| Stat | Drain/min | Notes |
|---|---|---|
| FOOD | −2 | |
| ENE | −1 | −1 extra if overweight (weight > 50 → sluggish) |
| HYG | −1 | **−4 more per poop** on screen (max 3 poops) |
| JOY | −1 | **−2 extra** if FOOD < 30, **−2 extra** if HYG < 30 |

- ~**15 %/min** chance to poop (only if FOOD > 40). Poops tank hygiene fast.
- **Care slip-up** = letting any stat hit **≤ 10** (30-min cooldown so it counts once).
  Each slip-up **delays evolution by 1 level** and cools the bond.

### Actions
- 🍎 **Berry** (3 flavors): +25 FOOD, **+3 weight** (+8 if it was already ≥ 80 FOOD:
  overfeeding fattens). Each species has a **hidden favorite flavor**
  → +35 FOOD, +10 JOY, ♥, bond, and it gets revealed.
- 🍬 **Candy:** +10 FOOD, +12 JOY, but **+12 weight** (fattening).
- ⚖️ **Weight** (0–100) burns off by itself, −1 every 10 min awake (half as fast
  asleep), plus exercise, walks and shakes. The card shows the age as days + hours.
- ⚽ **Play / minigame:** +JOY, −ENE; the minigame trains **SPEED** and burns weight.
- 🥊 **Training bag:** trains **STRENGTH** (~4 hits = 1 pt, cap +18/session), tires it.
- 🫧 **Bath:** clears poops, HYG → 100.
- 👆 **Pet it:** +5 JOY + bond.
- 🌙 **Sleep:** rest — ENE **+6/min**, needs drain ~**4× slower** with floors
  (FOOD 30 / JOY 35 / HYG 45). No poops, no slip-ups, can't run away while asleep.

### Sensors (motion + microphone, if the board has them)
Detected at boot (QMI8658 IMU and ES7210 mic ADC, both on the 1.75C); without
them everything else works the same. Serial command `SENS` prints live levels
for tuning the thresholds (top of `motion.cpp` / `mic.cpp`).
- 🫨 **Shake the board:** a short play, +8 JOY −2 ENE, burns weight (max once
  every 5 s, only with the screen on). Shaking the **egg** rocks it = one tap.
- 🚶 **Walk with it:** the IMU's hardware pedometer counts steps even with the
  screen off. Every **200 steps** → +4 JOY, +bond (daily cap), −1 weight, −1 ENE.
  Carrying the **egg** warms it: every **300 steps** = one tap (900 = hatch).
  When new steps arrive, a small "N steps" pill shows the total on the main
  screen for 8 s; the total is also on the battle stats page of the card and in
  the menu's info page. The chip only starts counting after ~10 steps in a row.
- ✋ **Pick it up** (after it sat still) → wakes a dimmed screen. Walking with it
  in a pocket doesn't.
- 👏 **Clap or call it** (short loud sound) → it walks to the centre / nods, like
  a pat; also wakes a dimmed screen. Mics are off while it sleeps or the screen is off.
- 💨 **Blow into the mic** during the bath → the foam pops right away; otherwise
  the wind ruffles it.

### Extra minigames (fork)
Tap **Play** → game picker. Each has its own record (player-wide) and reward:

| Game | How | Reward |
|---|---|---|
| ⚾ **Pokéball** | the original keep-it-up game | JOY, trains SPEED |
| ❓ **Who's that Pokémon?** | a silhouette, pick the name out of 3; 8 rounds (needs the sprite set) | +3 JOY per right answer (max 24), bond if ≥ 5 |
| 🍓 **Berry catch** | tilt the board (or tap where to go) to catch falling berries, 30 s | FOOD (+2 each, max 30), SPEED, burns weight |
| 🎵 **Memory** | repeat the growing sequence of colours/tones | +3 JOY per step (max 25), bond if ≥ 6 |
| 🌀 **Tilt maze** | roll the ball to the green exit, 60 s (needs the motion sensor) | +15 JOY and SPEED if you make it; score = seconds left |
| ⚔️ **VS** | an on-demand battle (see **Battles**) | |

All cost a little energy. The tilt games take the posture you start in as "flat";
if the ball rolls the wrong way on your board, flip `TILT_SX` / `TILT_SY` at the top
of `games.ino`.

### Settings menu & BOOT button (fork)
- **BOOT, short press:** on the main screen opens **settings**; anywhere else it's
  **back** (gallery, card, clock, keyboard, game picker, a game in progress — no
  reward). With the screen dimmed it just wakes it. **Long press:** sound on/off.
- **Pages** (swipe sideways, tap a row to change it):
  1. **Settings:** volume (4 levels), brightness (3), screen timeout (short 30 s /
     normal 90 s / long 3 min to dim), stay on while on USB.
  2. **Sensors:** microphone, gestures, step counter, wild encounters — each on/off.
  3. **Info:** firmware, battery, steps, trainer rank, free RAM/PSRAM, sprite storage, uptime.
  4. **Reset:** clear steps, new game (tap twice to confirm; settings are kept).
- Settings live in their own NVS namespace (`tpcfg`) and are saved when you leave the menu.

### Eggs & who you get (spawn odds)
- **First ever pet:** you pick a starter — **Bulbasaur / Charmander / Squirtle**.
- Hatch the egg: tap it **3×** (or wait — it hatches on its own).
- Every later egg rolls a **rarity tier** (over the ~79 base forms that come from eggs):

| Tier | Base chance | After a proper goodbye | # species |
|---|---|---|---|
| ✨ Legendary | ~3 %\* | ~10 % | 5 |
| 🔵 Rare | ~27 % | ~45 % | 27 |
| ⚪ Common | the rest | the rest | 47 |

  \* Legendaries only start appearing once you've **registered ≥ 25** Pokémon.
- A daily **streak** and high **bond** push rare/legendary odds higher.
- A clean **goodbye blesses** the next egg; a **run-away curses** it (forces Common).
- Within a tier it favors species whose **evolution line you haven't finished** (so
  all 151 are completable).
- **Shiny:** base **1 / 48** (→ **1 / 24** right after a goodbye), improved by
  streak/bond down to a best of **1 / 8**. Tracked separately in the dex.
- Every hatch rolls unique **genes** (90–110 % per stat) — no two are identical.

### Evolution
- Triggers when **level ≥ its evolution level** (16 for most base forms; ~30 for
  stone-style, ~40 for trade-style) **and every stat ≥ 40** at that moment.
- **Never automatic** — a button appears and **you tap to witness it** (with a
  flicker between the old and new form). Each **slip-up delays it by 1 level**.
- You can **decline** ("keep form"); it re-offers at the next level.
- *Eevee* branches toward whichever evolution you're still missing.

### The three endings (you choose & witness each — none auto-fire)
- 💛 **Farewell** — when it's a **final form** that has lived **3 days**. A button
  appears; triggering it **blesses your next egg**. You can **postpone** ("stay
  together", re-offered in a day). The good ending.
- 💔 **Run-away** — if you let **all four stats sit at 0 for a full hour**. A single
  act of care cancels it. It **curses the next egg** (forces Common). The sad ending.
- 👋 **Release** — long-press the creature to let it go on your terms (neutral).

After any ending, a **new egg** appears.

### Bonds, streaks, medals, Pokédex
- **Streak** (player-wide, survives across pets): first care each real day; milestones
  at **3 / 7 / 30 / 100** days; skipping a day breaks it.
- **Bond** (per pet, resets on hatch): grows with affection (**cap +8/day**), cools on
  neglect. Both streak & bond improve egg/shiny odds.
- **8 medals** (Lv10/25/50, favorite berry found, 7-day streak, max bond, final form,
  "fit" = weight ≤ 20 & no slip-ups), per-pet + a global counter.
- **Pokédex:** raising a species registers it; **151 + shinies** to complete.

### Battle stats
ATK / DEF / SPD = real **Gen-1 base** × genes + level + training (STRENGTH ← bag,
SPEED ← minigame, DEFENSE ← 12 h of unbroken good care), plus +2 to a random one
per battle won.

### Battles (timing-based)
- **Where:** tap **Play** → the menu offers the ball minigame or **VS** (an
  on-demand battle, **5 per day**). Walking also brings **wild Pokémon**: every
  400 steps there's a 50 % chance one shows up as a pill under the header for
  10 min — tap it to fight (wild ones don't use the daily quota).
- **How:** turns alternate (the faster one starts). A needle sweeps a bar; tap
  when it's in the middle. **Your turn:** green = critical ×1.5, yellow = normal,
  outside = weak ×0.5. **Its turn:** green = **dodge**, yellow = block ×0.5,
  outside = full hit. No tap in ~4 s counts as outside. Tap the top to flee (= loss).
- **Damage:** level, ATK/DEF and **type** (one type per species, Gen-1 chart
  simplified: ×2 / ×0.5, immunities count as ×0.5). ~5–6 hits per side at any level.
- **Opponents:** common (85 %) or rare (15 %) egg species, never legendary,
  already evolved if their level allows it; level = yours −3…+1.
- **Result:** both cost ENE (−10). **Win:** +12 JOY, bond, +2 to a random battle
  stat, +1 win (every 5 wins = +1 **trainer rank**, max 8, shown on the battle
  page of the card). **Loss:** −6 JOY, nothing else. Needs ENE ≥ 20 and awake.
- Serial `BATTLE [dex]` starts a test battle.

## Hardware

This fork targets two Waveshare boards. They share the display, touch, PMU and
audio, but differ in a few pins and parts:

| | **ESP32-S3-Touch-AMOLED-1.75C** (default) | **ESP32-S3-Touch-AMOLED-1.75** |
|---|---|---|
| Select with | nothing (default in `pin_config.h`) | `#define BOARD_AMOLED_175` / `-DBOARD_AMOLED_175` |
| LCD reset / touch reset | GPIO1 / GPIO2 | GPIO39 / GPIO40 |
| Audio MCLK | GPIO16 | GPIO42 |
| Sprites stored in | internal flash (FAT partition), compressed | microSD |
| RTC | none (system clock, re-seeded from the save) | PCF85063 |
| Motion / mic | QMI8658 + 2 mics on ES7210 | detected if present |
| Pins from | [waveshareteam/ESP32-S3-Touch-AMOLED-1.75C](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C) (`pin_config.h` + schematic) | [waveshareteam/ESP32-S3-Touch-AMOLED-1.75](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75) |

A firmware built for one board does **not** light up the screen on the other
(different reset pins).

Upstream notes on the original 1.75:

- Board: [ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75)
  — get the **Standard** (no case) or **-G** (GPS, also fits) version; **not the "-B"**
  (ships with a protective case that won't fit).
  - **Where to buy**: [Waveshare store](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm) (often on backorder), or the **-G** on Amazon — TamaPoke leaves the GPS unused, the board is otherwise identical: 🇺🇸 [.com](https://www.amazon.com/dp/B0F7XWWMJW?tag=capsuleradar-20) · 🇪🇸 [.es](https://www.amazon.es/dp/B0F7XWWMJW?tag=capsuleradar-21) · 🇩🇪 [.de](https://www.amazon.de/dp/B0F7XWWMJW?tag=capsulerada03-21) · 🇬🇧 [.co.uk](https://www.amazon.co.uk/dp/B0F7XWWMJW?tag=capsulerada0d-21) · 🇮🇹 [.it](https://www.amazon.it/dp/B0F7XWWMJW?tag=capsulerada08-21) · 🇫🇷 [.fr](https://www.amazon.fr/dp/B0F7XWWMJW?tag=capsulerada0e-21) · 🇯🇵 [.co.jp](https://www.amazon.co.jp/dp/B0F7XWWMJW?tag=capsuleradar-22) · 🇨🇦 [.ca](https://www.amazon.ca/dp/B0F7XWWMJW?tag=capsulerada09-20). Optional battery — 1100 mAh protected LiPo, MX1.25 plug: 🇪🇸 [.es](https://www.amazon.es/dp/B0F1FGZQS5?tag=capsuleradar-21) · 🇺🇸 [.com](https://www.amazon.com/dp/B0F1FGZQS5?tag=capsuleradar-20) · 🇮🇹 [.it](https://www.amazon.it/dp/B0F1FGZQS5?tag=capsulerada08-21) · 🇫🇷 [.fr](https://www.amazon.fr/dp/B0F1FGZQS5?tag=capsulerada0e-21) (DE/UK: that exact pack is not listed — search for a *protected* 3.7 V LiPo, ~1100 mAh, size **102540**, 1.25 mm micro-JST plug) — check plug polarity against the board first. <sub>Amazon links are affiliate links; as an Amazon Associate I earn from qualifying purchases.</sub>
  - **MicroSD card** (1.75 only — holds the sprite set; any small, class-10 card works): 🇪🇸 [.es](https://www.amazon.es/s?k=microsd+32gb&tag=capsuleradar-21) · 🇺🇸 [.com](https://www.amazon.com/s?k=microsd+32gb&tag=capsuleradar-20) · 🇩🇪 [.de](https://www.amazon.de/s?k=microsd+32gb&tag=capsulerada03-21) · 🇬🇧 [.co.uk](https://www.amazon.co.uk/s?k=microsd+32gb&tag=capsulerada0d-21) · 🇮🇹 [.it](https://www.amazon.it/s?k=microsd+32gb&tag=capsulerada08-21) · 🇫🇷 [.fr](https://www.amazon.fr/s?k=microsd+32gb&tag=capsulerada0e-21) · 🇯🇵 [.co.jp](https://www.amazon.co.jp/s?k=microsd+32gb&tag=capsuleradar-22) · 🇨🇦 [.ca](https://www.amazon.ca/s?k=microsd+32gb&tag=capsulerada09-20)
- Round 466×466 AMOLED, **CO5300** driver (QSPI, 80 MHz)
- Capacitive touch **CST9217** (I2C, address 0x5A)
- **AXP2101** (power management + battery + PWR button), **ES8311** audio codec
  (→ amplifier → external speaker on the MX1.25 connector); on the 1.75 also a
  **PCF85063** RTC and a microSD slot; on the 1.75C a **QMI8658** IMU and two
  microphones on an **ES7210** ADC
- Pins: see the table above and `pin_config.h`

## Libraries (Arduino IDE / arduino-cli)

| Library | Author | Use |
|---|---|---|
| GFX Library for Arduino (`Arduino_GFX`) | moononournation | CO5300 over QSPI + framebuffer in PSRAM (**not** "Adafruit GFX") |
| SensorLib | Lewis He | CST9217 touch, PCF85063 RTC (1.75 only) |
| XPowersLib | Lewis He | AXP2101 PMU (battery, brightness, PWR button) |
| U8g2 | olikraus | CJK glyphs (`unifont_t_japanese3`, the only Japanese subset that also carries `！？。、「」`; `unifont_t_korean2` for Hangul, 2350 syllables against korean1's 478); only the font data is used, not its display driver |
| ESP_I2S, FFat (bundled in the ESP32 core) | Espressif | I2S to the ES8311 codec and from the ES7210 mics; FAT partition for the sprites on the 1.75C |

The ES7210 driver is a few register writes in `mic.cpp` (ported from Waveshare's
1.75C example), the QMI8658 one is the same in `motion.cpp`, and sprite
decompression uses the `tinfl` already in the ESP32-S3 ROM: no extra libraries.

> **Why not SensorLib for the QMI8658:** SensorLib 0.5.x ships *two* different
> classes both named `SensorQMI8658` (the old header-only one in
> `src/SensorQMI8658.hpp` and a new one in `src/sensor/imu/qmi8658/` with its own
> `.cpp`). Including the old one links it against the new one's vtable, and the
> first virtual call in `begin()` jumps to garbage: `InstrFetchProhibited` during
> `setup()` and an endless reboot loop. The fork talks to the chip's registers
> directly instead (map and pedometer sequence taken from SensorLib itself).

## IDE setup / build

- Board: **ESP32S3 Dev Module** · Flash **16MB** · PSRAM **OPI PSRAM**
  (required: the 466×466×16-bit framebuffer ≈ 434 KB lives in PSRAM) ·
  Partition Scheme **`16M Flash (3MB APP/9.9MB FATFS)`** (required on the 1.75C:
  the sprites live in that FAT partition) · USB CDC On Boot **Enabled**
  (otherwise `Serial` is the UART and the build fails on `setTxTimeoutMs`)
- Target board: the 1.75C by default; for the original 1.75 uncomment
  `#define BOARD_AMOLED_175` in `pin_config.h` (or pass `-DBOARD_AMOLED_175`).
- Tested with the ESP32 core **3.3.10** (CI) and 3.3.12.

**Arduino IDE, step by step**
1. *Boards Manager* → install **esp32** by Espressif (3.3.10 recommended).
2. *Library Manager* → install the four libraries in the table above.
3. Open `TamaPoke.ino` (the folder must be called `TamaPoke`), pick the settings
   above in the *Tools* menu and the board's COM port, then **Upload**. The first
   build takes several minutes (the core and U8g2's fonts are compiled once and
   then cached).
4. Load the sprites (next sections), then restart with the PWR button.

**arduino-cli**

```bash
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
arduino-cli compile --fqbn "$FQBN" .                                   # 1.75C
arduino-cli compile --fqbn "$FQBN" --build-property "compiler.cpp.extra_flags=-DBOARD_AMOLED_175" .  # 1.75
arduino-cli upload -p /dev/cu.usbmodemXXXX --fqbn "$FQBN" .
```

`bash tools/build_web.sh` (1.75C) or `bash tools/build_web.sh 175` (1.75)
rebuilds `web/firmware/tamapoke.bin` and the sprite bundle for the web installer.

### Easiest install: the web installer

`web/index.html` flashes the firmware (ESP Web Tools) and pushes the sprites
over Web Serial (to the SD on the 1.75, to the internal flash on the 1.75C), no
Arduino needed. Serve it over HTTPS or `localhost` (secure context) and open it
in **Chrome/Edge**. See [`web/README.md`](web/README.md).

> In this fork the bundled `web/firmware/tamapoke.bin` is still the **upstream
> v1.18 build for the 1.75**, which can't read the compressed sprites. Run
> `bash tools/build_web.sh` before publishing the installer. To load the sprites
> after flashing from the Arduino IDE, just use step 2 of the page locally:
>
> ```bash
> cd web && python3 -m http.server 8000   # open http://localhost:8000, skip step 1
> ```
>
> Close the IDE's Serial Monitor first: only one program can use the port.
> A full **Erase** on the 1.75C also wipes the sprites (they live in the flash):
> load them again.

### Generate and load the sprites yourself

All sprites come from **[PMD SpriteCollab](https://github.com/PMDCollab/SpriteCollab)**
(CC BY-NC). You can regenerate the whole set and load it onto your board with the
pipeline below — the firmware accepts files over USB (PUT protocol with per-block
ACK), so you don't have to remove anything (on the 1.75 it formats the SD to FAT
if needed; on the 1.75C it formats the flash's FAT partition on first boot).

```bash
python3 tools/pack_pmd.py       # fetch + pack PMD sprites: the 151 + shiny -> tools/sdcard/mons/p[s]NNN.bin
python3 tools/make_thumbs.py    # Pokédex thumbnails (from the PMD sprites) -> thumbs.bin
python3 tools/send_sd.py --port COM5   # send them over USB, compressed (port is required on Windows)
```

To make the **one-click web-installer bundle** instead of sending over USB:

```bash
python3 tools/pack_bundle.py    # bundle tools/sdcard/mons/* into web/sprites.pak (compressed)
```

Then load it from the web installer's **"Load sprites"** button (or `send_sd.py`
above). `pack_pmd.py` also takes individual dex numbers, e.g. `pack_pmd.py 7 25`.

**Compression (`TPKZ`):** `pack_bundle.py` and `send_sd.py` wrap every file as
`"TPKZ" + uint32 raw size + zlib stream` (`tools/tpkz.py`); the firmware inflates
it on load and still accepts plain files. ~41 MB raw → **~1.2 MB** (about 1.7 MB
on the FAT partition), so it fits the 1.75C's flash with room to spare and uploads
in under a minute. `--raw` on either tool sends uncompressed files, for firmware
older than this fork.

## How to play

On first run you **choose a starter** (Bulbasaur / Charmander / Squirtle). After
that you start with an **egg**. Tap it 3 times or wait and it hatches. From then
on, care for your companion:

**Four stats** that decay: **FOOD**, **JOY**, **ENE** (energy), **HYG** (hygiene).
If one bottoms out it counts as a *slip-up*.

**Buttons (bottom arc, icons):**
- 🍎 **Feed** → food menu: 3 berries (each species has a hidden favourite that
  gives a bonus) and a candy (+happiness but it fattens; weight makes it sluggish).
- ⚽ **Play** → the **game picker**: the pokeball minigame (trains SPEED), four
  extra minigames and **VS**, a battle against a random wild Pokémon (5 per day).
  See **Extra minigames** and **Battles** above.
- 🌙 **Light** → sleep/wake (recovers energy, dims the screen). While asleep,
  needs decay much slower (rest).
- 🫧 **Bath** → a foam scene that cleans up the poops.

**Touch gestures:**
- Tap the creature = pet it (+happiness, bond).
- Horizontal swipe = open the **Pokédex / gallery**.
- Vertical swipe up = open the **stat card** (4 pages: Profile / Battle / Medals /
  Progress; swipe between them; tap the name on Profile to rename; on Battle the
  "Train strength" button opens the bag).
- Swipe down = **set the clock** and pick the **language** + sound on/off.
- Long press (3 s) on the creature = **release** dialog.
- A **"Wild …!"** pill under the header = a Pokémon met while walking: tap it to fight.

**Board gestures (if the board has the sensors):** shake = quick play / rock the
egg · walk with it = steps (rewards, egg hatching, wild encounters) · pick it up =
wake a dimmed screen · clap or call = it comes to you · blow during the bath = pop
the foam. Details in **Sensors** above.

**Physical BOOT button (fork):** short = settings menu / back · long = sound on/off.

**Physical PWR button:** short = screen on/off · long (4 s) = full power-off
(on the 1.75 the RTC stays alive, so time passes even while it's off; on the
1.75C it doesn't, see *Time & leveling*).

## Decisions: you choose, and you watch

The three life-cycle endings and evolution **don't happen on their own** — when
the conditions are met a button appears and you tap it (so you're present to
witness it), each opening a two-option dialog:

- **Evolution** (red button): *Evolve* (epic animation: halo, rays, sparkles and
  a **flicker between the old and new form**) or *Keep form* (re-offered next level).
- **Farewell** (gold button, final form + 3 days): *Say goodbye* (warm farewell,
  rising hearts → new egg) or *Stay together* (keep your companion; re-offered in
  a day). Tension: a maxed-out friend vs. completing the Pokédex.
- **Runaway** (dark button, total neglect for 1 h): a somber "feels abandoned"
  ending in the rain — caring for the creature cancels it.

## Sprites: PMD SpriteCollab everywhere

- **PMD SpriteCollab** (everything — main screen, stat card, minigame **and the
  Pokédex grid + detail view**): behaviour sprites — `tools/pack_pmd.py` packs
  actions (Idle, Walk L/R, Sleep, Eat, Hurt, Attack, Pose, Nod, DeepBreath) into
  the multi-action **TPK2** format (`/mons/pNNN.bin`). The engine in `TamaPoke.ino`
  makes the creature wander, gesture, curl up to sleep, chew and wince. Anchored by
  the feet (lowest content row), not the canvas. The Pokédex thumbnails
  (`thumbs.bin`, TPTH) are derived from these by `tools/make_thumbs.py`.
- **In-house workshop** (`tools/sprites.py`): 9 primitive-drawn sprites as a
  no-SD fallback + the UI icons. Generates `species.h`. Preview in
  `tools/sheet.png`, emit with `python3 tools/sprites.py emit`.

`sdmon.h/.cpp` loads the PMD sprites into PSRAM (`PmdMon` for TPK2) plus the
thumbnails (`SdThumbs`). `SdMon` (TPK1) remains as a dormant legacy fallback only.

## Pokédex and species data

`tools/dex_data.py` is the **single source**: name, slug, type (accent colour +
background biome), evolution line with gen-1 levels, rarities and starters.
`tools/dex_stats.py` has the real base stats (from PokéAPI). `tools/gen_names.py`
pulls the **official localized names** from PokéAPI into `tools/dex_names.py`
(only French and German differ in gen 1; Spanish, Italian and Portuguese use the
English ones). `gen_dex.py` emits `dex.h` (the `DEX_TBL[152]` table plus the
per-language name tables and the `dexName()` accessor). The pet's identity is its
Pokédex number (persisted in NVS).

- **Evolution** gen-1 style (levels 16/36/…; stones ≈30, trade ≈40; Eevee
  branches to whichever evolution you're missing). Each slip-up delays it 1
  level; it won't evolve with any stat < 40 or while asleep.

## Battle stats and training

Each creature has ATK/DEF/SPD = real gen-1 base × **genes** (90–110 %, rolled at
hatch) + level + **training**:
- SPEED ← the minigame
- DEFENSE ← sustained good care (12 h with no slip-ups)
- STRENGTH ← the training bag (whacking)

Shown on the Battle page of the stat card. The (hidden) weight goes up with candy
and burns off with training.

## Retention: streak, bond, medals, name

- **Streak** (the player's, persists across creatures): the first care of each
  real day advances the streak; 3/7/30/100 milestones are celebrated; skipping a
  day breaks it. Flame badge on the main screen.
- **Bond** (the creature's): rises slowly with care and petting, drops with slip-ups.
- **Medals** for the individual (level, berry, streak, bond, final form, fit) +
  a global counter. Medals page of the stat card.
- **Name**: touch keyboard; the nickname rules the header and the card.

High streak and bond **improve the egg roll** (rarity and shiny): caring well
always pays off.

## Life cycle, eggs by rarity, languages

The life cycle lasts **3 days** of play. Three endings (all leave a new egg):
**farewell** (final form + 3 days), **release** (long press), **runaway** (all 4
bars at zero for 1 h). Each bred species is recorded in the **bred Pokédex**
(normal and shiny separately).

The egg rolls rarity over the ~79 base forms (47 common / 27 rare / 5 legendary),
**biased towards the lines you're missing** (all 151 are completable), blessed by
a farewell and punished by a runaway. Legendaries only with 25+ registered.
**Shiny** 1/48 (better with streak/bond/farewell).

**Languages:** the UI ships in 8 languages — English (default), Spanish, French,
German, Italian, Portuguese, Japanese and Korean — switchable from the settings
screen (swipe down).
**Pokémon names are localized too**: French, German, Japanese and Korean show
the official names (Bulbizarre, Bisasam, フシギダネ, 이상해씨...). Spanish, Italian
and Portuguese use the English ones, which is what those regions officially use
for gen 1. Nidoran♀/♂ read ニドランF/M in Japanese and 니드런 암/수 in Korean: no
U8g2 unifont subset carries ♀ or ♂, so each language picks its own substitute.

## Backgrounds: biome + real time

The idle screen paints the sky from the **RTC's real time** (dawn / day / dusk /
night with moon and stars) and the ground from the **type's biome** (meadow,
beach, forest, volcano, mountain, snow). Sleeping forces night.

## Layout

- `TamaPoke.ino` — init, game loop, render of every screen, gestures, serial console, audio
- `pet.h` / `pet.cpp` — pet state and logic (stats, evolution, life cycle, streak/bond/medals, NVS)
- `sdmon.h` / `sdmon.cpp` — TPK1 (animated) and TPK2 (PMD) sprites + thumbnails (plain or `TPKZ`-compressed), stored on the SD (1.75) or the flash's FAT partition (1.75C), and file reception over USB (PUT/LS)
- `rtcbat.h` / `rtcbat.cpp` — clock (PCF85063 RTC, or the ESP32 system clock on the 1.75C) + AXP2101 PMU (battery, brightness, PWR button)
- `audio.h` / `audio.cpp` — ES8311 + I2S + Game-Boy-style tone synth (non-blocking task); also exposes the I2S input for the mics
- `motion.h` / `motion.cpp` — *(fork)* QMI8658: shake, pick-up and hardware pedometer
- `mic.h` / `mic.cpp` — *(fork)* ES7210 init + a listening task that detects claps and blowing
- `battle.h` / `battle.cpp` — *(fork)* battle logic with no hardware (type chart, damage, wild picks); the battle screen is in `TamaPoke.ino`
- `settings.h` / `settings.cpp` — *(fork)* player settings in NVS + the pure helpers that apply them (volume register, brightness, screen timeout)
- `menu.ino` — *(fork)* BOOT button and the settings menu
- `games.ino` — *(fork)* game picker and the four extra minigames
  (the IDE compiles every `.ino` in the folder after `TamaPoke.ino`, so they see its globals and drawing helpers)
- `i18n.h` / `i18n.cpp` — the 8-language string tables
- `dex.h` — GENERATED (`gen_dex.py`): the 151 table (the fork adds each species' type)
- `species.h` — GENERATED (`sprites.py`): fallback sprites, UI icons, colours
- `pin_config.h` — the official pins of both boards (`BOARD_AMOLED_175` selects the original)
- `tools/` — pipeline: `dex_data.py` (data), `dex_stats.py`, `dex_names.py` +
  `gen_names.py` (localized names), `gen_dex.py`,
  `sprites.py` (workshop), `pack_pmd.py` / `make_thumbs.py`
  (packers), `pack_bundle.py` (web bundle), `send_sd.py` (USB upload), `tpkz.py`
  (*fork*: sprite compression), `touch_log.py`
- `tools/sdcard/mons/` — the generated .bin files (animated, shiny, PMD, thumbnails)
- `web/` — the browser installer (ESP Web Tools + Web Serial sprite loader)

## Battery

The percentage on screen comes from a typical single-cell LiPo discharge curve (`batcurve.h`) over a smoothed
voltage, and only goes down while discharging; on USB or while charging the PMU's own gauge is used. When the
smoothed voltage stays under 3.30 V for 30 s the pet is saved and the PMU powers off (not in the first 30 s
after boot); the PMU itself is set to cut at 3.1 V as a backstop.

What the firmware does to save power (the policy is in `power.h`, covered by `test/test_power.cpp`):

- **Frame rate follows activity.** 10 fps while you interact; 6.7 fps after 1 min without touching;
  4 fps when the screen dims (90 s); 1 fps when it is almost off (5 min); 2 fps while the pet sleeps and
  nobody touches it. The ball minigame keeps its 85 ms step. Over 10 idle minutes this sends about 68 % fewer
  frames to the panel (each one is a full redraw plus a 434 KB QSPI transfer).
- **Static screens are not redrawn.** Keyboard, clock, card pages 2-4 and the starter choice are painted once
  and again only when something on them changes.
- **Light sleep when nobody is looking.** With the screen off, or almost off (5 min), and on battery, the CPU
  sleeps until the next frame or 200 ms; a touch wakes it at once. With USB connected it never sleeps (the
  serial port for the SD card stays available), and USB is checked right before each sleep.
- **Speaker amplifier only when it can be used.** It is off while the pet sleeps and while the screen is dimmed
  or off; a sound requested meanwhile (e.g. "ready to evolve") switches it on for 4 s, and sounds wait 50 ms
  after a power-up so the start is not cut.
- **Low battery (< 20 %, not charging):** brightness capped (100, then 60 under 10 %) and frames twice as slow.
- **Battery reads over I2C** every 10 s instead of 2 s when the screen is off or almost off.
- The pet walks by real time, so lower frame rates do not slow it down.

## Serial console (115200, debug)

`STATS` (full state) · `SPEC <dex>` (change species) · `LVL <n>` · `HATCH` ·
`SHINY` · `NICK <x>` · `BYE` / `RUN` (farewell / runaway) · `ABANDON` (force the
runaway-ready state) · `WIPE` (factory reset → new game) · `BEEP` (audio test) ·
`REG` (Pokédex) · `EGGS` (simulate 20 eggs) · `GAL` (gallery) · `CAREDAY` ·
`TIME <epoch>` / `RTCSET <epoch>` · `HEALTH` (uptime + heap for the soak test) ·
`LS` / `PUT` / `SDINFO` (sprite files, on the SD or the flash) · *fork:* `SENS`
(live mic level / noise floor and accelerometer readings, to tune the thresholds
at the top of `mic.cpp` and `motion.cpp`) · `BATTLE [dex]` (start a test battle).
Every recognised gesture also prints a `SENSOR …` line.

To test fast: lower `PET_TICK_MS`, `MINUTES_PER_LEVEL` and `FAREWELL_AGE_MIN` in `pet.h`.

## Roadmap

- **Fork: test on a real 1.75C** — sensor thresholds (`SENS`), mic clock (if the
  mic stays silent, the ES7210's MCLK on GPIO16 is the first suspect), battle timing
  (`AIM_PERIOD_MS` in the sketch, `TIMING_*_PX` in `battle.h`).
- **Battles** — first version done (timing-based, wild encounters from walks,
  types, trainer rank). Next: trainer/gym fights, maybe battles between two boards.
- **Web installer for both boards** — today it serves a single firmware; a board
  picker (two manifests) would let one page flash either.
- **Soak test** 24–48 h (instrumentation ready: `HEALTH` command/heartbeat).

*(Done: 3D-printed case [published on MakerWorld](https://makerworld.com/es/models/2937822-tamapoke-a-pokemon-pokeball-tamagotchi); repo public with the browser installer + one-click sprite bundle.)*

## Community forks

- **[TamaPoke — Expanded](https://github.com/ShadowEnemyx/TamaPoke/tree/tamapoke-expanded-update)** by **ShadowEnemy** — a substantial community fork (different author/branch): a full **type-matchup battle system**, all **151 + shinies** with a **Pokédex / collection box** and daily goals, **6 UI languages**, **ES8311 sound**, starter choice and a one-click web installer. Worth a look. 🎮
- **[TamaPoke](https://github.com/DylanPDao/TamaPoke)** by **DylanPDao** — another substantial fork: **gym battles** and **LAN battles** between two devices, **movesets**, a **party + box** system, an **EV/IV** stat system, and coverage extended **up to Gen 3 (386)**. Keeps the PMD sprite pipeline. 🏆

## Credits

All sprites: [PMD SpriteCollab](https://github.com/PMDCollab/SpriteCollab)
(community, CC BY-NC). Base stats: [PokéAPI](https://pokeapi.co). Pokémon is a ™ of
Nintendo / Game Freak / The Pokémon Company. Non-commercial, personal-use project.
Full list in [`CREDITS.md`](CREDITS.md). Version history in
[`CHANGELOG.md`](CHANGELOG.md) — most of the fixes there came from people
who built one and reported what they found.

Original game, firmware and tooling: **[socquique/TamaPoke](https://github.com/socquique/TamaPoke)**
and its contributors. This fork's 1.75C port, compressed sprites, sensors and
battles build on that work; 1.75C pins and the ES7210 init sequence come from
[Waveshare's 1.75C examples](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C),
the QMI8658 register map and pedometer setup from [SensorLib](https://github.com/lewisxhe/SensorLib).

## License

- **Source code** (firmware + tooling): **[MIT](LICENSE)**.
- **Sprites & names**: © Nintendo / Game Freak / The Pokémon Company; pixel art
  from [PMD SpriteCollab](https://github.com/PMDCollab/SpriteCollab) (CC BY-NC 4.0).
  **Non-commercial use only.**
- **3D-printed case**: remix of *"Pokeball"* by **yoyothechicken**
  ([MakerWorld #839922](https://makerworld.com/es/models/839922-pokeball)),
  licensed **CC BY-NC-SA**, and shared here under the same terms.

This is an unofficial fan project, not affiliated with or endorsed by Nintendo.
