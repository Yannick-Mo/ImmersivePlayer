#include "WaveformWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

WaveformWidget::WaveformWidget(QWidget *parent) : QWidget(parent) {
    m_amplitudes.resize(kBarCount, 0);
    setFixedHeight(80);

    m_updateTimer.setInterval(33);
    connect(&m_updateTimer, &QTimer::timeout, this, [this]() {
        QMutexLocker l(&m_mutex);
        for (auto &a : m_amplitudes) {
            a = std::max(0.0, a - 0.5);
        }
        update();
    });
    m_updateTimer.start();
}

void WaveformWidget::feedData(const QVector<double> &samples) {
    QMutexLocker l(&m_mutex);
    if (samples.isEmpty()) return;

    int step = std::max(static_cast<int>(1), static_cast<int>(samples.size() / kBarCount));
    for (int i = 0; i < kBarCount && i * step < samples.size(); ++i) {
        double sum = 0;
        int count = 0;
        for (int j = 0; j < step && i * step + j < samples.size(); ++j) {
            sum += std::abs(samples[i * step + j]);
            ++count;
        }
        if (count > 0) {
            double val = (sum / count) * kMaxAmplitude;
            m_amplitudes[i] = std::max(m_amplitudes[i], val * 0.3);
            m_amplitudes[i] = std::min(kMaxAmplitude * 1.0, m_amplitudes[i]);
        }
    }
}

void WaveformWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height() - 20;
    const int barW = std::max(2, (w - kBarCount * 2) / kBarCount);
    const int gap = 2;
    const int midY = h - 10;

    QMutexLocker l(&m_mutex);

    for (int i = 0; i < kBarCount; ++i) {
        double amp = m_amplitudes[i];
        int barH = static_cast<int>((amp / kMaxAmplitude) * (h - 20));

        QLinearGradient grad(0, midY - barH, 0, midY);
        grad.setColorAt(0, QColor(0, 212, 255));
        grad.setColorAt(0.5, QColor(102, 68, 255));
        grad.setColorAt(1, QColor(0, 212, 255, 50));

        int x = i * (barW + gap) + gap;
        QPainterPath path;
        path.addRoundedRect(x, midY - barH, barW, barH, 1, 1);
        p.fillPath(path, grad);
    }

    // Bottom text
    p.setPen(QColor(255, 255, 255, 20));
    QFont f = p.font();
    f.setPointSize(7);
    p.setFont(f);
    p.drawText(2, h + 14, "L");
    p.drawText(w - 10, h + 14, "R");
    p.drawText(w / 2 - 30, h + 14, "AUDIO WAVEFORM");
}
