#include "AudioVisualizer.h"
#include <cstring>
#include <algorithm>

static constexpr float kPi = 3.14159265358979323846f;

AudioVisualizer::AudioVisualizer(QObject *parent) : QObject(parent) {}

void AudioVisualizer::hannWindow(float* buf, int n) {
    for (int i = 0; i < n; ++i)
        buf[i] *= 0.5f * (1.0f - std::cos(2.0f * kPi * i / (n - 1)));
}

void AudioVisualizer::feedSamples(const float* data, int count) {
    QMutexLocker lock(&m_mutex);

    // Write to raw ring buffer (for waveform mode)
    for (int i = 0; i < count; ++i) {
        m_rawBuffer[m_rawWriteIdx & (RAW_BUF_SIZE - 1)] = data[i];
        m_rawWriteIdx++;
    }

    // Read last FFT_SIZE samples from raw ring buffer
    unsigned int rw = m_rawWriteIdx;
    for (int i = 0; i < FFT_SIZE; ++i)
        m_fftReal[i] = m_rawBuffer[(rw - FFT_SIZE + i) & (RAW_BUF_SIZE - 1)];

    // Hann window
    float windowed[FFT_SIZE];
    std::memcpy(windowed, m_fftReal, FFT_SIZE * sizeof(float));
    hannWindow(windowed, FFT_SIZE);

    // Copy windowed to FFT work buffer
    std::memset(m_fftImag, 0, FFT_SIZE * sizeof(float));
    for (int i = 0; i < FFT_SIZE; ++i)
        m_fftReal[i] = windowed[i];

    // Radix-2 DIT FFT
    int n = FFT_SIZE;
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(m_fftReal[i], m_fftReal[j]);
            std::swap(m_fftImag[i], m_fftImag[j]);
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        float wAngle = -2.0f * kPi / len;
        float wReal = std::cos(wAngle);
        float wImag = std::sin(wAngle);
        for (int i = 0; i < n; i += len) {
            float uReal = 1.0f, uImag = 0.0f;
            for (int j = 0; j < len / 2; ++j) {
                float tReal = uReal * m_fftReal[i + j + len / 2]
                            - uImag * m_fftImag[i + j + len / 2];
                float tImag = uReal * m_fftImag[i + j + len / 2]
                            + uImag * m_fftReal[i + j + len / 2];
                m_fftReal[i + j + len / 2] = m_fftReal[i + j] - tReal;
                m_fftImag[i + j + len / 2] = m_fftImag[i + j] - tImag;
                m_fftReal[i + j] += tReal;
                m_fftImag[i + j] += tImag;
                float nReal = uReal * wReal - uImag * wImag;
                float nImag = uReal * wImag + uImag * wReal;
                uReal = nReal;
                uImag = nImag;
            }
        }
    }

    // Magnitude → spectrum bins
    const float invN = 1.0f / n;
    for (int i = 0; i < SPECTRUM_BINS; ++i) {
        float mag = std::sqrt(m_fftReal[i] * m_fftReal[i] + m_fftImag[i] * m_fftImag[i]) * invN;
        mag *= m_gain;
        float prev = m_data.spectrum[i];
        m_data.spectrum[i] = prev + 0.4f * (std::min(mag, 1.0f) - prev);
        if (m_data.spectrum[i] >= m_data.peakHold[i]) {
            m_data.peakHold[i] = m_data.spectrum[i];
        } else {
            m_data.peakHold[i] -= 0.008f;
            if (m_data.peakHold[i] < 0.0f) m_data.peakHold[i] = 0.0f;
        }
    }

    // Waveform samples (from raw buffer)
    for (int i = 0; i < WAVEFORM_SAMPLES; ++i) {
        m_data.waveform[i] = m_rawBuffer[(rw - WAVEFORM_SAMPLES + i) & (RAW_BUF_SIZE - 1)];
    }
}

AudioVisualizer::VisualData AudioVisualizer::readData() {
    QMutexLocker lock(&m_mutex);
    return m_data;
}
