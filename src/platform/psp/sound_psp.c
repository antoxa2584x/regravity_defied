#include <pspkernel.h>
#include <pspaudio.h>
#include <stdlib.h>
#include "sound.h"
#include "gd_sound.h"

// Sony PSP sound backend. Unlike the DS/3DS, where queuing a sample to the audio
// hardware returns immediately and playback runs on its own, the PSP's audio
// output calls (sceAudioSRCOutputBlocking) block the calling thread until the
// chunk has been consumed. So a dedicated worker thread owns playback: the game
// thread just raises a flag on a crash (sound_play_crash), and the worker streams
// the Wilhelm scream through the sample-rate-converter channel, which resamples
// our SOUND_SAMPLE_RATE clip up to the PSP's 44.1 kHz output.
//
// The clip is converted once at init from signed 8-bit mono to the 16-bit stereo
// frames sceAudioSRCOutputBlocking expects, padded up to a whole number of output
// chunks so the final Output call never reads past the buffer. If anything in the
// bring-up fails the backend silently disables itself (the SFX is non-essential),
// mirroring the 3DS backend.

#define CHUNK 1024                                  // frames per SRC output call (multiple of 64)
#define FRAMES (((WILHELM_LEN + CHUNK - 1) / CHUNK) * CHUNK)  // clip rounded up to whole chunks

// Input rate handed to the sample-rate converter. The clip is sampled at
// SOUND_SAMPLE_RATE (16384 Hz, a GBA-timer-friendly power of two), but the PSP's
// SRC channel only accepts a fixed set of standard rates — 8000/11025/12000/
// 16000/22050/24000/32000/44100/48000 — and rejects 16384, which made
// sceAudioSRCChReserve fail and left the crash SFX silent. 16000 is the nearest
// supported rate; the resulting ~2.4% pitch shift on a one-shot scream is inaudible.
#define PSP_SRC_RATE 16000

static short* g_clip;           // 16-bit stereo, SOUND_SAMPLE_RATE, FRAMES frames (zero-padded tail)
static volatile int g_play;     // set by sound_play_crash, consumed by the worker
static int g_ok;                // 1 once the clip and worker thread are ready

static int audio_thread(SceSize args, void* argp) {
    (void)args; (void)argp;
    for (;;) {
        // Poll for a play request. A 2 ms idle poll keeps latency inaudible while
        // costing almost nothing when no crash is playing.
        while (!g_play) sceKernelDelayThread(2000);
        g_play = 0;

        // Reserve the SRC channel at a PSP-supported rate (see PSP_SRC_RATE); the
        // hardware converts it up to its native 44.1 kHz output. Skip this
        // playthrough if the channel is unavailable.
        if (sceAudioSRCChReserve(CHUNK, PSP_SRC_RATE, 2) < 0) continue;
        for (int frame = 0; frame < FRAMES; frame += CHUNK)
            sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX, g_clip + frame * 2);
        sceAudioSRCChRelease();
    }
    return 0;
}

void sound_init(void) {
    // Convert the clip to padded 16-bit stereo once. wilhelm_pcm is signed 8-bit
    // mono at SOUND_SAMPLE_RATE; widen each sample to 16-bit (<<8) and duplicate it
    // to both channels. Frames beyond WILHELM_LEN stay zero (silence padding).
    g_clip = (short*)calloc(FRAMES * 2, sizeof(short));
    if (!g_clip) { g_ok = 0; return; }
    for (int i = 0; i < WILHELM_LEN; i++) {
        short s = (short)((int)wilhelm_pcm[i] << 8);
        g_clip[i * 2]     = s;
        g_clip[i * 2 + 1] = s;
    }

    int thid = sceKernelCreateThread("audio_thread", audio_thread, 0x12, 0x1000, 0, 0);
    if (thid < 0) { free(g_clip); g_clip = 0; g_ok = 0; return; }
    sceKernelStartThread(thid, 0, 0);
    g_ok = 1;
}

void sound_play_crash(void) {
    // Raise the flag; the worker thread plays the clip from the start. A crash hold
    // (~2 s) is longer than the clip (~1 s), so playback always finishes before the
    // next crash could retrigger it.
    if (g_ok) g_play = 1;
}

void sound_tick(void) {
    // Nothing to do: the worker thread owns playback once the flag is raised.
}
