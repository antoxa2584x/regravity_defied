<div align="center">

# 🏍️ ReGravity Defied

### An open-source Game Boy Advance port of the classic J2ME motorcycle-trials game **Gravity Defied**

Written in C — bare-metal `arm-none-eabi` on GBA (no SDK, just the hardware), plus native **Nintendo DS / DSi** (devkitARM + libnds), **Nintendo 3DS / 2DS** (devkitARM + libctru, with stereoscopic 3D), and **Sony PSP** (pspdev / pspsdk) builds, all driven from one shared game core.

<br/>

![Platform](https://img.shields.io/badge/platform-Game%20Boy%20Advance-8B5CF6?style=flat-square)
![Platform](https://img.shields.io/badge/platform-Nintendo%20DS%20%C2%B7%20DSi-E60012?style=flat-square)
![Platform](https://img.shields.io/badge/platform-Nintendo%203DS%20%C2%B7%202DS-D12228?style=flat-square)
![Platform](https://img.shields.io/badge/platform-Sony%20PSP-003791?style=flat-square)
![Language](https://img.shields.io/badge/language-C-00599C?style=flat-square&logo=c)
![Toolchain](https://img.shields.io/badge/toolchain-arm--none--eabi--gcc-A42E2B?style=flat-square)
![Mode](https://img.shields.io/badge/video-240%C3%97160%20%C2%B7%20256%C3%97192%20%C2%B7%20480%C3%97272-1f6feb?style=flat-square)
![License](https://img.shields.io/badge/license-open%20source-22C55E?style=flat-square)

🔗 **[github.com/antoxa2584x/regravity_defied](https://github.com/antoxa2584x/regravity_defied)**

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
| 🎯 **Four targets** | One portable game core behind a small platform layer — builds a bare-metal **GBA** `.gba`, a native **DS/DSi** `.nds`, a native **3DS/2DS** `.3dsx` with stereoscopic 3D, and a **Sony PSP** `EBOOT.PBP`. |

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

### 5 · Build & play (Nintendo 3DS / 2DS — with stereoscopic 3D)

The 3DS build needs the **devkitPro** 3DS toolchain (devkitARM + libctru):

```bash
# install devkitPro pacman per https://devkitpro.org/wiki/Getting_Started, then:
sudo dkp-pacman -S 3ds-dev
export DEVKITPRO=/opt/devkitpro

make -f Makefile.3ds          # one .3dsx per levels/*.mrg  →  ReGravity_Defied_<mod>.3dsx
make -f Makefile.3ds icon     # (optional) generate the HOME-menu app icon (needs Pillow)
make -f Makefile.3ds clean
```

Run a `ReGravity_Defied_*.3dsx` in **Citra** or on a homebrew-enabled 3DS/2DS (copy
to the SD card and launch from the Homebrew Launcher). Saves are written to the SD
card root as `regravity_defied.sav`.

The 3DS is the second **dual-screen** target, so it reuses the DS layout — the
track-detail card and the in-game progress minimap live on the bottom screen.
On top of that it drives the **autostereoscopic top screen**: the flat 2D scene is
rendered once per eye with per-layer horizontal parallax (the track sits at the
screen plane, the bike floats toward you, and the flags pop out the most — a
pop-up-book depth). Slide the **3D slider** up to dial in the effect; at zero the
image is a comfortable flat 2D. The Circle Pad mirrors the D-pad for steering.

> 🆕 The 3DS target is new: it builds against libctru and packages a valid `.3dsx`,
> but hasn't yet been verified on hardware/emulator. The GBA build remains the
> primary, battle-tested target.

### 6 · Build & play (Sony PSP)

The PSP build needs the **pspdev** toolchain (psp-gcc + pspsdk + `mksfoex` + `pack-pbp`):

```bash
# install pspdev per https://github.com/pspdev/pspdev (or `brew install pspdev`),
# then make sure psp-config is on PATH:
make -f Makefile.psp          # one EBOOT per levels/*.mrg  →  ReGravity_Defied_<mod>.pbp
make -f Makefile.psp clean
```

Run a `ReGravity_Defied_*.pbp` in the **PPSSPP** emulator, or on a homebrew-enabled
PSP by copying it to `ms0:/PSP/GAME/<folder>/EBOOT.PBP` on the Memory Stick. The PSP
has a single screen, so it uses the GBA single-screen layout, rendered at the LCD's
native **480×272**. Cross accelerates, Circle/Square brake, the D-pad or analog stick
lean/steer, and the shoulder triggers are L/R. Saves are written to the Memory Stick
at `ms0:/PSP/SAVEDATA/regravity_defied.sav`.

> 🆕 The PSP target is new: it builds against pspsdk and packages a valid
> `EBOOT.PBP`, but hasn't yet been verified on hardware/emulator. The GBA build
> remains the primary, battle-tested target.

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
swaps in its own hardware backends — `*_gba.c` for the GBA, `*_nds.c` for the DS,
`*_3ds.c` for the 3DS, `*_psp.c` for the PSP.

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
├── save_nds.c      DS: SD-card save file via libfat
│
├── platform_3ds.c  3DS: libctru gfx init, stereo 3D, system-tick timer, key + Circle Pad read
├── graphics_3ds.c  3DS: BGR555→RGB565 rotated present (per-eye stereo) + software fades
├── sound_3ds.c     3DS: libctru ndsp SFX
├── save_3ds.c      3DS: SD-card save file via stdio
│
├── platform_psp.c  PSP: module header, exit callback, display setup, sceCtrl key + analog read
├── graphics_psp.c  PSP: 480×272 back-buffer→VRAM present (512 stride, 5551) + software fades
├── sound_psp.c     PSP: sceAudio SRC playback on a worker thread
└── save_psp.c      PSP: Memory Stick save file via stdio
gba.ld              GBA linker script
Makefile  GBA  ·  Makefile.nds  DS/DSi  ·  Makefile.3ds  3DS/2DS  ·  Makefile.psp  PSP
assets/             source PNGs + sound/ + icon.jpg; tools/*.py bake them into headers / the DS & 3DS icons
levels/*.mrg        track-data mod packs — one ROM is built per file
```

---

## 🛠️ How the Mod Build Works

Each `levels/<name>.mrg` is compiled into its own `build/<name>/` directory with
`MOD_NAME` baked in, embedded via `objcopy`, and linked into a standalone
`ReGravity_Defied_<name>.gba`. The mod name shows up on the league screen, so you
can ship a `classic` ROM, a `1000`-track ROM, or your own pack side by side.

`Makefile.nds`, `Makefile.3ds` and `Makefile.psp` mirror this exactly for the DS,
3DS and PSP, producing one `.nds` / `.3dsx` / `.pbp` per mod.

---

## 🎯 GBA vs. DS vs. 3DS vs. PSP — what differs

The game logic, rendering primitives, menus and physics are identical on all four —
only the thin hardware backends change:

| | Game Boy Advance | Nintendo DS / DSi | Nintendo 3DS / 2DS | Sony PSP |
|---|---|---|---|---|
| **Toolchain** | bare-metal `arm-none-eabi`, custom `crt0.s` + `gba.ld` | devkitARM + libnds + calico | devkitARM + libctru | pspdev + pspsdk |
| **CPU** | ARM7TDMI @ 16.78 MHz | ARM9 @ 67 MHz (+ ARM7 for sound) | ARM11 @ 268 MHz | MIPS Allegrex @ 222–333 MHz |
| **Display** | Mode 3 bitmap, DMA blit | LCDC framebuffer, native 256×192 full-screen | 320×240 centered on the 400-wide top screen; BGR555→RGB565 rotated present | VRAM framebuffer, native 480×272 (512 stride), 5551 present |
| **Second screen** | — | detail card + in-game minimap | detail card + in-game minimap | — |
| **Stereoscopic 3D** | — | — | per-layer parallax, rendered once per eye, scaled by the 3D slider | — |
| **Frame timing** | fixed 60 Hz sim; ~30 fps render | fixed 60 Hz sim; 60 fps render | fixed 60 Hz sim; 60 fps render | fixed 60 Hz sim; 60 fps render |
| **Sound** | DirectSound FIFO (hand-fed) | `soundPlaySample` (ARM7) | libctru `ndsp` | `sceAudio` SRC (worker thread) |
| **Save** | battery SRAM | `regravity.sav` on SD via libfat | `regravity_defied.sav` on SD via stdio | `regravity_defied.sav` on Memory Stick |

---

## 📜 License

Open source, released for **educational purposes**. This is a fan reimplementation of
*Gravity Defied* — the original game and its assets belong to their respective owners.

<div align="center">
<sub>Built with C, fixed-point math, and a lot of respect for the 16.78 MHz ARM7TDMI.</sub>
</div>
