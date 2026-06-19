#include <nds.h>
#include "sound.h"
#include "gd_sound.h"

// NDS sound backend. The DS sound hardware is driven by the ARM7; libnds'
// soundPlaySample() forwards the request over the FIFO, so playback runs
// asynchronously and there is no per-frame servicing to do (unlike the GBA
// DirectSound FIFO, which sound_gba.c hand-feeds).

void sound_init(void) {
    soundEnable();
}

void sound_play_crash(void) {
    // wilhelm_pcm is signed 8-bit PCM at SOUND_SAMPLE_RATE; WILHELM_LEN is the
    // real audio length (the buffer has a trailing-silence guard beyond it).
    // Full volume, centre pan, no loop.
    soundPlaySample(wilhelm_pcm, SoundFormat_8Bit, WILHELM_LEN,
                    SOUND_SAMPLE_RATE, 127, 64, false, 0);
}

void sound_tick(void) {
    // Nothing to do: the ARM7 owns playback once the sample is queued.
}
