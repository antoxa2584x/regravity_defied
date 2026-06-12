#ifndef SOUND_H
#define SOUND_H

// DirectSound one-shot SFX (channel A, fed by DMA1, clocked by Timer 1).
void sound_init(void);
// Play the Wilhelm scream from the start (used on a crash).
void sound_play_crash(void);
// Call once per rendered frame: stops playback when the clip has finished so
// the FIFO is not fed past the end of the sample buffer.
void sound_tick(void);

#endif // SOUND_H
