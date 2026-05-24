#include "AudioRenderer.h"
#include <QDebug>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <thread>
#include <chrono>

// ── Construction / Destruction ──────────────────────────────────────────────

AudioRenderer::AudioRenderer(QObject* parent)
    : QObject(parent)
    , m_ringBuf(RB_SIZE, 0)
    , m_visBuffer(VIS_BUF_SIZE, 0.0f)
{
}

AudioRenderer::~AudioRenderer() {
    stop();
}

// ── Static helpers ──────────────────────────────────────────────────────────

bool AudioRenderer::hasOutputDevice() {
    ma_context ctx;
    if (ma_context_init(NULL, 0, NULL, &ctx) != MA_SUCCESS)
        return false;
    ma_device_info* infos;
    ma_uint32 count;
    ma_result r = ma_context_get_devices(&ctx, &infos, &count, NULL, NULL);
    ma_context_uninit(&ctx);
    return r == MA_SUCCESS && count > 0;
}

// ── Callbacks ───────────────────────────────────────────────────────────────

void AudioRenderer::dataCallback(ma_device* pDevice, void* pOutput,
                                  const void* /*pInput*/, unsigned int frameCount) {
    auto* self = static_cast<AudioRenderer*>(pDevice->pUserData);
    if (!self || self->m_error.load(std::memory_order_relaxed)) {
        unsigned int ch = self ? self->m_channels : 2;
        std::memset(pOutput, 0, frameCount * ch * sizeof(int16_t));
        return;
    }

    int channels = self->m_channels;
    unsigned int needed = frameCount * channels;
    auto* out = static_cast<int16_t*>(pOutput);

    // Read from ring buffer
    unsigned int r = self->m_readIdx.load(std::memory_order_relaxed);
    unsigned int w = self->m_writeIdx.load(std::memory_order_acquire);
    unsigned int avail = w - r;
    unsigned int toRead = needed < avail ? needed : avail;

    if (toRead > 0) {
        unsigned int base = r & RB_MASK;
        unsigned int end = (r + toRead) & RB_MASK;
        if (end > base) {
            std::memcpy(out, self->m_ringBuf.data() + base, toRead * sizeof(int16_t));
        } else {
            unsigned int first = RB_SIZE - base;
            std::memcpy(out, self->m_ringBuf.data() + base, first * sizeof(int16_t));
            std::memcpy(out + first, self->m_ringBuf.data(), (toRead - first) * sizeof(int16_t));
        }
    }
    self->m_readIdx.store(r + toRead, std::memory_order_release);

    // Silence on underrun
    if (toRead < needed) {
        std::memset(out + toRead, 0, (needed - toRead) * sizeof(int16_t));
    }

    // Volume
    qreal vol = self->m_muted.load(std::memory_order_relaxed) ? 0.0
                : self->m_volume.load(std::memory_order_relaxed);
    if (std::abs(vol - 1.0) > 0.0001) {
        for (unsigned int i = 0; i < needed; ++i) {
            out[i] = static_cast<int16_t>(out[i] * vol);
        }
    }

    // Clock: frameCount device-frames → microseconds at base sample rate
    if (self->m_baseSampleRate > 0) {
        int64_t dur = static_cast<int64_t>(frameCount) * 1000000LL / self->m_baseSampleRate;
        self->m_customClock.fetch_add(dur, std::memory_order_relaxed);
    }
}

void AudioRenderer::notificationCallback(const ma_device_notification* pNotification) {
    auto* self = static_cast<AudioRenderer*>(pNotification->pDevice->pUserData);
    if (!self) return;

    switch (pNotification->type) {
    case ma_device_notification_type_started:
        qDebug() << "AudioRenderer: device started";
        break;
    case ma_device_notification_type_stopped:
        qDebug() << "AudioRenderer: device stopped";
        break;
    case ma_device_notification_type_rerouted:
        // miniaudio has already reinitialized the device on the new endpoint.
        // The data callback is already firing on the new device. No action needed.
        qDebug() << "AudioRenderer: device rerouted (hot-plug)";
        break;
    default:
        break;
    }
}

// ── Device lifecycle ────────────────────────────────────────────────────────

bool AudioRenderer::createDevice(int effectiveRate) {
    destroyDevice();

    if (!hasOutputDevice()) {
        qWarning() << "AudioRenderer: no audio output device";
        return false;
    }

    m_effectiveRate = effectiveRate;

    m_context = new (std::nothrow) ma_context;
    if (!m_context)
        return false;
    if (ma_context_init(NULL, 0, NULL, m_context) != MA_SUCCESS) {
        qWarning() << "AudioRenderer: ma_context_init failed";
        delete m_context; m_context = nullptr;
        return false;
    }

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format   = ma_format_s16;
    cfg.playback.channels = m_channels;
    cfg.sampleRate        = static_cast<ma_uint32>(effectiveRate);
    cfg.dataCallback      = dataCallback;
    cfg.notificationCallback = notificationCallback;
    cfg.pUserData         = this;

    m_device = new (std::nothrow) ma_device;
    if (!m_device) {
        ma_context_uninit(m_context);
        delete m_context; m_context = nullptr;
        return false;
    }
    if (ma_device_init(m_context, &cfg, m_device) != MA_SUCCESS) {
        qWarning() << "AudioRenderer: ma_device_init failed";
        delete m_device; m_device = nullptr;
        ma_context_uninit(m_context);
        delete m_context; m_context = nullptr;
        return false;
    }

    // Reset ring buffer（设备已停止，回调线程不会并发访问）
    m_ringBuf.assign(RB_SIZE, 0);
    m_writeIdx.store(0, std::memory_order_release);
    m_readIdx.store(0, std::memory_order_release);

    // Reset visualization buffer
    m_visBuffer.assign(VIS_BUF_SIZE, 0.0f);
    m_visWriteIdx.store(0, std::memory_order_release);

    return true;
}

void AudioRenderer::destroyDevice() {
    if (m_device) {
        ma_device_uninit(m_device);
        delete m_device;
        m_device = nullptr;
    }
    if (m_context) {
        ma_context_uninit(m_context);
        delete m_context;
        m_context = nullptr;
    }
}

// ── Public API ──────────────────────────────────────────────────────────────

bool AudioRenderer::init(int sampleRate, int channels, double speed) {
    destroyDevice();

    m_baseSampleRate = sampleRate;
    m_channels = channels;
    m_customClock = 0;
    m_error = false;
    m_initialized = false;

    int effectiveRate = std::max(1, static_cast<int>(sampleRate * speed));
    if (!createDevice(effectiveRate)) {
        m_error = true;
        return false;
    }

    m_initialized = true;
    return true;
}

void AudioRenderer::start() {
    if (!m_initialized || !m_device) return;
    if (ma_device_start(m_device) != MA_SUCCESS) {
        qWarning() << "AudioRenderer: ma_device_start failed";
        m_error = true;
    }
}

void AudioRenderer::stop() {
    m_error = false;
    destroyDevice();
    m_initialized = false;
    m_customClock = 0;
    m_writeIdx = 0;
    m_readIdx = 0;

    // Reset visualization buffer to prevent stale data
    {
        QMutexLocker lock(&m_visMutex);
        m_visBuffer.assign(VIS_BUF_SIZE, 0.0f);
        m_visWriteIdx = 0;
    }
}

bool AudioRenderer::reinitialize() {
    if (!hasOutputDevice())
        return false;

    unsigned int rate = m_effectiveRate > 0 ? m_effectiveRate
                      : static_cast<unsigned int>(std::max(1, m_baseSampleRate));

    if (!createDevice(rate))
        return false;

    m_error = false;
    m_initialized = true;
    qDebug() << "AudioRenderer: reinitialized on new device";
    return true;
}

// ── Write (called from audio feed thread) ───────────────────────────────────

void AudioRenderer::write(const AVFrame* frame) {
    if (!m_initialized || !frame || !frame->data[0]) return;

    int numSamples = frame->nb_samples;
    int totalSamples = numSamples * m_channels;

    // Convert to interleaved int16_t
    static thread_local std::vector<int16_t> s_audioBuffer;
    if (s_audioBuffer.size() < static_cast<size_t>(totalSamples)) {
        s_audioBuffer.resize(totalSamples);
    }
    int16_t* buf = s_audioBuffer.data();
    size_t bufBytes = totalSamples * sizeof(int16_t);

    if (frame->format == AV_SAMPLE_FMT_FLTP || frame->format == AV_SAMPLE_FMT_FLT) {
        if (frame->format == AV_SAMPLE_FMT_FLT) {
            // Interleaved float: all channels packed in data[0]
            const float* src = reinterpret_cast<const float*>(frame->data[0]);
            for (int i = 0; i < totalSamples; ++i) {
                float s = std::clamp(src[i], -1.0f, 1.0f);
                buf[i] = static_cast<int16_t>(s * 32767.0f);
            }
        } else {
            // Planar float: each channel in its own plane
            for (int i = 0; i < numSamples; ++i) {
                for (int ch = 0; ch < m_channels; ++ch) {
                    const float* src = reinterpret_cast<const float*>(frame->data[ch]);
                    float s = std::clamp(src[i], -1.0f, 1.0f);
                    buf[i * m_channels + ch] = static_cast<int16_t>(s * 32767.0f);
                }
            }
        }
    } else if (frame->format == AV_SAMPLE_FMT_S16 || frame->format == AV_SAMPLE_FMT_S16P) {
        if (frame->format == AV_SAMPLE_FMT_S16P) {
            for (int i = 0; i < numSamples; ++i) {
                for (int ch = 0; ch < m_channels; ++ch) {
                    const int16_t* src = reinterpret_cast<const int16_t*>(frame->data[ch]);
                    buf[i * m_channels + ch] = src[i];
                }
            }
        } else {
            std::memcpy(buf, frame->data[0], bufBytes);
        }
    } else {
        return;
    }

    // ── Push to visualization buffer (mono float) ──
    {
        QMutexLocker lock(&m_visMutex);
        for (int si = 0; si < numSamples; ++si) {
            float sum = 0.0f;
            for (int ch = 0; ch < m_channels; ++ch) {
                int idx = si * m_channels + ch;
                sum += buf[idx] / 32767.0f;
            }
            m_visBuffer[m_visWriteIdx & (VIS_BUF_SIZE - 1)] = sum / m_channels;
            m_visWriteIdx++;
        }
    }

    // Push to ring buffer with backpressure (spin-wait up to ~100ms for space)
    unsigned int w;
    int retries = 0;
    for (;;) {
        w = m_writeIdx.load(std::memory_order_relaxed);
        unsigned int r = m_readIdx.load(std::memory_order_acquire);
        unsigned int free = RB_SIZE - (w - r);
        if (free >= static_cast<unsigned int>(totalSamples))
            break;
        if (++retries > 200) {
            return; // timeout — audio device likely stopped
        }
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }

    unsigned int base = w & RB_MASK;
    unsigned int end = (w + totalSamples) & RB_MASK;
    if (end > base) {
        std::memcpy(m_ringBuf.data() + base, buf, totalSamples * sizeof(int16_t));
    } else {
        unsigned int first = RB_SIZE - base;
        std::memcpy(m_ringBuf.data() + base, buf, first * sizeof(int16_t));
        std::memcpy(m_ringBuf.data(), buf + first, (totalSamples - first) * sizeof(int16_t));
    }
    m_writeIdx.store(w + totalSamples, std::memory_order_release);
}

// ── Visualization buffer ─────────────────────────────────────────────────────

bool AudioRenderer::readVisSamples(float* out, int count) {
    QMutexLocker lock(&m_visMutex);
    if (m_visWriteIdx == 0)
        return false;
    if (count > static_cast<int>(VIS_BUF_SIZE))
        count = VIS_BUF_SIZE;
    unsigned int w = m_visWriteIdx;
    unsigned int start = (w >= static_cast<unsigned int>(count))
        ? (w - count) : (VIS_BUF_SIZE + w - count);
    for (int i = 0; i < count; ++i) {
        out[i] = m_visBuffer[(start + i) & (VIS_BUF_SIZE - 1)];
    }
    return true;
}

// ── Clock ───────────────────────────────────────────────────────────────────

int64_t AudioRenderer::getClock() const {
    // No equivalent to QAudioSink::processedUSecs() in miniaudio.
    // Return custom clock which tracks consumed audio content.
    if (!m_initialized) return -1;
    return m_customClock.load(std::memory_order_relaxed);
}

int64_t AudioRenderer::getCustomClock() const {
    if (!m_initialized) return -1;
    return m_customClock.load(std::memory_order_relaxed);
}

// ── Volume ──────────────────────────────────────────────────────────────────

void AudioRenderer::setVolume(qreal volume) {
    m_volume = std::clamp(volume, 0.0, 1.0);
}

void AudioRenderer::setMuted(bool muted) {
    m_muted = muted;
}
