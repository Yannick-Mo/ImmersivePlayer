#pragma once
#include <QObject>
#include <QVector>
#include <QMutex>
#include <cmath>

class AudioVisualizer : public QObject {
    Q_OBJECT
public:
    explicit AudioVisualizer(QObject *parent = nullptr);

    void feedSamples(const float* data, int count);
    void setGain(float gain) { m_gain = gain; }
    float gain() const { return m_gain; }

    struct VisualData {
        float spectrum[64];
        float waveform[256];
        float peakHold[64];
    };
    VisualData readData();

signals:
    void dataReady();

private:
    void hannWindow(float* buf, int n);

    static constexpr int FFT_SIZE = 128;
    static constexpr int SPECTRUM_BINS = 64;
    static constexpr int WAVEFORM_SAMPLES = 256;

    static constexpr int RAW_BUF_SIZE = 4096;
    float m_rawBuffer[RAW_BUF_SIZE]{};
    unsigned int m_rawWriteIdx = 0;
    QMutex m_mutex;

    float m_fftReal[FFT_SIZE]{};
    float m_fftImag[FFT_SIZE]{};

    VisualData m_data{};
    float m_gain = 1.0f;
};
