#include "dsp.h"

#include <easymemory.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct DSPState {
    DelayLine delayLine;
    Tap tapBuffers[2][DSP_MAX_TAPS]; // double buffer bc audio is a separate thread with race conds
    size_t tapCounts[2];
    _Atomic int currentBuffer;
};

DSPState* DSPInit(int sampleRate, float maxDelaySecs) {
    DSPState* state = EZ_ALLOC(1, sizeof(DSPState));
    if (!state) return NULL;

    size_t capacity = (size_t)((float)sampleRate * maxDelaySecs);
    if (capacity < 1) capacity = 1;

    state->delayLine.samples = EZ_ALLOC(capacity, sizeof(float));
    if (!state->delayLine.samples) {
        EZ_FREE(state);
        return NULL;
    }
    state->delayLine.capacity = capacity;
    state->delayLine.writeIndex = 0;
    state->delayLine.sampleRate = sampleRate;

    state->tapCounts[0] = 0;
    state->tapCounts[1] = 0;
    atomic_store(&state->currentBuffer, 0);

    return state;
}

void DSPDestroy(DSPState* state) {
    if (!state) return;
    EZ_FREE(state->delayLine.samples);
    EZ_FREE(state);
}

void DSPSubmitTaps(DSPState* state, const Tap* taps, size_t numTaps) {
    if (!state) return;
    if (numTaps > DSP_MAX_TAPS) numTaps = DSP_MAX_TAPS;

    int current = atomic_load(&state->currentBuffer);
    int back = 1 - current;

    if (numTaps > 0 && taps) {
        memcpy(state->tapBuffers[back], taps, numTaps * sizeof(Tap));
    }
    state->tapCounts[back] = numTaps;

    atomic_store(&state->currentBuffer, back);
}

void DSPProcess(DSPState* state, float* buffer, unsigned int frames, unsigned int channels) {
    if (!state || !buffer || channels == 0) return;

    DelayLine* dl = &state->delayLine;

    // swap double-buffer bc audio is a separate thread to the game
    int idx = atomic_load(&state->currentBuffer);
    const Tap* taps = state->tapBuffers[idx];
    size_t numTaps = state->tapCounts[idx];

    for (unsigned int f = 0; f < frames; f++) {
        // downmix incoming frame to a dry sample (for mono, this is no-op for now)
        float dry = 0.0f;
        for (unsigned int c = 0; c < channels; c++) {
            dry += buffer[f * channels + c];
        }
        dry /= (float)channels;

        // write dry sample into buffer
        dl->samples[dl->writeIndex] = dry;

        // sum of contributions over taps
        float wet = 0.0f;
        for (size_t t = 0; t < numTaps; t++) {
            size_t delaySamples = (size_t)(taps[t].delay_seconds * (float)dl->sampleRate);
            if (delaySamples >= dl->capacity) delaySamples = dl->capacity - 1;

            // walk backward from index
            size_t readIndex = (dl->writeIndex + dl->capacity - delaySamples) % dl->capacity;
            wet += dl->samples[readIndex] * taps[t].amplitude;
        }

        // to be replaced with hrtf later
        // wet output (single ray used in pathtrace to do dry later)
        float out = wet; // todo: consider normalization for deeper sounds
        for (unsigned int c = 0; c < channels; c++) {
            buffer[f * channels + c] = out;
        }

        dl->writeIndex = (dl->writeIndex + 1) % dl->capacity;
    }
}
