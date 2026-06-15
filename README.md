<div align="center">

# 🏍️ ReGravity Defied

### An open-source Game Boy Advance port of the classic J2ME motorcycle-trials game **Gravity Defied**

Written in C for the bare-metal `arm-none-eabi` toolchain — no SDK, no engine, just the hardware.

<br/>

![Platform](https://img.shields.io/badge/platform-Game%20Boy%20Advance-8B5CF6?style=flat-square)
![Language](https://img.shields.io/badge/language-C-00599C?style=flat-square&logo=c)
![Toolchain](https://img.shields.io/badge/toolchain-arm--none--eabi--gcc-A42E2B?style=flat-square)
![Mode](https://img.shields.io/badge/video-Mode%203%20%C2%B7%20240%C3%97160-1f6feb?style=flat-square)
![License](https://img.shields.io/badge/license-open%20source-22C55E?style=flat-square)

🔗 **[github.com/antoxa2584x/regravity_defied_gba](https://github.com/antoxa2584x/regravity_defied_gba)**

</div>

---

## ✨ Features

| | |
|---|---|
| 🏁 **Authentic physics** | Verlet-integrated bike dynamics ported from the original — 3 engine leagues (**100cc · 175cc · 220cc**), 10 tracks each. |
| 🧍 **Articulated rider** | A jointed sprite (legs, torso, arm, helmet) that leans with the bike. |
| 🎞️ **Smooth rendering** | Double-buffered Mode 3 renderer with a fixed **60 Hz** simulation fully decoupled from the frame rate. |
| 🔓 **Progression** | The first track is open by default; finishing a track unlocks the next, and clearing a league unlocks the next league. |
| ⏱️ **Best times** | Per-track scoreboard saved to battery-backed **SRAM**. |
| 📍 **Smart cursor** | The level screen remembers and scrolls back to the **last track you opened** in each league — also persisted to SRAM. |
| ⚡ **Fast navigation** | Hold the D-pad to auto-scroll long league/level lists. |
| 🔊 **Sound** | Crash SFX via DirectSound. |
| ⚙️ **Settings** | Pick tilt buttons (D-pad or L/R shoulders), toggle sound, reset progress (with confirmation), and an About screen. |
| 🧩 **Mod packs** | Drop a `levels/*.mrg` file in and the build spits out a separate ROM with the mod name baked onto the menu. |

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

### 2 · Build

```bash
make            # one ROM per levels/*.mrg  →  ReGravity_Defied_<mod>.gba
make assets     # regenerate src/gd_assets.h + src/gd_sound.h from assets/
make debug      # build with mGBA debug logging (-DDEBUG)
make clean      # remove build artifacts
```

### 3 · Play

Run a `ReGravity_Defied_*.gba` in any GBA emulator (**mGBA**, VisualBoyAdvance, no$gba)
or on real hardware via a flashcart. SRAM saves require an emulator/flashcart with save support.

> 💡 **WSL tip:** if a Windows emulator has the ROM open, the `.gba` file is locked and
> `make` can't overwrite it — close the emulator before rebuilding.

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

```
src/
├── main.c        game loop, state machine, menus, progression
├── physics.c     bike physics + rider rendering
├── graphics.c    Mode 3 drawing primitives + back buffer
├── level.c       track decoding and rendering
├── sound.c       DirectSound SFX playback
├── save.c        SRAM save/load (progress, best times, last-track cursor)
├── crt0.s        startup code
gba.ld            linker script
assets/           source PNGs + sound/; tools/convert_*.py bake them into headers
levels/*.mrg      track-data mod packs — one ROM is built per file
```

---

## 🛠️ How the Mod Build Works

Each `levels/<name>.mrg` is compiled into its own `build/<name>/` directory with
`MOD_NAME` baked in, embedded via `objcopy`, and linked into a standalone
`ReGravity_Defied_<name>.gba`. The mod name shows up on the league screen, so you
can ship a `classic` ROM, a `1000`-track ROM, or your own pack side by side.

---

## 📜 License

Open source, released for **educational purposes**. This is a fan reimplementation of
*Gravity Defied* — the original game and its assets belong to their respective owners.

<div align="center">
<sub>Built with C, fixed-point math, and a lot of respect for the 16.78 MHz ARM7TDMI.</sub>
</div>
