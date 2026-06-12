# ReGravity Defied

An open-source Game Boy Advance port of the classic J2ME motorcycle trials game
**Gravity Defied**, written in C for the `arm-none-eabi-gcc` toolchain.

GitHub: https://github.com/antoxa2584x/regravity_defied_gba

## Features

- Verlet-integrated bike physics ported from the original game (3 engine
  leagues — 100cc / 175cc / 220cc — 10 tracks each).
- Articulated sprite rider (legs, torso, arm, helmet) that leans with the bike.
- Double-buffered Mode 3 renderer with a fixed 60 Hz simulation decoupled from
  the frame rate.
- **Progression**: the first track is open by default; finishing a track unlocks
  the next, and clearing a whole league unlocks the next league.
- **Best-time scoreboard** per track, saved to battery-backed SRAM.
- Crash sound effect (DirectSound).
- **Settings**: choose tilt buttons (D-pad or L/R shoulders), reset progress
  (with confirmation), and an About screen.

## Prerequisites

ARM cross-compiler (WSL/Linux):

```bash
sudo apt update
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi make
```

Regenerating the asset/sound headers (optional — the generated headers are
committed) needs Python with Pillow and miniaudio:

```bash
pip install pillow miniaudio
```

## Building

```bash
make            # builds regravity_defied.gba
make assets     # regenerate src/gd_assets.h + src/gd_sound.h from assets/
make clean      # remove build artifacts
make debug      # build with mGBA debug logging (-DDEBUG)
```

## Controls

| Button            | Action                                       |
|-------------------|----------------------------------------------|
| A                 | Accelerate / confirm                         |
| B                 | Brake / back                                 |
| LEFT / RIGHT      | Lean (D-pad tilt mode)                        |
| L / R             | Lean (shoulder tilt mode)                     |
| START             | Select / start                               |
| SELECT            | In-game: track menu · League menu: settings  |

## Project Structure

- `src/main.c` — game loop, state machine, menus, progression.
- `src/physics.c` — bike physics, rider rendering.
- `src/graphics.c` — Mode 3 drawing primitives + back buffer.
- `src/level.c` — track decoding and rendering.
- `src/sound.c` — DirectSound SFX playback.
- `src/save.c` — SRAM save/load (progress + best times).
- `src/crt0.s`, `gba.ld` — startup code and linker script.
- `assets/` — source PNGs and `sound/`; `tools/convert_*.py` bake them into headers.
- `levels.mrg` — track data (embedded into the ROM).

## Running

Run `regravity_defied.gba` in any GBA emulator (mGBA, VisualBoyAdvance, no$gba)
or on hardware via a flashcart. SRAM saves require an emulator/flashcart with
save support.

> Note (WSL): if a Windows emulator has the ROM open, the `.gba` file is locked
> and `make` cannot overwrite it — close the emulator before rebuilding.

## License

Open source. This is a fan reimplementation of Gravity Defied for educational
purposes; the original game and assets belong to their respective owners.
