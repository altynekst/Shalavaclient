#include "audio_capture.h"
#include "wasapi_impl.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <thread>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

CAudioCapture::CAudioCapture()
    : m_Capturing(false), m_ShouldStop(false)
{
    m_Initialized = false;
    m_pImpl = nullptr;
    m_Bass = m_Mid = m_Treble = m_Volume = 0;
    m_TrackInfoReady.store(false);
    m_PendingValid = false;
    memset(m_PendingTitle, 0, sizeof(m_PendingTitle));
    memset(m_PendingArtist, 0, sizeof(m_PendingArtist));
    m_BeatAvgEnergy = 0; m_BeatThreshold = 1.5f;
    m_BeatCooldown = 0; m_BeatIntensity = 0;
    m_Beat = false;
    m_FFTPos = 0;
    memset(m_RawSpectrum, 0, sizeof(m_RawSpectrum));
    memset(m_TrackArtist, 0, sizeof(m_TrackArtist));
    memset(m_SmoothSpectrum, 0, sizeof(m_SmoothSpectrum));
    memset(m_Waveform, 0, sizeof(m_Waveform));
    m_HasTrackInfo = false;
    m_TrackInfoTimer = 0.0f;
    memset(m_RawWaveform, 0, sizeof(m_RawWaveform));
    memset(m_FFTBuffer, 0, sizeof(m_FFTBuffer));
    memset(m_TrackTitle, 0, sizeof(m_TrackTitle));
    strncpy(m_TrackTitle, "Waiting...", sizeof(m_TrackTitle) - 1);
    m_TrackInfoQuerying.store(false);
    m_QueryTimeout = 0.0f;
}

CAudioCapture::~CAudioCapture()
{
    Shutdown();
}

bool CAudioCapture::Init()
{
    if(m_Initialized) return true;
    SWasapiImpl *impl = new SWasapiImpl();
    if(!Wasapi_Init(impl)) { delete impl; return false; }
    m_pImpl = impl;
    m_Initialized = true;
    m_ShouldStop.store(false);
    m_Capturing.store(true);
    m_Thread = std::thread(&CAudioCapture::CaptureThread, this);
    return true;
}

void CAudioCapture::Shutdown()
{
    if(!m_Initialized) return;
    m_ShouldStop.store(true);
    if(m_Thread.joinable()) m_Thread.join();
    m_Capturing.store(false);
    if(m_pImpl) { Wasapi_Shutdown((SWasapiImpl*)m_pImpl); delete (SWasapiImpl*)m_pImpl; m_pImpl = nullptr; }
    m_Initialized = false;
}

void CAudioCapture::CaptureThread()
{
    static float buf[4096];
    SWasapiImpl *impl = (SWasapiImpl*)m_pImpl;
    if(!impl)
    {
        m_Capturing.store(false);
        return;
    }

    while(!m_ShouldStop.load())
    {
        int count = 0;
        bool ok = false;
        try { ok = Wasapi_GetNextBuffer(impl, buf, 4096, &count); } catch(...) { ok = false; }

        if(ok && count > 0)
        {
            for(int i = 0; i < count; i++)
            {
                m_FFTBuffer[m_FFTPos] = buf[i];
                m_RawWaveform[m_FFTPos % WAVEFORM_SAMPLES] = buf[i];
                m_FFTPos++;

                if(m_FFTPos >= FFT_SIZE)
                {
                    m_FFTPos = 0;
                    float real[FFT_SIZE], imag[FFT_SIZE];
                    for(int j = 0; j < FFT_SIZE; j++)
                    {
                        float w = 0.5f * (1.0f - cosf(2.0f * 3.14159265f * j / (FFT_SIZE - 1)));
                        real[j] = m_FFTBuffer[j] * w;
                        imag[j] = 0;
                    }
                    FFT(real, imag, FFT_SIZE);
                    std::lock_guard<std::mutex> lock(m_Mutex);
                    int halfN = FFT_SIZE / 2;
                    float bs = (float)halfN / SPECTRUM_BANDS;
                    for(int b = 0; b < SPECTRUM_BANDS; b++)
                    {
                        int s2 = (int)(b * bs), e = (int)((b + 1) * bs);
                        if(e > halfN) e = halfN;
                        if(s2 >= e) s2 = e - 1;
                        if(s2 < 0) s2 = 0;
                        float sum = 0;
                        for(int j = s2; j < e; j++)
                            sum += sqrtf(real[j] * real[j] + imag[j] * imag[j]);
                        m_RawSpectrum[b] = sum / std::max(1, e - s2);
                    }
                    for(int j = 0; j < WAVEFORM_SAMPLES; j++)
                        m_Waveform[j] = m_RawWaveform[j];
                }
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    m_Capturing.store(false);
}

void CAudioCapture::FFT(float *real, float *imag, int n)
{
    for(int i = 1, j = 0; i < n; i++)
    {
        int bit = n >> 1;
        for(; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if(i < j) { std::swap(real[i], real[j]); std::swap(imag[i], imag[j]); }
    }
    for(int len = 2; len <= n; len <<= 1)
    {
        float ang = -2.0f * (float)M_PI / len;
        float wR = cosf(ang), wI = sinf(ang);
        for(int i = 0; i < n; i += len)
        {
            float curR = 1, curI = 0;
            for(int j = 0; j < len / 2; j++)
            {
                float uR = real[i+j], uI = imag[i+j];
                float vR = real[i+j+len/2]*curR - imag[i+j+len/2]*curI;
                float vI = real[i+j+len/2]*curI + imag[i+j+len/2]*curR;
                real[i+j] = uR+vR; imag[i+j] = uI+vI;
                real[i+j+len/2] = uR-vR; imag[i+j+len/2] = uI-vI;
                float nR = curR*wR - curI*wI;
                curI = curR*wI + curI*wR;
                curR = nR;
            }
        }
    }
}


void CAudioCapture::Update(float deltaTime, float sensitivity, float smoothing)
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        float sens = sensitivity / 50.0f;
        float smooth = smoothing / 100.0f;

        for(int i = 0; i < SPECTRUM_BANDS; i++)
        {
            float t = m_RawSpectrum[i] * sens * (1.0f + logf(1.0f + (float)i) * 0.8f);
            t = std::clamp(t, 0.0f, 1.0f);
            m_SmoothSpectrum[i] = m_SmoothSpectrum[i] * smooth + t * (1.0f - smooth);
        }

        m_Bass = m_Mid = m_Treble = 0;
        for(int i = 0; i < SPECTRUM_BANDS; i++)
        {
            if(i < SPECTRUM_BANDS/4) m_Bass += m_SmoothSpectrum[i];
            else if(i < SPECTRUM_BANDS*3/4) m_Mid += m_SmoothSpectrum[i];
            else m_Treble += m_SmoothSpectrum[i];
        }
        m_Bass /= (SPECTRUM_BANDS/4);
        m_Mid /= (SPECTRUM_BANDS/2);
        m_Treble /= (SPECTRUM_BANDS/4);

        m_Volume = 0;
        for(int i = 0; i < SPECTRUM_BANDS; i++) m_Volume += m_SmoothSpectrum[i];
        m_Volume /= SPECTRUM_BANDS;

        float energy = m_Bass * 3.0f + m_Volume;
        m_BeatAvgEnergy = m_BeatAvgEnergy * 0.95f + energy * 0.05f;
        m_BeatCooldown -= deltaTime;
        if(energy > m_BeatAvgEnergy * m_BeatThreshold && m_BeatCooldown <= 0)
        {
            m_Beat = true; m_BeatIntensity = 1.0f; m_BeatCooldown = 0.15f;
        }
        else m_Beat = false;
        m_BeatIntensity = std::max(0.0f, m_BeatIntensity * (1.0f - deltaTime * 8.0f));
    }

    m_TrackInfoTimer -= deltaTime;

    if(m_TrackInfoQuerying.load())
    {
        m_QueryTimeout -= deltaTime;
        if(m_QueryTimeout <= 0.0f)
        {
            strncpy(m_TrackTitle, "This is not work on ur pc", sizeof(m_TrackTitle) - 1);
            strncpy(m_TrackArtist, "Wow sad", sizeof(m_TrackArtist) - 1);
            m_HasTrackInfo = true;
        }
    }

    if(m_TrackInfoTimer <= 0.0f)
    {
        m_TrackInfoTimer = 5.0f;
        if(!m_TrackInfoQuerying.load())
        {
            m_TrackInfoQuerying.store(true);
            m_QueryTimeout = 2.0f;
            std::thread([this]() {
                SMediaInfo info;
                bool ok = MediaInfo_Query(&info);
                if(ok && info.valid)
                {
                    strncpy(m_PendingTitle, info.title, sizeof(m_PendingTitle) - 1);
                    strncpy(m_PendingArtist, info.artist, sizeof(m_PendingArtist) - 1);
                    m_PendingValid = true;
                }
                else
                {
                    strncpy(m_PendingTitle, "This is not work on ur pc", sizeof(m_PendingTitle) - 1);
                    strncpy(m_PendingArtist, "Wow sad", sizeof(m_PendingArtist) - 1);
                    m_PendingValid = true;
                }
                m_TrackInfoReady.store(true);
                m_TrackInfoQuerying.store(false);
            }).detach();
        }
    }

    if(m_TrackInfoReady.load())
    {
        strncpy(m_TrackTitle, m_PendingTitle, sizeof(m_TrackTitle) - 1);
        strncpy(m_TrackArtist, m_PendingArtist, sizeof(m_TrackArtist) - 1);
        m_HasTrackInfo = m_PendingValid;
        m_TrackInfoReady.store(false);
    }
}

float CAudioCapture::GetBand(int i) const
{
    return (i >= 0 && i < SPECTRUM_BANDS) ? m_SmoothSpectrum[i] : 0;
}

float CAudioCapture::GetWaveform(int i) const
{
    return (i >= 0 && i < WAVEFORM_SAMPLES) ? m_Waveform[i] : 0;
}