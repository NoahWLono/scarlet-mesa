# Release validation

The project includes repeatable simulation tests and an application smoke test. Run them with `ctest --test-dir build --output-on-failure` after building.

## Simulation

Eleven test groups cover movement and focus, clamping, pausing, dialogue, the player hitbox, once-only grazing, damage, continues, bombs, shooting, pickups, power limits, wave and boss transitions, practice, timeouts, deterministic results, invalid inputs, and entity cleanup.

Three complete campaigns exercise all nine patterns and reach the ending on Easy, Normal, and Lunatic. These progression tests grant invulnerability and wait for phase timeouts; they verify the campaign state machine, not human difficulty. They finish after about 408 seconds of simulated play, excluding dialogue.

A separate test fires real player shots and predicts boss movement for all 27 spell/difficulty combinations. Each spell can be captured within its 38-second limit at maximum power. Invulnerability isolates damage output from dodging, so these results establish attainable damage with uninterrupted aim, not a guarantee of an easy capture.

## Application

`scarlet-mesa --smoke-test <directory>` queues keyboard events through SDL and checks menu navigation, difficulty, dialogue, movement, focused movement, shooting, pause/resume, once-per-press bombs, autofire, mute, and every practice encounter. It saves eight rendered BMP screenshots. The Python wrapper also tests a missing audio backend and checks that play continues silently.

On the development Linux host, this smoke test passed both with dummy drivers and in a native Wayland window using the OpenGL renderer. The screenshots in `assets/screenshots/` came from that native run. AddressSanitizer and UndefinedBehaviorSanitizer checks passed for the simulation and application. LeakSanitizer could not run under the host's tracing environment; tests separately check bounded entity counts and cleanup.

## Packaged platforms

The release workflow builds Linux x86-64 on Ubuntu 22.04 and a universal macOS application with Apple Silicon and Intel slices. It tests the extracted archives from an unrelated working directory, verifies the Mac slices with `lipo`, and runs the Mac package on both Apple Silicon and Intel runners. Publishing a tagged release depends on those jobs succeeding.

The Mac app is ad-hoc signed, not notarized. Linux release archives bundle SDL2 and the C++ runtime, while loading the host's desktop and audio libraries. Source builds are the path for other architectures and Unix platforms; they are not covered by the binary release matrix.
