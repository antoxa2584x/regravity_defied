#include "gba.h"
#include "sound.h"
#include "gd_sound.h"

// Timer 0 (the game clock) runs at 16.78MHz/1024 = 16384 Hz; we reuse it as a
// wall-clock to know when the clip has finished, independent of frame rate.
#define TIMER0_HZ 16384
#define PLAY_TICKS ((int)((long long)WILHELM_LEN * TIMER0_HZ / SOUND_SAMPLE_RATE))

static int playing = 0;
static int draining = 0;      // clocking silence through the DAC before stopping
static uint16_t play_start;   // Timer 0 reading when playback began

// Direct FIFO A write port (each 32-bit word = 4 signed 8-bit samples).
#define FIFO_A (*(volatile uint32_t*)REG_FIFO_A)

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
    draining = 0;
}

void sound_tick(void) {
    if (draining) {
        // The silence written last frame has been clocked out, so the DAC now
        // sits at zero. Safe to fully stop without a pop.
        REG_DMA1CNT_H = 0;
        REG_TM1CNT_H  = 0;
        draining = 0;
        return;
    }
    if (!playing) return;
    if ((uint16_t)(REG_TM0CNT_L - play_start) >= PLAY_TICKS) {
        // End of clip. If we just stopped Timer 1 here, the DAC would hold the
        // last (non-zero) sample as a DC offset — an audible pop. Instead stop
        // the DMA, flush the FIFO, and push a few zero samples so the next
        // values clocked to the DAC are silence. Timer 1 keeps running this
        // frame to consume them; the hard stop happens next tick (draining).
        REG_DMA1CNT_H  = 0;          // stop feeding the FIFO from the sample buffer
        REG_SOUNDCNT_H |= 0x0800;    // reset FIFO A (auto-clears)
        FIFO_A = 0;                  // 8 samples of silence — plenty to span one
        FIFO_A = 0;                  // frame at 16 kHz before the next tick stops
        playing = 0;
        draining = 1;
    }
}
