# Theseus and the Minotaur -- Design Document Index

A 2D puzzle game rendered as a stylized 3D voxel diorama. Players guide Theseus
through a labyrinth of walled grid-based puzzles, outsmarting the Minotaur by
exploiting its deterministic movement rules. Built on a custom SDL3 + OpenGL
engine with orthographic projection.

## Document Index

| #  | Document                                          | Description                                                  |
|----|---------------------------------------------------|--------------------------------------------------------------|
| 01 | [Core Mechanics](01-core-mechanics.md)            | Grid rules, turn cycle, movement, win/loss, Minotaur AI      |
| 02 | [Visual Style](02-visual-style.md)                | Voxel diorama aesthetic, materials, lighting, color palette   |
| 03 | [Level Design](03-level-design.md)                | Biomes, difficulty curve, level format, environmental features|
| 04 | [Overworld](04-overworld.md)                      | Biome dioramas, progression graph, navigation                |
| 05 | [UI / UX](05-ui-ux.md)                            | Menus, HUD, save system, transitions, accessibility          |
| 06 | [Input](06-input.md)                              | Abstraction layer, keyboard/gamepad/touch/remote schemes      |
| 07 | [Audio](07-audio.md)                              | Music, SFX, ambience                                         |
| 08 | [Engine Architecture](08-engine-architecture.md)  | SDL3+OpenGL, rendering pipeline, scene graph, ECS             |
| 09 | [Content Pipeline](09-content-pipeline.md)        | Asset formats, voxel authoring workflow, level data format    |
| 10 | [Platform Targets](10-platform-targets.md)        | Target platforms, build matrix, platform-specific concerns    |

## High-Level Parameters

- **Grid sizes:** 4x4 to 16x16, any rectangular dimension
- **Biomes:** ~13 (1 tutorial + 12 main), all Greek/ancient/Crete themed
- **Levels:** 100--200 total, ~10 per biome + secret levels
- **Level authoring:** External generator (not in this repo)
- **Actors:** Theseus (player) + one Minotaur (AI) per level
- **Platforms (launch):** Steam (Windows, macOS, Steam Deck)
- **Platforms (fast-follow):** Apple TV (tvOS), iOS / iPadOS
