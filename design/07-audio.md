# 07 -- Audio

## 1. Overview

Audio contributes to atmosphere and feedback. The overall audio design should
complement the matte, quiet, diorama aesthetic -- sounds are **intimate and
close**, as if the player is looking at a miniature world.

## 2. Music

### 2.1 Structure

- **Per-biome soundtrack** -- each biome has its own music track or set of
  tracks.
- **Title screen** has a distinct theme.
- **Overworld** may share the biome music or have its own lighter arrangement.

### 2.2 Style

> **TBD:** Musical direction is not yet finalized. Candidate directions:
>
> - Ambient / atmospheric (Brian Eno-influenced, sparse and contemplative)
> - Greek-themed instrumentation (lyre, aulos, frame drum with modern treatment)
> - Minimalist / puzzle-game (clean tones, gentle rhythms, Lumines-like)
> - Acoustic / folk (guitar, strings, understated)
>
> These are not mutually exclusive -- the soundtrack could blend elements.

### 2.3 Behavior

- Music loops seamlessly within a biome.
- Crossfade on biome transitions.
- Music volume reduces during the Minotaur's movement phase to heighten
  tension (optional, may be too subtle to notice).
- Player-adjustable volume (see [05 -- UI/UX](05-ui-ux.md) §4).

## 3. Sound Effects

### 3.1 Puzzle SFX

| Event                   | Sound Direction                           |
|-------------------------|-------------------------------------------|
| Theseus moves           | Light footstep / soft click               |
| Theseus waits           | Subtle breath / ambient rustle            |
| Minotaur moves          | Heavier footstep, slightly ominous        |
| Minotaur blocked        | Dull thud against wall                    |
| Environmental trigger   | Feature-specific (gears, spikes, water)   |
| Win (reach exit)        | Bright, satisfying chime / flourish       |
| Loss (caught)           | Low, dramatic stinger                     |
| Undo                    | Soft rewind / reverse whoosh              |
| Reset                   | Quick scatter / rebuild sound             |

### 3.2 Menu / UI SFX

- Menu navigation: soft tick / click.
- Menu confirm: gentle positive tone.
- Menu back: soft negative/neutral tone.

### 3.3 Overworld SFX

- Player token movement: light tap / footstep.
- Level enter: subtle dramatic lead-in.
- Biome transition: atmospheric whoosh or portal sound.

## 4. Ambient Sound

Each biome may have a **background ambient loop** layered under the music:

- Stone Labyrinth: echoing drips, distant wind
- Dark Forest: crickets, rustling leaves, owl calls
- Mechanical Halls: ticking gears, hissing steam
- Infernal Dungeon: crackling fire, deep rumbles
- etc.

Ambient loops play continuously and crossfade with biome transitions.

## 5. Technical Considerations

- Audio playback via SDL3 audio API (SDL_mixer or raw SDL audio).
- Formats: OGG Vorbis for music, WAV for short SFX.
- Minimal audio channels needed (no 3D spatial audio required given the
  top-down diorama perspective).
- Music and SFX on independent volume channels.

> **TBD:** Will adaptive music be needed (e.g. layers that change based on
> proximity to the Minotaur)? This adds complexity but could enhance tension.
