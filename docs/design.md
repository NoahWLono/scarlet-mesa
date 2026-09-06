# Scarlet Mesa

A free C++17 / SDL2 Touhou fan game. Reimu follows an incident from the Golden Gate Bridge through a SoMa accelerator to Yukari's boundary model above the bay. The writing treats AI safety jargon as fictional spell-card comedy. All sprites, visual assets, and music are created for this game; Touhou characters and setting are credited to Team Shanghai Alice.

## Play

A portrait 600 by 800 playfield sits inside a 1200 by 900 presentation with a persistent sidebar. Three stages each have an enemy-wave section, pre-boss dialogue, and three distinct spell patterns. Players move, hold fire, focus for precise movement, graze bullets, collect power and points, and use bombs. A small central hitbox is visible while focused. Easy, Normal, and Lunatic alter bullet density and speed. Stage practice, pause, restart, continue, game over, and an ending make the campaign usable end to end.

## Implementation plan

1. Build and test a deterministic, SDL-independent simulation in `game.hpp` / `game.cpp`. Verify movement bounds, collisions, grazing, bombs, pausing, phase changes, full-campaign completion, and bounded entities.
2. Create original title art and a portable font atlas in `assets/`. Draw gameplay sprites, San Francisco scenery, and bullets with SDL geometry.
3. Compose loopable music and synthesize effects. Use SDL audio without mixer dependencies.
4. Integrate the title, options, practice, dialogue, gameplay, ending, audio controls, persistent records, and screenshots in a C++ application using a fixed timestep.
5. Build native packages, run simulation tests plus application smoke tests, inspect screenshots and actual keyboard-driven play, and review the final source.
6. Publish source and free downloadable packages to the authenticated user's GitHub repository. Verify the remote commit and released files.

Simulation and render/audio modules use `src/game.hpp` as their contract. Intermediate work belongs in the parent `work/` directory; only source and distributable assets belong in the deliverable repository.
