# Project Map

## Purpose
Jogo PIF, described in the supplied version 0.1 presentation, is a 2D top-down action roguelite combining combat and logic challenges.

## Requirements
- Newer learner decision: fight several ordinary minions, encounter a room boss, and unlock a seal after defeating the boss. Cuphead is a reference for boss emphasis.
- Some special enemies have easier seals; bosses have harder seals. Avoid stopping combat to seal every ordinary minion.
- A failed special-enemy or room-boss seal revives the enemy stronger. It can revive repeatedly without a fixed cap; each defeat requires another seal until the player succeeds or loses. The player approaches a fallen special/boss and presses E to start the seal; combat pauses while the challenge is open.
- A successful room-boss seal grants a seal item and opens the next area.
- Run uses five spaces: clearing, trail ruins, and profaned courtyard each have five swarm rounds with special enemies followed by a boss; chapel is a narrative pause without combat; fractured sanctuary is the necromancer encounter.
- A swarm round ends after all enemies are eliminated, including revived special enemies. Between rounds, the player chooses from presented bonus options. Bonus categories include weapons, damage, and life.
- Enemies enter from outside the player's visible screen. Every wave has at least one special enemy; counts of ordinary and special enemies grow gradually across waves. Enemy strength scales mainly by area and wave, with a light adjustment to the player's current stats. Normal post-wave rewards present three choices; a weapon switch can be one of these choices and carries its random bonus. Any enemy can drop a ground health pickup that must be collected.
- Opening flow: main menu offers Start Run, Continue and Settings. If an unfinished run exists, Start Run warns before replacing it. Start Run plays a skippable cutscene, then the player chooses the starting weapon inside the game. Each reward choice, including the pre-boss one, has one free reroll that replaces its presented options. Continue resumes from the start of the latest wave or boss checkpoint. The game saves after reward selection before the next wave/boss, including build and health; progress within an unfinished wave resets. Failed boss seals create new checkpoints with the boss stronger and the failed attempt retained.
- Logic challenges: ultimate and easier special-enemy seals use true/false; area-boss seals use multiple-choice equivalent-expression simplification; the final necromancer seal uses more complex simplifications. Exact operators and difficulty progression remain open.
- Normal enemy roles: melee pursuer and ranged attacker, with consistent readable silhouettes. They are the same creatures becoming more corrupted across phases. Special enemies retain a telegraphed charge behavior and receive progressively corrupted visual variants in clearing, ruins and courtyard.
- Area bosses have much larger sprites and distinct attack patterns. Their precise visual designs and moves are deferred until the gameplay base is defined.
- Exactly one equipped weapon at a time. At run start the player chooses bible, katana, or firearm; between rounds a different weapon may replace it. Katana: area slash, repeats while mouse is held according to attack cadence. Bible: rotates automatically, with click-triggered faster rotation on cooldown. Firearm: straight shot toward aim, repeats while mouse is held according to attack cadence. Each weapon has at most three upgrades; prior levels persist within the run when switching away. The earlier simultaneous-weapon assumption is superseded.
- Every weapon switch between waves grants a random bonus whose identity is unknown before the choice and shown clearly after. Switch bonuses persist and stack for the run, including repeated bonus types. There is no separate stack cap; wave count limits opportunities to switch. Exact bonus pool remains undecided.
- A weapon-swap card is part of the normal three-option random reward pool and has a high but non-guaranteed appearance chance. The card names which of the other two weapons will replace the current one; only the added random switch bonus is hidden until selected.
- A normal three-option bonus choice follows waves one through four. After wave five in each combat area, a stronger two-category choice replaces the normal reward before the boss: buff the currently equipped weapon or buff player stats. This reward is separate from the three normal weapon upgrades and lasts for the rest of the run. Each choice screen has one free reroll.
- Controls chosen: WASD movement, left mouse weapon action, right mouse or Space dodge. Dodge is a short movement-direction impulse with a cooldown.
- Ultimate charges by dealing damage and activates with Q. It pauses combat for a logic question; wrong answers still release a weaker attack. Bible: greatly increased radius and spin speed. Katana: jumping super slash. Firearm: large straight-line burst.
- Guardian protection: a would-be fatal hit automatically triggers a logic proposition. Correct answer restores 50% of maximum health but spends the guardian's single use for the run, so it is unavailable at the final seal; wrong answer ends the run.
- Guardian rescue and restored health are saved immediately so Continue preserves the spent one-use power even when an unfinished wave restarts.
- Final necromancer seal allows three logic attempts. After each failed attempt, the necromancer returns stronger and must be fought again. After three failures, available guardian power completes the ritual; if it was already spent, the run ends in defeat. A correct answer wins.
- Three area-boss seals together open the passage to the fractured sanctuary.
- Closing during an unchosen reward screen restarts the completed wave. Closing during an ultimate or seal question returns to the last saved wave/boss checkpoint.
- On a lost run, choose one of three status advantages (maximum health, damage, speed) for the next run; weapon upgrades reset. Bonuses from consecutive losses accumulate until a victorious run resets the accumulated advantage.
- Presentation concepts include a cursed forest, angel guardian, necromancer, ultimate attacks and logic seals. Proposed details in the document remain subject to discussion.
- The earlier raylib challenges 1 and 2 are intended as learning steps, but their role in PIF is unresolved. The meteor game is no longer the target.
- A user-supplied practical development guide (2026-11-02) proposes a first playable room built stepwise: provisional player and floor, collision, pursuer, basic attack, enemy attack/dodge, fallen body, logic seal, then art. Its visual-map/collision-map distinction supports using an image for the background while keeping obstacles as gameplay data. It proposes a one-time revival and 16x16 tiles/32x32 character frames, but those are document proposals, not user decisions; the agreed unlimited revival rule takes precedence.
- Combat arenas are large but bounded, with passages to later arenas. They form one continuous world rather than separate loaded maps. For the clearing prototype, the camera fits the entire arena on screen rather than following the player, per the user's latest correction. Background art may be prepared by area and drawn at world positions; collision data remains separate. The player can return to cleared arenas, which do not spawn new waves or rewards. The passage behind the current arena closes during an active wave and during the boss fight; it reopens when the wave ends or the boss is sealed.

## Components
- `src/game.c`: raylib window, fullscreen setup, camera fitting the clearing to the screen, WASD movement, temporary HUD and pedestal interaction.
- `src/clearing.c` / `src/clearing.h`: clearing texture, generated collision mask, map bounds, spawn and pedestal interaction position.
- `assets/clareira.png`: learner-provided clearing art (1672x941 pixels).
- `design/clareira_marcada.png`: learner's latest red collision lines, black branch marks and yellow spawn marker; `tools/generate_clearing_mask.py` converts red lines to `assets/clareira_collision.png` and black marks to `assets/clareira_foreground_mask.png`. Pedestal area remains at its earlier code position because the annotation has no blue marker.
- `CMakeLists.txt`: C99 build configuration, raylib 5.5 via FetchContent.

## Main Flow
Implemented: `main` initializes fullscreen window -> loads clearing, collision and foreground masks -> updates provisional player movement with short-step wall checks -> fits the whole clearing in the viewport -> draws clearing, player, then marked branches above the player -> unloads resources/closes window. Outer bounds and the latest marked red barrier centerlines are active. Unmarked decorations remain passable.
Chosen gameplay outline: for each of the first three combat spaces, clear five swarm rounds with ordinary and special enemies -> choose a normal bonus after waves one through four, or a stronger weapon-or-stat choice after the fifth -> room boss -> harder boss seal (failure revives it stronger) -> collect area seal and open next space. Chapel provides a narrative pause; fractured sanctuary holds the necromancer. The fight pauses during seal challenges.

## Data and Trust Boundaries
No persistence, authentication, or external runtime services observed in starter source.

## Build and Deployment
- MSYS2 UCRT64 GCC, CMake, Ninja. `cmake --build build` previously succeeded and produced `build/JogoPiF.exe`.

## Unknowns
- Gameplay base consolidated in `docs/GAME_DESIGN.md` for learner review. Remaining numerical tuning includes wave spawn pacing, logic difficulty, enemy strengthening and health pickup chance/amount.
- Deferred tuning/content: area boss identities and specific attack patterns, exact sprite designs, specific weapon upgrade effects, stationary dodge direction, cooldown values, and exact switch-bonus pool.
- Three collected area seals open the final sanctuary passage; no other combat effect specified.
- How challenges 1 and 2 relate to the main game.
- Scene modules and navigation design.
