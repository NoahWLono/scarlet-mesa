# Credits

Scarlet Mesa is a free, unofficial fan game based on **Touhou Project**, created by **ZUN / Team Shanghai Alice**. Reimu Hakurei, Cirno, Sakuya Izayoi, Yukari Yakumo, and the Touhou setting belong to their original creators. This game is not affiliated with or endorsed by Team Shanghai Alice. The [official fan creation guidelines](https://touhou-project.news/guidelines_en/) apply to the underlying Touhou material.

## Game

Created for NoahWLono with OpenAI Codex. The C++17 game simulation, SDL2 renderer, gameplay geometry, scripts, and original music were written for Scarlet Mesa. No official Touhou game assets or melodies were extracted. The original source code and music use the [MIT license](LICENSE); it does not extend to Touhou character rights.

The AI safety references are fictional jokes within the story. The game does not make claims about real models, companies, or people.

## Art and typography

The title illustration is an original generation made with OpenAI's built-in image generation tool. It depicts Reimu and Yukari over the Golden Gate Bridge. [Asset provenance](assets/ART-PROVENANCE.txt) and the [full generation prompt](assets/title-prompt.txt) are included. The title image is not an official Touhou illustration.

The printable ASCII atlases were rasterized from **JetBrains Mono**, by the JetBrains Mono Project Authors, using the locally installed Nerd Font Mono build. The font is licensed under the [SIL Open Font License 1.1](assets/FONT-LICENSE.txt). [Atlas metadata](assets/font-metadata.json) records the source filenames, hashes, and rendering metrics. [JetBrains Mono upstream](https://github.com/JetBrains/JetBrainsMono).

## Music and sound

The soundtrack uses original melodies and procedural synthesis, with no sampled recordings or copied Touhou themes. The WAV files can be rebuilt with Python and NumPy using `python3 scripts/make_audio.py`. Sound effects are synthesized by the game's audio code.

| Stage | Track | Tempo |
| --- | --- | --- |
| 1 | Golden Gate Overclock | 144 BPM |
| 2 | Gradient Descent at Midnight | 154 BPM |
| 3 | Boundary of a Finite Oracle | 164 BPM |

## Runtime dependency

[Simple DirectMedia Layer 2](https://www.libsdl.org/) provides windowing, input, graphics, and audio under the zlib license. Bundled releases include `SDL-LICENSE.txt`. CMake can use a system SDL2 installation or fetch the pinned source release recorded in `CMakeLists.txt`.
