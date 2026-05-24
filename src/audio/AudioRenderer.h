#pragma once

#include <QObject>
#include <QMutex>
#include <memory>
#include <atomic>
#include <vector>

extern "C" {
#include <libavutil/frame.h>
}

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_WAV
#define MA_NO_FLAC
#define MA_NO_MP3
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MA_NO_GENERATION
#include "miniaudio.h"

class AudioRenderer : public QObject {
    Q_OBJECT
public:
    explicit AudioRenderer(QObject* parent = nullptr);
    ~AudioRenderer();

    bool init(int sampleRate, int channels, double speed = 1.0);
    void start();
    void stop();

    /// Copy vis samples to out[0..count-1]. Thread-safe. count may be clipped to VIS_BUF_SIZE.
    bool readVisSamples(float* out, int count);

    /// Push a decoded AVFrame (float planar or s16). Called from audio feed thread.
    void write(const AVFrame* frame);

    int64_t getClock() const;
    int64_t getCustomClock() const;

    void setVolume(qreal volume);
    qreal volume() const { return m_volume; }
    void setMuted(bool muted);
    bool isMuted() const { return m_muted; }
    bool hasError() const { return m_error.load(); }

    /// Reinitialize with current default device. Returns true on success.
    bool reinitialize();

    /// True if at least one audio output device exists.
    static bool hasOutputDevice();

private:
    void destroyDevice();
    bool createDevice(int effectiveRate);

    // ── miniaudio callbacks (static, dispatch via pUserData) ──
    static void dataCallback(ma_device* pDevice, void* pOutput, const void* pInput, unsigned int frameCount);
    static void notificationCallback(const ma_device_notification* pNotification);

    // ── Ring buffer (lock-free SPSC, power-of-2 sized) ──
    static constexpr unsigned int RB_SIZE = 1u << 18; // 262144 samples ≈ 2.7s @ 48kHz stereo s16
    static constexpr unsigned int RB_MASK = RB_SIZE - 1;
    std::vector<int16_t> m_ringBuf;
    std::atomic<unsigned int> m_writeIdx{0}; // only feed thread writes
    std::atomic<unsigned int> m_readIdx{0};  // only callback thread writes

    ma_device*   m_device = nullptr;
    ma_context*  m_context = nullptr;
    int m_baseSampleRate = 0;
    int m_channels = 0;
    unsigned int m_effectiveRate = 0;

    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_error{false};
    std::atomic<qreal> m_volume{1.0};
    std::atomic<bool> m_muted{false};
    std::atomic<int64_t> m_customClock{0};  // microseconds of audio consumed

    // ── Visualization sample buffer (thread-safe, UI reads, audio feed writes) ──
    static constexpr unsigned int VIS_BUF_SIZE = 2048;
    std::vector<float> m_visBuffer; // initialized in constructor init list
    QMutex m_visMutex;
    std::atomic<unsigned int> m_visWriteIdx{0};
};
