#include "gba.h"
#include "save.h"

// GBA storage backend: battery-backed cartridge SRAM. SRAM lives at 0x0E000000
// and must be accessed one byte at a time.
#define SRAM ((volatile uint8_t*)0x0E000000)

// Emulators/flashcarts enable 32 KB SRAM when this marker is present in the ROM.
__attribute__((used)) static const char sram_sig[] = "SRAM_V113";

void save_backend_read(void* dst, int n) {
    uint8_t* d = dst;
    for (int i = 0; i < n; i++) d[i] = SRAM[i];
}

void save_backend_write(const void* src, int n) {
    const uint8_t* s = src;
    for (int i = 0; i < n; i++) SRAM[i] = s[i];
}
