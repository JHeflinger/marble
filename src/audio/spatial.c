#include "spatial.h"

#include <phonon.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <easyobjects.h>

/*
 * manages how HRTF is applied
 * - uses working buffers to transfer samples, reused in SpatialApply
 * - after debugging found that raylib's variable framerate means frame size is now 1
 * - manages multiple spatial sources
 */

static IPLContext       g_context     = NULL;
static IPLHRTF          g_hrtf        = NULL;
static IPLAudioSettings g_audioSettings;
static IPLAudioBuffer   g_inBuffer;
static IPLAudioBuffer   g_outBuffer;
static int              g_initialized = 0;

struct SpatialSource {
    IPLBinauralEffect effect;
};

int SpatialInit(int sampleRate, int frameSize) {
    if (g_initialized) return 1;

    IPLContextSettings ctxSettings = { 0 };
    ctxSettings.version = STEAMAUDIO_VERSION;
    if (iplContextCreate(&ctxSettings, &g_context) != IPL_STATUS_SUCCESS) {
        return 0;
    }

    g_audioSettings.samplingRate = sampleRate;
    g_audioSettings.frameSize    = frameSize;

    IPLHRTFSettings hrtfSettings = { 0 };
    hrtfSettings.type   = IPL_HRTFTYPE_DEFAULT;
    hrtfSettings.volume = 1.0f;
    if (iplHRTFCreate(g_context, &g_audioSettings, &hrtfSettings, &g_hrtf) != IPL_STATUS_SUCCESS) {
        iplContextRelease(&g_context);
        return 0;
    }

    if (iplAudioBufferAllocate(g_context, 1, frameSize, &g_inBuffer) != IPL_STATUS_SUCCESS) {
        iplHRTFRelease(&g_hrtf);
        iplContextRelease(&g_context);
        return 0;
    }
    if (iplAudioBufferAllocate(g_context, 2, frameSize, &g_outBuffer) != IPL_STATUS_SUCCESS) {
        iplAudioBufferFree(g_context, &g_inBuffer);
        iplHRTFRelease(&g_hrtf);
        iplContextRelease(&g_context);
        return 0;
    }

    g_initialized = 1;
    return 1;
}

void SpatialShutdown(void) {
    if (!g_initialized) return;

    iplAudioBufferFree(g_context, &g_outBuffer);
    iplAudioBufferFree(g_context, &g_inBuffer);
    iplHRTFRelease(&g_hrtf);
    iplContextRelease(&g_context);

    g_context     = NULL;
    g_hrtf        = NULL;
    g_initialized = 0;
}

SpatialSource* SpatialCreateSource(void) {
    if (!g_initialized) return NULL;

    SpatialSource* source = (SpatialSource*)EZ_ALLOC(1, sizeof(SpatialSource));
    if (!source) return NULL;

    IPLBinauralEffectSettings effectSettings = { 0 };
    effectSettings.hrtf = g_hrtf;

    if (iplBinauralEffectCreate(g_context, &g_audioSettings, &effectSettings, &source->effect) != IPL_STATUS_SUCCESS) {
        EZ_FREE(source);
        return NULL;
    }

    return source;
}

void SpatialDestroySource(SpatialSource* source) {
    if (!source) return;
    if (source->effect) iplBinauralEffectRelease(&source->effect);
    EZ_FREE(source);
}

void SpatialApply(SpatialSource* source,
                  const float* monoIn,
                  float* stereoOut,
                  float dirX, float dirY, float dirZ,
                  unsigned int frames) {
    if (!source || !g_initialized) return;

    IPLBinauralEffectParams params = { 0 };
    params.direction     = (IPLVector3){ dirX, dirY, dirZ };
    params.interpolation = IPL_HRTFINTERPOLATION_NEAREST;
    params.spatialBlend  = 1.0f;
    params.hrtf          = g_hrtf;
    params.peakDelays    = NULL;

    for (unsigned int f = 0; f < frames; f++) {
        g_inBuffer.data[0][0] = monoIn[f];

        iplBinauralEffectApply(source->effect, &params, &g_inBuffer, &g_outBuffer);

        stereoOut[f * 2 + 0] = g_outBuffer.data[0][0];
        stereoOut[f * 2 + 1] = g_outBuffer.data[1][0];
    }
}