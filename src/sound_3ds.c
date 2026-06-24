#include <3ds.h>
#include <stdlib.h>
#include <string.h>
#include "sound.h"
#include "gd_sound.h"

// Nintendo 3DS sound backend. The 3DS audio DSP is driven through libctru's ndsp
// service: one channel plays the Wilhelm scream sample on a crash. The DSP reads
// from a linear-memory copy of the clip (flushed from the CPU cache once), so
// playback is fully asynchronous and there is nothing to service per frame.
//
// ndspInit needs the DSP firmware present on the system; if it is unavailable the
// backend silently disables itself (the crash SFX is non-essential) so the game
// still runs.

static int ok;                 // 1 once ndsp + the sample buffer are ready
static ndspWaveBuf wbuf;       // describes the clip to the DSP
static u8* lbuf;               // linear-memory copy of wilhelm_pcm for DSP DMA

void sound_init(void) {
    if (R_FAILED(ndspInit())) { ok = 0; return; }

    ndspSetOutputMode(NDSP_OUTPUT_MONO);
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_NONE);
    ndspChnSetRate(0, (float)SOUND_SAMPLE_RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM8);
    float mix[12] = {0};
    mix[0] = mix[1] = 1.0f;    // full volume, both output channels
    ndspChnSetMix(0, mix);

    lbuf = (u8*)linearAlloc(WILHELM_LEN);
    if (!lbuf) { ndspExit(); ok = 0; return; }
    memcpy(lbuf, wilhelm_pcm, WILHELM_LEN);
    DSP_FlushDataCache(lbuf, WILHELM_LEN);

    memset(&wbuf, 0, sizeof(wbuf));
    wbuf.data_vaddr = lbuf;
    wbuf.nsamples   = WILHELM_LEN;   // signed 8-bit PCM, 1 byte = 1 sample

    atexit(ndspExit);
    ok = 1;
}

void sound_play_crash(void) {
    if (!ok) return;
    // Restart from the beginning: drop any in-flight playback and re-queue.
    ndspChnWaveBufClear(0);
    wbuf.status = NDSP_WBUF_FREE;
    DSP_FlushDataCache(lbuf, WILHELM_LEN);
    ndspChnWaveBufAdd(0, &wbuf);
}

void sound_tick(void) {
    // Nothing to do: the DSP owns playback once the wave buffer is queued.
}
