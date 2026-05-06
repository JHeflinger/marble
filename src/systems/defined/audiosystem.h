#ifndef AUDIOSYSTEM_H
#define AUDIOSYSTEM_H

#include "systems/system.h"
#include "audio/dsp.h"
#include "audio/spatial.h"

#define NUM_GHOST_AUDIO 3

System* GenerateAudioSystem();

void DSPProcessorCallback0(void* b, unsigned int f);
void DSPProcessorCallback1(void* b, unsigned int f);
void DSPProcessorCallback2(void* b, unsigned int f);

#endif
