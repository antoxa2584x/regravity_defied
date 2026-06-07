# GBA Hello World Project (WSL)

This is a minimal Game Boy Advance project designed to be built in WSL using the `arm-none-eabi-gcc` toolchain.

## Prerequisites (WSL)

Install the ARM cross-compiler:

```bash
sudo apt update
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi make
```

## Building

Run `make` to build the project:

```bash
make
```

This will produce `hello_world.gba`.

## Project Structure

- `src/main.c`: The main game logic (Mode 3 drawing).
- `src/crt0.s`: Startup code and GBA header.
- `gba.ld`: Linker script.
- `Makefile`: Build instructions.

## Running

You can run `hello_world.gba` in any GBA emulator (e.g., mGBA, VisualBoyAdvance).

If you have an emulator installed in Windows, you can point it to this file in the WSL filesystem (e.g., `\\wsl$\Ubuntu\home\...`).
