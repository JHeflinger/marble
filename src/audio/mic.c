#include "mic.h"
#include <raylib.h>
#include <miniaudio.h>
#include <stdio.h>
#include <math.h>

float g_mic_volume = 0.0f;
ma_device g_mic_device;

float MicVolume() {
    return g_mic_volume;
}

static void mic_data_callback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
    (void)device;
    (void)output;
    if (input == NULL) {
        g_mic_volume = 0.0f;
        return;
    }
    const float* samples = (const float*)input;
    float sum = 0.0f;
    for (ma_uint32 i = 0; i < frameCount; i++) {
        float s = samples[i];
        sum += s * s;
    }
    float rms = sqrtf(sum / (float)frameCount);
    if (rms > 1.0f)
        rms = 1.0f;
    g_mic_volume = rms;
}

BOOL InitMic() {
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate = 44100;
    config.dataCallback = mic_data_callback;
    if (ma_device_init(NULL, &config, &g_mic_device) != MA_SUCCESS) return FALSE;
    if (ma_device_start(&g_mic_device) != MA_SUCCESS) {
        ma_device_uninit(&g_mic_device);
        return FALSE;
    }
    return TRUE;
}

void CloseMic() {
    ma_device_uninit(&g_mic_device);
}
