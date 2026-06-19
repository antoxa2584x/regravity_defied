<div align="center">

# 🏍️ ReGravity Defied

### An open-source Game Boy Advance port of the classic J2ME motorcycle-trials game **Gravity Defied**

Written in C — bare-metal `arm-none-eabi` on GBA (no SDK, just the hardware), plus a native **Nintendo DS / DSi** build on devkitARM + libnds, both driven from one shared game core.

<br/>

![Platform](https://img.shields.io/badge/platform-Game%20Boy%20Advance-8B5CF6?style=flat-square)
![Platform](https://img.shields.io/badge/platform-Nintendo%20DS%20%C2%B7%20DSi-E60012?style=flat-square)
![Language](https://img.shields.io/badge/language-C-00599C?style=flat-square&logo=c)
![Toolchain](https://img.shields.io/badge/toolchain-arm--none--eabi--gcc-A42E2B?style=flat-square)
![Mode](https://img.shields.io/badge/video-240%C3%97160%20%C2%B7%20256%C3%97192-1f6feb?style=flat-square)
![License](https://img.shields.io/badge/license-open%20source-22C55E?style=flat-square)

🔗 **[github.com/antoxa2584x/regravity_defied_gba](https://github.com/antoxa2584x/regravity_defied_gba)**

</div>

---

## ✨ Features

| | |
|---|---|
| 🏁 **Authentic physics** | Verlet-integrated bike dynamics ported from the original — 3 engine leagues (**100cc · 175cc · 220cc**). |
| 🧍 **Articulated rider** | A jointed sprite (legs, torso, arm, helmet) that leans with the bike. |
| 🎞️ **Smooth rendering** | Double-buffered Mode 3 renderer with a fixed **60 Hz** simulation fully decoupled from the frame rate. |
| 🔓 **Progression** | The first track is open by default; finishing a track unlocks the next, and clearing a league unlocks the next league. |
| ⏱️ **Best times** | Per-track scoreboard saved to battery-backed **SRAM**. |
| 📍 **Smart cursor** | The level screen remembers and scrolls back to the **last track you opened** in each league — also persisted to SRAM. |
| ⚡ **Fast navigation** | Hold the D-pad to auto-scroll long league/level lists. |
| 🔊 **Sound** | Crash SFX via DirectSound. |
| ⚙️ **Settings** | Pick tilt buttons (D-pad or L/R shoulders), toggle sound, reset progress (with confirmation), and an About screen. |
| 🧩 **Mod packs** | Drop a `levels/*.mrg` file in and the build spits out a separate ROM with the mod name baked onto the menu. |
| 🎯 **Two targets** | One portable game core behind a small platform layer — builds a bare-metal **GBA** `.gba` and a native **DS/DSi** `.nds`. |

---

## 🚀 Quick Start

### 1 · Install the toolchain (WSL / Linux)

```bash
sudo apt update
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi make
```

> 🐍 Regenerating the asset/sound headers is **optional** — they're committed.
> If you want to, install Python with `pillow` and `miniaudio`:
> ```bash
> pip install pillow miniaudio
> ```

### 2 · Build (GBA)

```bash
make            # one ROM per levels/*.mrg  →  ReGravity_Defied_<mod>.gba
make assets     # regenerate src/gd_assets.h + src/gd_sound.h from assets/
make debug      # build with mGBA debug logging (-DDEBUG)
make clean      # remove build artifacts
```

### 3 · Play (GBA)

Run a `ReGravity_Defied_*.gba` in any GBA emulator (**mGBA**, VisualBoyAdvance, no$gba)
or on real hardware via a flashcart. SRAM saves require an emulator/flashcart with save support.

> 💡 **WSL tip:** if a Windows emulator has the ROM open, the `.gba` file is locked and
> `make` can't overwrite it — close the emulator before rebuilding.

### 4 · Build & play (Nintendo DS / DSi)

The DS build needs the **devkitPro** DS toolchain (devkitARM + libnds + calico + libfat):

```bash
# install devkitPro pacman per https://devkitpro.org/wiki/Getting_Started, then:
sudo dkp-pacman -S nds-dev
export DEVKITPRO=/opt/devkitpro

make -f Makefile.nds          # one .nds per levels/*.mrg  →  ReGravity_Defied_<mod>.nds
make -f Makefile.nds clean
```

Run a `ReGravity_Defied_*.nds` in a DS emulator (**melonDS**, DeSmuME) or on real
hardware — flashcart on DS/DS Lite, or directly on a homebrew-enabled **DSi/3DS** (the
`.nds` is flagged DSi-compatible). Saves are written to the SD card as `regravity.sav`.

> 🆕 The DS/DSi target is new: it builds, links, and packages a valid `.nds`, but
> hasn't yet been verified on hardware/emulator. The GBA build remains the primary,
> battle-tested target.

---

## 🎮 Controls

| Button | Action |
|:---|:---|
| **A** | Accelerate · confirm |
| **B** | Brake · back |
| **← / →** | Lean *(D-pad tilt mode)* |
| **L / R** | Lean *(shoulder tilt mode)* |
| **↑ / ↓** | Navigate menus · **hold to scroll faster** |
| **START** | Select · start |
| **SELECT** | In-game: track menu · League menu: settings |

---

## 🗂️ Project Structure

Portable game code shares a small **platform layer** (`platform.h`); each target
swaps in its own hardware backends — `*_gba.c` for the GBA, `*_nds.c` for the DS.

```
src/
├── main.c          game loop, state machine, menus, progression   (portable)
├── physics.c       bike physics + rider rendering                 (portable)
├── graphics.c      drawing primitives + back buffer                (portable)
├── level.c         track decoding and rendering                   (portable)
├── save.c          save logic (progress, best times, cursor)      (portable)
│
├── platform.h      color/keys/timing + platform_init/keys/timer/vsync interface
├── gba.h           GBA hardware register map (includes platform.h)
│
├── platform_gba.c  GBA: display mode, free-running timer, key read
├── graphics_gba.c  GBA: DMA back-buffer→VRAM blit + hardware fades
├── sound_gba.c     GBA: DirectSound FIFO SFX playback
├── save_gba.c      GBA: battery-backed SRAM
├── crt0.s          GBA: bare-metal startup
│
├── platform_nds.c  DS: LCDC framebuffer, DS timer, libnds key read
├── graphics_nds.c  DS: full-screen back-buffer→VRAM blit + brightness fades
├── sound_nds.c     DS: libnds soundPlaySample SFX
└── save_nds.c      DS: SD-card save file via libfat
gba.ld              GBA linker script
Makefile            GBA build      ·    Makefile.nds   DS/DSi build
assets/             source PNGs + sound/ + icon.jpg; tools/*.py bake them into headers / the DS banner icon
levels/*.mrg        track-data mod packs — one ROM is built per file
```

---

## 🛠️ How the Mod Build Works

Each `levels/<name>.mrg` is compiled into its own `build/<name>/` directory with
`MOD_NAME` baked in, embedded via `objcopy`, and linked into a standalone
`ReGravity_Defied_<name>.gba`. The mod name shows up on the league screen, so you
can ship a `classic` ROM, a `1000`-track ROM, or your own pack side by side.

`Makefile.nds` mirrors this exactly for the DS, producing one `.nds` per mod.

---

## 🎯 GBA vs. DS — what differs

The game logic, rendering primitives, menus and physics are identical on both —
only the thin hardware backends change:

| | Game Boy Advance | Nintendo DS / DSi |
|---|---|---|
| **Toolchain** | bare-metal `arm-none-eabi`, custom `crt0.s` + `gba.ld` | devkitARM + libnds + calico |
| **CPU** | ARM7TDMI @ 16.78 MHz | ARM9 @ 67 MHz (+ ARM7 for sound) |
| **Display** | Mode 3 bitmap, DMA blit | LCDC framebuffer, rendered at native 256×192 full-screen (bottom screen black) |
| **Frame timing** | fixed 60 Hz sim; ~30 fps render | fixed 60 Hz sim; 60 fps render |
| **Sound** | DirectSound FIFO (hand-fed) | `soundPlaySample` (ARM7) |
| **Save** | battery SRAM | `regravity.sav` on SD via libfat |

---

## 📜 License

Open source, released for **educational purposes**. This is a fan reimplementation of
*Gravity Defied* — the original game and its assets belong to their respective owners.

<div align="center">
<sub>Built with C, fixed-point math, and a lot of respect for the 16.78 MHz ARM7TDMI.</sub>
</div>
