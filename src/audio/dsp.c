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

    // atomic safe listener directions
    _Atomic float dirX;
    _Atomic float dirY;
    _Atomic float dirZ;
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

    atomic_store(&state->dirX, 0.0f);
    atomic_store(&state->dirY, 0.0f);
    atomic_store(&state->dirZ, -1.0f);

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

void DSPProcess(DSPState* state, float* monoOut, unsigned int frames) {
    if (!state || !monoOut) return;

    DelayLine* dl = &state->delayLine;

    int idx = atomic_load(&state->currentBuffer);
    const Tap* taps = state->tapBuffers[idx];
    size_t numTaps = state->tapCounts[idx];

    for (unsigned int f = 0; f < frames; f++) {
        float dry = monoOut[f];

        // write dry sample into buffer
        dl->samples[dl->writeIndex] = dry;

        // sum contributions from all taps
        float wet = 0.0f;
        for (size_t t = 0; t < numTaps; t++) {
            size_t delaySamples = (size_t)(taps[t].delay_seconds * (float)dl->sampleRate);
            if (delaySamples >= dl->capacity) delaySamples = dl->capacity - 1;

            size_t readIndex = (dl->writeIndex + dl->capacity - delaySamples) % dl->capacity;
            wet += dl->samples[readIndex] * taps[t].amplitude;
        }

        monoOut[f] = wet;

        dl->writeIndex = (dl->writeIndex + 1) % dl->capacity;
    }
}

void DSPGetDirection(DSPState* state, float* outX, float* outY, float* outZ) {
    if (!state) {
        *outX = 0.0f; *outY = 0.0f; *outZ = -1.0f;
        return;
    }
    *outX = atomic_load(&state->dirX);
    *outY = atomic_load(&state->dirY);
    *outZ = atomic_load(&state->dirZ);
}

void DSPSubmitDirection(DSPState* state, float x, float y, float z) {
    if (!state) return;
    atomic_store(&state->dirX, x);
    atomic_store(&state->dirY, y);
    atomic_store(&state->dirZ, z);
}
