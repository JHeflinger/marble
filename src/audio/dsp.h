#ifndef DSP_H
#define DSP_H

#include <stddef.h>
#include "tap.h"

/*
 * one DelayLine per source, one DSPState per resource
 * this file is formatting so that when we move away from raylib, this is still consistent for the backend to consume
 *
 * overall idea:
 * - path tracer dumps taps found into DSPState
 * - raylib calls DSPProcess as a callback to populate the buffer it needs
 */

#define DSP_MAX_TAPS 2048

typedef struct {
    float* samples; // circular buffer, past dry samples
    size_t capacity;
    size_t writeIndex;
    int sampleRate;
} DelayLine;

typedef struct DSPState DSPState; // in .c

DSPState* DSPInit(int sampleRate, float maxDelaySecs);

void DSPDestroy(DSPState* state);

void DSPSubmitTaps(DSPState* state, const Tap* taps, size_t numTaps);

/*
 * process a buffer of audio in place
 * should be called by audio backend as callback
 *
 * for each input frame, this writes the dry sample into the delay line, then
 * reads each tap's delayed sample and sums them into the output. Output is
 * just the wet and processing mostly happens in audiosystem
 */
void DSPProcess(DSPState* state, float* monoOut, unsigned int frames);

void DSPGetDirection(DSPState* state, float* outX, float* outY, float* outZ);

void DSPSubmitDirection(DSPState* state, float x, float y, float z);
#endif //DSP_H
