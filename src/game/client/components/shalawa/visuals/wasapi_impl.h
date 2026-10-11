#ifndef SHALAWA_WASAPI_IMPL_H
#define SHALAWA_WASAPI_IMPL_H

struct SWasapiImpl
{
    void *pEnumerator;
    void *pDevice;
    void *pAudioClient;
    void *pCaptureClient;
    int channels;
    int bitsPerSample;
};

bool Wasapi_Init(SWasapiImpl *impl);
void Wasapi_Shutdown(SWasapiImpl *impl);
bool Wasapi_GetNextBuffer(SWasapiImpl *impl, float *outSamples, int maxSamples, int *outCount);

#endif