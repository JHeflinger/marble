#ifndef TAP_H
#define TAP_H

//
// Created by Patback on 4/27/2026.
//

/**
 * Tap = a delayed attenuated copy of a source
 * every successful path found by path tracer to audio source -> 1 tap
 */
typedef struct {
    float delay_seconds;
    float amplitude;
} Tap;

#endif
