#ifndef SPATIAL_H
#define SPATIAL_H

#include <stddef.h>

/*
 * manages HRTF application
 *
 * takes taps derived from pathtracer and dsp layer and creates a spatial effect
 * lifecycle: SpatialInit -> SpatialCreateSource per audio source -> SpatialApply on audio buffer
 * SpatialApply must only be called in a raylib callback
 *
 * note that phonon assumes the opposite-handed system (negate x)
 */

typedef struct SpatialSource SpatialSource;

int SpatialInit(int sampleRate, int frameSize);

void SpatialShutdown(void);

SpatialSource* SpatialCreateSource(void);

void SpatialDestroySource(SpatialSource* source);

void SpatialApply(SpatialSource* source,
                  const float* monoIn,
                  float* stereoOut,
                  float dirX, float dirY, float dirZ,
                  unsigned int frames);

#endif //SPATIAL_H