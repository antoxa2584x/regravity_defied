#include "gba.h"
#include "sound.h"
#include "gd_sound.h"

// Timer 0 (the game clock) runs at 16.78MHz/1024 = 16384 Hz; we reuse it as a
// wall-clock to know when the clip has finished, independent of frame rate.
#define TIMER0_HZ 16384
#define PLAY_TICKS ((int)((long long)WILHELM_LEN * TIMER0_HZ / SOUND_SAMPLE_RATE))

static int playing = 0;
static uint16_t play_start;   // Timer 0 reading when playback began

void sound_init(void) {
    REG_SOUNDBIAS  = 0x0200;   // default output bias
    REG_SOUNDCNT_X = 0x0080;   // master sound enable (must precede CNT_H writes)
    // DirectSound A: 100% volume, output to both speakers, clocked by Timer 1.
    REG_SOUNDCNT_H = 0x0004 | 0x0100 | 0x0200 | 0x0400;
}

void sound_play_crash(void) {
    // Stop any current playback and rewind to the start of the sample.
    REG_DMA1CNT_H  = 0;
    REG_SOUNDCNT_H |= 0x0800;  // reset FIFO A (auto-clears)
    REG_TM1CNT_H   = 0;

    REG_DMA1SAD   = (uint32_t)wilhelm_pcm;
    REG_DMA1DAD   = REG_FIFO_A;
    REG_DMA1CNT_H = DMA_SOUND_FIFO;   // wait for FIFO requests, 4 words each

    // Timer 1 overflows once per output sample to pace the FIFO.
    REG_TM1CNT_L = 65536 - (16777216 / SOUND_SAMPLE_RATE);
    REG_TM1CNT_H = TM_ENABLE;         // prescaler /1 (counts CPU cycles)

    play_start = REG_TM0CNT_L;
    playing = 1;
}

void sound_tick(void) {
    if (!playing) return;
    if ((uint16_t)(REG_TM0CNT_L - play_start) >= PLAY_TICKS) {
        REG_DMA1CNT_H = 0;            // stop feeding the FIFO past the buffer end
        REG_TM1CNT_H = 0;
        playing = 0;
    }
}
