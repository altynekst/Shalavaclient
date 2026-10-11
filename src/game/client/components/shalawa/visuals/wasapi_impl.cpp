#include "wasapi_impl.h"
#include <cstring>

#ifdef _WIN32

#include <windows.h>
#include <objbase.h>

static const CLSID s_CLSID_MMDeviceEnumerator = {
    0xBCDE0395,0xE52F,0x467C,{0x8E,0x3D,0xC4,0x57,0x92,0x91,0x69,0x2E}
};
static const IID s_IID_IMMDeviceEnumerator = {
    0xA95664D2,0x9614,0x4F35,{0xA7,0x46,0xDE,0x8D,0xB6,0x36,0x17,0xE6}
};
static const IID s_IID_IAudioClient = {
    0x1CB9AD4C,0xDBFA,0x4C32,{0xB1,0x78,0xC2,0xF5,0x68,0xA7,0x03,0xB2}
};
static const IID s_IID_IAudioCaptureClient = {
    0xC8ADBD64,0xE71E,0x48A0,{0xA4,0xDE,0x18,0x5C,0x39,0x5C,0xD3,0x17}
};

#define AUDCLNT_STREAMFLAGS_LOOPBACK 0x00020000u
#define AUDCLNT_SHAREMODE_SHARED 0
#define AUDCLNT_BUFFERFLAGS_SILENT 0x00000002u

#pragma pack(push,1)
typedef struct {
    WORD  wFormatTag;
    WORD  nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD  nBlockAlign;
    WORD  wBitsPerSample;
    WORD  cbSize;
} WFX;
#pragma pack(pop)

typedef HRESULT (STDMETHODCALLTYPE *FnQueryInterface)(void*, const IID*, void**);
typedef ULONG   (STDMETHODCALLTYPE *FnAddRef)(void*);
typedef ULONG   (STDMETHODCALLTYPE *FnRelease)(void*);

typedef struct {
    FnQueryInterface QueryInterface;
    FnAddRef         AddRef;
    FnRelease        Release;
    void *EnumAudioEndpoints;
    HRESULT (STDMETHODCALLTYPE *GetDefaultAudioEndpoint)(void*, DWORD, DWORD, void**);
    void *GetDevice;
    void *RegisterEndpointNotificationCallback;
    void *UnregisterEndpointNotificationCallback;
} VtblEnum;

typedef struct {
    FnQueryInterface QueryInterface;
    FnAddRef         AddRef;
    FnRelease        Release;
    HRESULT (STDMETHODCALLTYPE *Activate)(void*, const IID*, DWORD, void*, void**);
    void *OpenPropertyStore;
    void *GetId;
    void *GetState;
} VtblDev;

typedef struct {
    FnQueryInterface QueryInterface;
    FnAddRef         AddRef;
    FnRelease        Release;
    HRESULT (STDMETHODCALLTYPE *Initialize)(void*, int, DWORD, LONGLONG, LONGLONG, const WFX*, const GUID*);
    void *GetBufferSize;
    void *GetStreamLatency;
    void *GetCurrentPadding;
    void *IsFormatSupported;
    HRESULT (STDMETHODCALLTYPE *GetMixFormat)(void*, WFX**);
    void *GetDevicePeriod;
    HRESULT (STDMETHODCALLTYPE *Start)(void*);
    HRESULT (STDMETHODCALLTYPE *Stop)(void*);
    void *Reset;
    void *SetEventHandle;
    HRESULT (STDMETHODCALLTYPE *GetService)(void*, const IID*, void**);
} VtblClient;

typedef struct {
    FnQueryInterface QueryInterface;
    FnAddRef         AddRef;
    FnRelease        Release;
    HRESULT (STDMETHODCALLTYPE *GetBuffer)(void*, BYTE**, UINT32*, DWORD*, UINT64*, UINT64*);
    HRESULT (STDMETHODCALLTYPE *ReleaseBuffer)(void*, UINT32);
    HRESULT (STDMETHODCALLTYPE *GetNextPacketSize)(void*, UINT32*);
} VtblCapture;

typedef struct { VtblEnum    *v; } ObjEnum;
typedef struct { VtblDev     *v; } ObjDev;
typedef struct { VtblClient  *v; } ObjClient;
typedef struct { VtblCapture *v; } ObjCapture;

#define ENUM(p)    ((ObjEnum*)(p))
#define DEV(p)     ((ObjDev*)(p))
#define CLI(p)     ((ObjClient*)(p))
#define CAP(p)     ((ObjCapture*)(p))

static void SafeRelease(void *p)
{
    if(p) ((ObjEnum*)p)->v->Release(p);
}

bool Wasapi_Init(SWasapiImpl *impl)
{
    memset(impl, 0, sizeof(*impl));
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    void *pEnum = nullptr;
    HRESULT hr = CoCreateInstance(s_CLSID_MMDeviceEnumerator, nullptr,
        CLSCTX_ALL, s_IID_IMMDeviceEnumerator, &pEnum);
    if(FAILED(hr)) return false;

    void *pDev = nullptr;
    hr = ENUM(pEnum)->v->GetDefaultAudioEndpoint(pEnum, 0, 0, &pDev);
    if(FAILED(hr)) { SafeRelease(pEnum); return false; }

    void *pClient = nullptr;
    hr = DEV(pDev)->v->Activate(pDev, &s_IID_IAudioClient, CLSCTX_ALL, nullptr, &pClient);
    if(FAILED(hr)) { SafeRelease(pDev); SafeRelease(pEnum); return false; }

    WFX *pwfx = nullptr;
    hr = CLI(pClient)->v->GetMixFormat(pClient, &pwfx);
    if(FAILED(hr) || !pwfx)
    {
        SafeRelease(pClient); SafeRelease(pDev); SafeRelease(pEnum);
        return false;
    }

    impl->channels      = pwfx->nChannels;
    impl->bitsPerSample = pwfx->wBitsPerSample;

    hr = CLI(pClient)->v->Initialize(pClient,
        AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
        200000, 0, pwfx, nullptr);
    CoTaskMemFree(pwfx);
    if(FAILED(hr)) { SafeRelease(pClient); SafeRelease(pDev); SafeRelease(pEnum); return false; }

    void *pCapture = nullptr;
    hr = CLI(pClient)->v->GetService(pClient, &s_IID_IAudioCaptureClient, &pCapture);
    if(FAILED(hr)) { SafeRelease(pClient); SafeRelease(pDev); SafeRelease(pEnum); return false; }

    hr = CLI(pClient)->v->Start(pClient);
    if(FAILED(hr))
    {
        SafeRelease(pCapture); SafeRelease(pClient);
        SafeRelease(pDev); SafeRelease(pEnum);
        return false;
    }

    impl->pEnumerator   = pEnum;
    impl->pDevice       = pDev;
    impl->pAudioClient  = pClient;
    impl->pCaptureClient = pCapture;
    return true;
}

void Wasapi_Shutdown(SWasapiImpl *impl)
{
    if(!impl) return;
    if(impl->pAudioClient) CLI(impl->pAudioClient)->v->Stop(impl->pAudioClient);
    SafeRelease(impl->pCaptureClient); impl->pCaptureClient = nullptr;
    SafeRelease(impl->pAudioClient);   impl->pAudioClient   = nullptr;
    SafeRelease(impl->pDevice);        impl->pDevice        = nullptr;
    SafeRelease(impl->pEnumerator);    impl->pEnumerator    = nullptr;
}

bool Wasapi_GetNextBuffer(SWasapiImpl *impl, float *outSamples, int maxSamples, int *outCount)
{
    *outCount = 0;
    if(!impl || !impl->pCaptureClient) return false;

    ObjCapture *pCap = CAP(impl->pCaptureClient);
    UINT32 packetLength = 0;
    if(FAILED(pCap->v->GetNextPacketSize(pCap, &packetLength))) return false;
    if(packetLength == 0) return true;

    BYTE *pData = nullptr;
    UINT32 numFrames = 0;
    DWORD flags = 0;
    if(FAILED(pCap->v->GetBuffer(pCap, &pData, &numFrames, &flags, nullptr, nullptr))) return false;

    int ch   = impl->channels < 1 ? 2 : impl->channels;
    int bits = impl->bitsPerSample;
    int copy = ((int)numFrames < maxSamples) ? (int)numFrames : maxSamples;

    if(!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && pData && copy > 0)
    {
        if(bits == 32)
        {
            const float *f32 = (const float*)pData;
            for(int i = 0; i < copy; i++)
            {
                float s = 0;
                for(int c = 0; c < ch; c++) s += f32[i * ch + c];
                outSamples[i] = s / ch;
            }
        }
        else if(bits == 16)
        {
            const short *s16 = (const short*)pData;
            for(int i = 0; i < copy; i++)
            {
                float s = 0;
                for(int c = 0; c < ch; c++) s += s16[i * ch + c] / 32768.0f;
                outSamples[i] = s / ch;
            }
        }
        *outCount = copy;
    }

    pCap->v->ReleaseBuffer(pCap, numFrames);
    return true;
}

#else

bool Wasapi_Init(SWasapiImpl *impl) { memset(impl, 0, sizeof(*impl)); return false; }
void Wasapi_Shutdown(SWasapiImpl *) {}
bool Wasapi_GetNextBuffer(SWasapiImpl *, float *, int, int *count) { *count = 0; return false; }

#endif