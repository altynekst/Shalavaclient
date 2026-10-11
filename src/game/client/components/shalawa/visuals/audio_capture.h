#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_VISUALS_AUDIO_CAPTURE_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_VISUALS_AUDIO_CAPTURE_H

#include <cstring>
#include <mutex>
#include <atomic>
#include <thread>
#include "media_info.h"

class CAudioCapture
{
public:
    static const int FFT_SIZE = 512;
    static const int SPECTRUM_BANDS = 64;
    static const int WAVEFORM_SAMPLES = 128;

    CAudioCapture();
    ~CAudioCapture();

    bool Init();
    void Shutdown();
    bool IsCapturing() const { return m_Capturing.load(); }

    float GetBand(int index) const;
    float GetWaveform(int index) const;
    float GetBass() const { return m_Bass; }
    const char *GetTrackArtist() const { return m_TrackArtist; }
    float GetMid() const { return m_Mid; }
    float GetTreble() const { return m_Treble; }
    float GetVolume() const { return m_Volume; }
    float GetBeatIntensity() const { return m_BeatIntensity; }
    bool IsBeat() const { return m_Beat; }
    const char *GetTrackTitle() const { return m_TrackTitle; }
    bool HasTrackInfo() const { return m_HasTrackInfo; }
    void Update(float deltaTime, float sensitivity, float smoothing);

private:
    void FFT(float *real, float *imag, int n);
    void CaptureThread();

    std::thread m_Thread;
    std::mutex m_Mutex;
    std::atomic<bool> m_Capturing;
    std::atomic<bool> m_TrackInfoQuerying;
    float m_QueryTimeout;
    std::atomic<bool> m_TrackInfoReady;
    char m_PendingTitle[256];
    char m_PendingArtist[256];
    bool m_PendingValid;
    std::atomic<bool> m_ShouldStop;

    void *m_pImpl;

    float m_RawSpectrum[SPECTRUM_BANDS];
    float m_SmoothSpectrum[SPECTRUM_BANDS];
    float m_Waveform[WAVEFORM_SAMPLES];
    float m_RawWaveform[WAVEFORM_SAMPLES];
    float m_FFTBuffer[FFT_SIZE];
    int m_FFTPos;

    float m_Bass, m_Mid, m_Treble, m_Volume;
    float m_BeatAvgEnergy, m_BeatThreshold;
    float m_BeatCooldown, m_BeatIntensity;
    bool m_Beat;

    char m_TrackTitle[256];
    char m_TrackArtist[256];
    bool m_HasTrackInfo;
    float m_TrackInfoTimer;
    bool m_Initialized;
};

#endif