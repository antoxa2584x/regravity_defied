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
| 🎯 **Four targets, one core** | A portable game core behind a small platform layer builds a bare-metal **GBA** `.gba`, a native **DS/DSi** `.nds`, a **3DS/2DS** `.3dsx`, and a **Sony PSP** `EBOOT.PBP` — identical physics, menus, and progression on all four. |
| 🏁 **Authentic physics** | Verlet-integrated bike dynamics ported from the original — 3 engine leagues (**100cc · 175cc · 220cc**). |
| 🧍 **Articulated rider** | A jointed sprite (legs, torso, arm, helmet) that leans with the bike. |
| 🎞️ **Native-res rendering** | Software rasteriser into a per-target framebuffer, fixed **60 Hz** simulation fully decoupled from the frame rate, filling each screen 1:1 — **240×160** (GBA), **256×192** (DS), **400×240** (3DS), **480×272** (PSP). |
| 🖥️ **Dual-screen** | On DS and 3DS the bottom screen carries the track-detail card and a live in-game progress **minimap** instead of crowding the playfield. |
| 🥽 **Stereoscopic 3D** | The 3DS top screen renders once per eye with per-layer parallax (track, bike, flags pop at different depths), dialed in live by the **3D slider**. |
| 🎨 **Customization** | Recolor the helmet, rider suit, and bike from a shared palette, with a live preview; choices persist with your save. |
| 🔓 **Progression** | The first track is open by default; finishing a track unlocks the next, and clearing a league unlocks the next league. |
| ⏱️ **Best times & cursor** | Per-track scoreboard plus the last track opened per league, persisted per platform — GBA battery **SRAM**, or an SD-card / Memory Stick save file on DS/3DS/PSP. |
| ⚡ **Fast navigation** | Hold Up/Down (or the analog stick) to auto-scroll long league/level lists. |
| 🔊 **Sound** | Crash SFX on every target — GBA DirectSound, DS ARM7 `soundPlaySample`, 3DS `ndsp`, PSP `sceAudio`. |
| ⚙️ **Settings** | Pick tilt buttons (D-pad or L/R shoulders), toggle sound, reset progress (with confirmation), and an About screen. |
| 🎛️ **Platform-aware UI** | Menus center and scale to each screen — the PSP draws the interface at 2× and shows its **✗ / ◯** buttons in prompts instead of A/B. |
| 🧩 **Mod packs** | Drop a `levels/*.mrg` file in and each build emits a separate ROM per mod with its name baked onto the menu. |

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
make            # one ROM per levels/*.mrg  →  release/gba/ReGravity_Defied_<mod>_v<ver>.gba
make assets     # regenerate src/generated/gd_assets.h + gd_sound.h from assets/
make debug      # build with mGBA debug logging (-DDEBUG)
make clean      # remove build/gba and release/gba
```

Every target writes its ROMs to `release/<platform>/` (objects go under `build/<platform>/`).

### 3 · Play (GBA)

Run a `release/gba/ReGravity_Defied_*.gba` in any GBA emulator (**mGBA**, VisualBoyAdvance, no$gba)
or on real hardware via a flashcart. SRAM saves require an emulator/flashcart with save support.

> 💡 **WSL tip:** if a Windows emulator has the ROM open, the `.gba` file is locked and
> `make` can't overwrite it — close the emulator before rebuilding.

### 4 · Build & play (Nintendo DS / DSi)

The DS build needs the **devkitPro** DS toolchain (devkitARM + libnds + calico + libfat):

```bash
# install devkitPro pacman per https://devkitpro.org/wiki/Getting_Started, then:
sudo dkp-pacman -S nds-dev
export DEVKITPRO=/opt/devkitpro

make -f Makefile.nds          # one .nds per levels/*.mrg  →  release/nds/ReGravity_Defied_<mod>_v<ver>.nds
make -f Makefile.nds clean
```

Run a `release/nds/ReGravity_Defied_*.nds` in a DS emulator (**melonDS**, DeSmuME) or on real
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

make -f Makefile.3ds          # one .3dsx per levels/*.mrg  →  release/3ds/ReGravity_Defied_<mod>_v<ver>.3dsx
make -f Makefile.3ds icon     # (optional) generate the HOME-menu app icon (needs Pillow)
make -f Makefile.3ds clean
```

Run a `release/3ds/ReGravity_Defied_*.3dsx` in **Citra** or on a homebrew-enabled 3DS/2DS (copy
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
make -f Makefile.psp          # per levels/*.mrg: a game folder + a .zip of it
make -f Makefile.psp icon     # (optional) regenerate the 144×80 Game-menu icon (needs Pillow)
make -f Makefile.psp clean
```

Each mod is emitted as a ready-to-run game folder,
`release/psp/ReGravity_Defied_<mod>_v<ver>/EBOOT.PBP`, with a 144×80 `ICON0.PNG`
(generated from `assets/icon.jpg`) so the entry shows artwork in the Game menu —
plus a same-named **`.zip`** of that folder (`ReGravity_Defied_<mod>_v<ver>.zip`,
containing `<game>/EBOOT.PBP`) for distribution. Run an `EBOOT.PBP` in the **PPSSPP**
emulator, or install it on a homebrew-enabled PSP (see [Install](#-install-on-hardware)).
The PSP has a single screen, so it uses the GBA single-screen layout
(with the **interface drawn at 2× and reflowed** for the larger panel), rendered at the
LCD's native **480×272**. Cross accelerates, Circle/Square brake, the D-pad or analog
stick lean/steer, and the shoulder triggers are L/R — and the on-screen prompts show
the PSP's **✗ / ◯ glyphs** instead of A/B. Saves are written to the Memory Stick at
`ms0:/PSP/SAVEDATA/regravity_defied.sav`.

> 🆕 The PSP target is new: it builds against pspsdk and packages a valid
> `EBOOT.PBP`, but hasn't yet been verified on hardware/emulator. The GBA build
> remains the primary, battle-tested target.

---

## 📥 Install on hardware

Each build lands in `release/<platform>/`. Pick your mod's ROM and follow the steps
for your device. All targets run in an emulator too (see each build section above).

### Game Boy Advance — `release/gba/*.gba`
1. Copy the `.gba` onto a GBA flashcart's SD/CF card (EZ-Flash, EverDrive-GBA, …).
2. Launch it from the cart's menu.
3. **Saves** need a flashcart with SRAM save support; progress is stored in the ROM's battery SRAM.

### Nintendo DS / DSi — `release/nds/*.nds`
- **DS / DS Lite:** copy the `.nds` to a DS flashcart (R4, etc.) and launch from its menu.
- **DSi / 3DS (homebrew):** the `.nds` is DSi-flagged — run it from the SD card via [TWiLight Menu++](https://github.com/DS-Homebrew/TWiLightMenu) or nds-bootstrap.
- **Saves** are written to the SD card as `regravity.sav`.

### Nintendo 3DS / 2DS — `release/3ds/*.3dsx`
1. On a homebrew-enabled console, copy the `.3dsx` to the SD card's `/3ds/` folder.
2. Launch it from the **Homebrew Launcher**.
3. **Saves** are written to the SD card root as `regravity_defied.sav`.

### Sony PSP — `release/psp/*.zip`
1. On a CFW / homebrew-enabled PSP, **extract the `.zip` into `ms0:/PSP/GAME/`** on the
   Memory Stick (it already contains a `ReGravity_Defied_<mod>_v<ver>/EBOOT.PBP` folder,
   so it lands at `ms0:/PSP/GAME/ReGravity_Defied_<mod>_v<ver>/EBOOT.PBP`).
2. The entry appears in the PSP's **Game** menu with its icon; launch it there.
3. **Saves** are written to `ms0:/PSP/SAVEDATA/regravity_defied.sav`.

> Equivalently, copy the unzipped `release/psp/ReGravity_Defied_<mod>_v<ver>/` folder
> directly into `ms0:/PSP/GAME/` — the `.zip` is just that folder, packaged.

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

The source tree is split by responsibility: a portable **core**, the **generated**
asset headers, and one **platform** backend folder per target. The core talks to
each backend only through the small interface in `core/platform.h`, so adding a
target is a new `platform/<name>/` folder plus a Makefile.

```
src/
├── core/                    portable game code + the platform interface (built for every target)
│   ├── main.c               game loop, state machine, menus, progression
│   ├── physics.c/.h         bike physics + rider rendering
│   ├── graphics.c/.h        drawing primitives + back buffer
│   ├── level.c/.h           track decoding and rendering
│   ├── save.c/.h            save logic (progress, best times, cursor)
│   ├── platform.h           color/keys/timing + platform_init/keys/timer/vsync interface
│   └── sound.h              SFX interface
│
├── generated/               committed, tool-generated (do not hand-edit)
│   ├── gd_assets.h           sprites/images  (tools/convert_assets.py)
│   ├── gd_sound.h            crash SFX PCM   (tools/convert_sound.py)
│   └── gd_sprites.h          flag sprites
│
└── platform/                one hardware backend per target (platform_/graphics_/sound_/save_)
    ├── gba/   …_gba.c + gba.h (register map) + crt0.s (startup) + gba.ld (linker script)
    ├── nds/   …_nds.c         LCDC framebuffer · libnds sound · libfat SD save
    ├── 3ds/   …_3ds.c         libctru gfx + stereo 3D · ndsp sound · stdio SD save
    └── psp/   …_psp.c         GU 480×272 present · sceAudio SFX · Memory Stick save

Makefile  GBA  ·  Makefile.nds  DS/DSi  ·  Makefile.3ds  3DS/2DS  ·  Makefile.psp  PSP
build/<platform>/      intermediate objects        release/<platform>/   final ROMs
assets/                source PNGs + sound/ + icon.jpg; tools/*.py bake them into src/generated/ + the DS/3DS icons
levels/*.mrg           track-data mod packs — one ROM is built per file
```

---

## 🛠️ How the Mod Build Works

Each `levels/<name>.mrg` is compiled into its own `build/gba/<name>/` directory with
`MOD_NAME` baked in, embedded via `objcopy`, and linked into a standalone
`release/gba/ReGravity_Defied_<name>_v<ver>.gba`. The mod name shows up on the league
screen, so you can ship a `classic` ROM, a `1000`-track ROM, or your own pack side by side.

`Makefile.nds`, `Makefile.3ds` and `Makefile.psp` mirror this exactly for the DS, 3DS
and PSP, producing one `.nds` / `.3dsx` / game-folder `EBOOT.PBP` per mod under
`release/<platform>/`.

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
