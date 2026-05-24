#include "AudioVisualizerPopup.h"
#include "audio/AudioRenderer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QApplication>
#include <QScreen>
#include <QGuiApplication>
#include <cmath>
#include <algorithm>
#include <cstring>

// ── Theme colors ──

struct ThemeColors {
    QColor primary;
    QColor secondary;
    QColor glow;
};

static ThemeColors getThemeColors(AudioVisualizerPopup::Theme t) {
    switch (t) {
    case AudioVisualizerPopup::Theme::IceBlue:
        return { QColor(0, 212, 255), QColor(102, 68, 255), QColor(0, 212, 255, 40) };
    case AudioVisualizerPopup::Theme::Amber:
        return { QColor(255, 107, 53), QColor(255, 179, 71), QColor(255, 107, 53, 40) };
    case AudioVisualizerPopup::Theme::Aurora:
        return { QColor(0, 255, 135), QColor(96, 239, 255), QColor(0, 255, 135, 40) };
    case AudioVisualizerPopup::Theme::Monochrome:
        return { QColor(255, 255, 255, 153), QColor(255, 255, 255, 51), QColor(255, 255, 255, 30) };
    }
    return {};
}

// ── Constructor ──

AudioVisualizerPopup::AudioVisualizerPopup(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(520, 340);

    m_visualizer = new AudioVisualizer(this);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(33);
    connect(m_refreshTimer, &QTimer::timeout, this, &AudioVisualizerPopup::refreshVisual);
    m_refreshTimer->start();

    setupUI();

    if (auto *screen = QGuiApplication::primaryScreen()) {
        QRect sg = screen->availableGeometry();
        move((sg.width() - width()) / 2 + sg.x(),
             (sg.height() - height()) / 2 + sg.y());
    }
}

void AudioVisualizerPopup::setupUI() {
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *panel = new QWidget(this);
    panel->setObjectName("visPanel");
    panel->setStyleSheet(
        "#visPanel { background: #06060c; border: 1px solid rgba(255,255,255,0.06);"
        "border-radius: 16px; }");
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Top bar ──
    auto *topBar = new QWidget(panel);
    topBar->setStyleSheet("background: transparent;");
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(14, 10, 14, 8);
    topLayout->setSpacing(4);

    auto modeBtnStyle = QString(
        "QPushButton { background: transparent; border: none; font-size: 11px; "
        "color: rgba(255,255,255,0.3); border-radius: 6px; padding: 4px 10px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.65); background: rgba(255,255,255,0.05); }"
        "QPushButton:pressed { color: rgba(255,255,255,0.45); }");
    auto modeBtnActiveStyle = QString(
        "QPushButton { background: rgba(0,212,255,0.08); border: 1px solid rgba(0,212,255,0.15); "
        "font-size: 11px; color: #00d4ff; border-radius: 6px; padding: 4px 10px; }");

    m_spectrumBtn = new QPushButton(QString::fromUtf8("\xe9\xa2\x91\xe8\xb0\xb1"), topBar);
    m_spectrumBtn->setFixedHeight(26);
    m_spectrumBtn->setCursor(Qt::PointingHandCursor);
    m_spectrumBtn->setStyleSheet(modeBtnActiveStyle);

    m_waveformBtn = new QPushButton(QString::fromUtf8("\xe6\xb3\xa2\xe5\xbd\xa2"), topBar);
    m_waveformBtn->setFixedHeight(26);
    m_waveformBtn->setCursor(Qt::PointingHandCursor);
    m_waveformBtn->setStyleSheet(modeBtnStyle);

    m_circularBtn = new QPushButton(QString::fromUtf8("\xe5\x9c\x86\xe5\xbd\xa2"), topBar);
    m_circularBtn->setFixedHeight(26);
    m_circularBtn->setCursor(Qt::PointingHandCursor);
    m_circularBtn->setStyleSheet(modeBtnStyle);

    auto setMode = [this, modeBtnStyle, modeBtnActiveStyle](Mode m, QPushButton* active) {
        m_mode = m;
        m_spectrumBtn->setStyleSheet(m == Mode::Spectrum ? modeBtnActiveStyle : modeBtnStyle);
        m_waveformBtn->setStyleSheet(m == Mode::Waveform ? modeBtnActiveStyle : modeBtnStyle);
        m_circularBtn->setStyleSheet(m == Mode::Circular ? modeBtnActiveStyle : modeBtnStyle);
    };

    connect(m_spectrumBtn, &QPushButton::clicked, this, [this, setMode]() { setMode(Mode::Spectrum, m_spectrumBtn); });
    connect(m_waveformBtn, &QPushButton::clicked, this, [this, setMode]() { setMode(Mode::Waveform, m_waveformBtn); });
    connect(m_circularBtn, &QPushButton::clicked, this, [this, setMode]() { setMode(Mode::Circular, m_circularBtn); });

    topLayout->addWidget(m_spectrumBtn);
    topLayout->addWidget(m_waveformBtn);
    topLayout->addWidget(m_circularBtn);
    topLayout->addStretch();

    // Theme button
    m_themeBtn = new QPushButton(QString::fromUtf8("\xe5\x86\xb0\xe8\x93\x9d \xe2\x96\xbe"), topBar);
    m_themeBtn->setFixedHeight(26);
    m_themeBtn->setFixedWidth(64);
    m_themeBtn->setCursor(Qt::PointingHandCursor);
    m_themeBtn->setStyleSheet(
        "QPushButton { font-size: 10px; color: rgba(255,255,255,0.4); background: transparent; "
        "border: 1px solid rgba(255,255,255,0.06); border-radius: 5px; padding: 0 6px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.75); border-color: rgba(255,255,255,0.15); }"
        "QPushButton:pressed { color: rgba(255,255,255,0.55); border-color: rgba(255,255,255,0.1); }");
    connect(m_themeBtn, &QPushButton::clicked, this, [this]() {
        m_theme = static_cast<Theme>((static_cast<int>(m_theme) + 1) % 4);
        const char* names[] = { "\xe5\x86\xb0\xe8\x93\x9d", "\xe7\x90\xa5\xe7\x8f\x80", "\xe6\x9e\x81\xe5\x85\x89", "\xe5\x8d\x95\xe8\x89\xb2" };
        m_themeBtn->setText(QString::fromUtf8(names[static_cast<int>(m_theme)]) + " \xe2\x96\xbe");
        // Update active mode button color to match theme
        auto tc = getThemeColors(m_theme);
        QString activeStyle = QString(
            "QPushButton { background: rgba(%1,%2,%3,0.08); border: 1px solid rgba(%4,%5,%6,0.15); "
            "font-size: 11px; color: rgb(%7,%8,%9); border-radius: 6px; padding: 4px 10px; }")
            .arg(tc.primary.red()).arg(tc.primary.green()).arg(tc.primary.blue())
            .arg(tc.primary.red()).arg(tc.primary.green()).arg(tc.primary.blue())
            .arg(tc.primary.red()).arg(tc.primary.green()).arg(tc.primary.blue());
        auto btn = m_mode == Mode::Spectrum ? m_spectrumBtn
                 : m_mode == Mode::Waveform ? m_waveformBtn : m_circularBtn;
        btn->setStyleSheet(activeStyle);
    });

    topLayout->addWidget(m_themeBtn);
    topLayout->addSpacing(6);

    // Sensitivity label + slider
    auto *sensLabel = new QLabel(QString::fromUtf8("\xe6\x95\x8f\xe6\x84\x9f"), topBar);
    sensLabel->setStyleSheet("font-size: 10px; color: rgba(255,255,255,0.15); border: none;");
    topLayout->addWidget(sensLabel);

    m_sensitivitySlider = new QSlider(Qt::Horizontal, topBar);
    m_sensitivitySlider->setRange(5, 300);
    m_sensitivitySlider->setValue(10);
    m_sensitivitySlider->setFixedWidth(60);
    m_sensitivitySlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 2px; background: rgba(255,255,255,0.04); border-radius: 1px; }"
        "QSlider::handle:horizontal { width: 8px; height: 8px; background: rgba(255,255,255,0.2); "
        "border-radius: 4px; margin: -3px 0; }"
        "QSlider::handle:hover { background: rgba(255,255,255,0.4); }"
        "QSlider::sub-page:horizontal { background: rgba(0,212,255,0.5); border-radius: 1px; }");
    connect(m_sensitivitySlider, &QSlider::valueChanged, this, [this](int v) {
        m_sensitivity = v / 10.0f;
    });

    topLayout->addWidget(m_sensitivitySlider);

    // Close
    m_closeBtn = new QPushButton(QStringLiteral("\xe2\x9c\x95"), topBar);
    m_closeBtn->setFixedSize(24, 24);
    m_closeBtn->setFlat(true);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(
        "QPushButton { font-size: 12px; color: rgba(255,255,255,0.35); background: transparent; "
        "border: none; border-radius: 6px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.7); background: rgba(255,255,255,0.06); }"
        "QPushButton:pressed { color: rgba(255,255,255,0.5); }");
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() { hide(); });

    topLayout->addWidget(m_closeBtn);

    layout->addWidget(topBar);

    // Separator
    auto *sep1 = new QWidget(panel);
    sep1->setFixedHeight(1);
    sep1->setStyleSheet("background: rgba(255,255,255,0.03);");
    layout->addWidget(sep1);

    // Canvas
    m_canvas = new QWidget(panel);
    m_canvas->setStyleSheet("background: transparent;");
    m_canvas->installEventFilter(this);
    layout->addWidget(m_canvas, 1);

    // Separator
    auto *sep2 = new QWidget(panel);
    sep2->setFixedHeight(1);
    sep2->setStyleSheet("background: rgba(255,255,255,0.03);");
    layout->addWidget(sep2);

    // ── Bottom bar ──
    auto *bottomBar = new QWidget(panel);
    bottomBar->setStyleSheet("background: transparent;");
    auto *botLayout = new QHBoxLayout(bottomBar);
    botLayout->setContentsMargins(14, 8, 14, 10);
    botLayout->setSpacing(6);

    m_muteBtn = new QPushButton(QStringLiteral("\xf0\x9f\x94\x8a"), bottomBar);
    m_muteBtn->setFixedSize(24, 24);
    m_muteBtn->setFlat(true);
    m_muteBtn->setCursor(Qt::PointingHandCursor);
    m_muteBtn->setStyleSheet(
        "QPushButton { font-size: 13px; color: rgba(255,255,255,0.35); background: transparent; "
        "border: none; border-radius: 4px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.7); }"
        "QPushButton:pressed { color: rgba(255,255,255,0.5); }");
    m_muteBtn->setToolTip(QString::fromUtf8("\xe9\x9d\x99\xe9\x9f\xb3"));
    connect(m_muteBtn, &QPushButton::clicked, this, &AudioVisualizerPopup::muteToggled);

    m_volSlider = new QSlider(Qt::Horizontal, bottomBar);
    m_volSlider->setRange(0, 100);
    m_volSlider->setValue(70);
    m_volSlider->setFixedWidth(60);
    m_volSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 3px; background: rgba(255,255,255,0.04); border-radius: 1px; }"
        "QSlider::handle:horizontal { width: 10px; height: 10px; background: rgba(255,255,255,0.25); "
        "border-radius: 5px; margin: -4px 0; }"
        "QSlider::handle:hover { background: rgba(255,255,255,0.4); }"
        "QSlider::sub-page:horizontal { background: rgba(0,212,255,0.5); border-radius: 1px; }");
    m_volSlider->setToolTip(QString::fromUtf8("\xe9\x9f\xb3\xe9\x87\x8f"));
    connect(m_volSlider, &QSlider::valueChanged, this, [this](int v) {
        emit volumeChanged(v / 100.0);
    });

    m_timeLabel = new QLabel("00:00:00 / 00:00:00", bottomBar);
    m_timeLabel->setStyleSheet("font-size: 10px; color: rgba(255,255,255,0.15); border: none;");
    m_timeLabel->setFixedWidth(150);

    m_fullscreenBtn = new QPushButton(QStringLiteral("\xe2\x9b\xb6"), bottomBar);
    m_fullscreenBtn->setFixedSize(28, 28);
    m_fullscreenBtn->setFlat(true);
    m_fullscreenBtn->setCursor(Qt::PointingHandCursor);
    m_fullscreenBtn->setToolTip(QString::fromUtf8("\xe5\x85\xa8\xe5\xb1\x8f"));
    m_fullscreenBtn->setStyleSheet(
        "QPushButton { font-size: 14px; color: rgba(255,255,255,0.35); background: transparent; "
        "border: none; border-radius: 6px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.7); background: rgba(255,255,255,0.06); }"
        "QPushButton:pressed { color: rgba(255,255,255,0.5); }");
    connect(m_fullscreenBtn, &QPushButton::clicked, this, &AudioVisualizerPopup::fullscreenRequested);

    botLayout->addWidget(m_muteBtn);
    botLayout->addWidget(m_volSlider);
    botLayout->addSpacing(8);
    botLayout->addWidget(m_timeLabel);
    botLayout->addStretch();
    botLayout->addWidget(m_fullscreenBtn);

    layout->addWidget(bottomBar);

    outer->addWidget(panel);
}

// ── Public API ──

void AudioVisualizerPopup::setAudioSource(AudioRenderer *renderer) {
    m_audioSource = renderer;
    if (renderer && !m_refreshTimer->isActive())
        m_refreshTimer->start();
}

void AudioVisualizerPopup::detachSource() {
    m_refreshTimer->stop();
    m_audioSource = nullptr;
    m_visData = AudioVisualizer::VisualData{};
    std::memset(m_waveformHistory, 0, sizeof(m_waveformHistory));
    std::memset(m_peaks, 0, sizeof(m_peaks));
    m_rings.clear();
    std::memset(m_spawnAccum, 0, sizeof(m_spawnAccum));
    m_circularRotation = 0.0f;
}

void AudioVisualizerPopup::setVolume(double vol) {
    m_volSlider->blockSignals(true);
    m_volSlider->setValue(qRound(vol * 100));
    m_volSlider->blockSignals(false);
}

void AudioVisualizerPopup::setMuted(bool muted) {
    m_muteBtn->setText(muted ? QStringLiteral("\xf0\x9f\x94\x87") : QStringLiteral("\xf0\x9f\x94\x8a"));
}

// ── Refresh ──

void AudioVisualizerPopup::refreshVisual() {
    if (m_audioSource) {
        float samples[2048];
        if (m_audioSource->readVisSamples(samples, 2048)) {
            m_visualizer->setGain(m_sensitivity);
            m_visualizer->feedSamples(samples, 2048);
        } else {
            // No audio data available (e.g. video has no audio track):
            // feed silence so spectrum/waveform decay naturally to zero
            float silence[2048]{};
            m_visualizer->feedSamples(silence, 2048);
        }
    }
    m_visData = m_visualizer->readData();

    m_circularRotation += 0.005f;
    if (m_circularRotation > 2.0f * 3.14159265f)
        m_circularRotation -= 2.0f * 3.14159265f;

    // Ring particle system for Circular mode
    if (m_mode == Mode::Circular) {
        for (int b = 0; b < 6; ++b) {
            float energy = 0;
            int start = (b * 64) / 6;
            int end = ((b + 1) * 64) / 6;
            for (int i = start; i < end; ++i)
                energy += m_visData.spectrum[i];
            energy /= (end - start);
            m_spawnAccum[b] += energy * 0.6f;
            if (m_spawnAccum[b] > 1.0f && m_rings.size() < 60) {
                m_spawnAccum[b] = 0;
                Ring r;
                r.speed = 0.5f + energy * 5.0f;
                r.maxRadius = 250.0f + b * 100.0f;
                r.band = b;
                m_rings.append(r);
            }
        }
        for (auto &r : m_rings) {
            r.radius += r.speed;
            r.opacity -= 0.003f;
            if (r.opacity < 0.0f) r.opacity = 0.0f;
        }
        m_rings.erase(
            std::remove_if(m_rings.begin(), m_rings.end(),
                           [](const Ring &rr) { return rr.opacity <= 0.0f || rr.radius > rr.maxRadius; }),
            m_rings.end());
    }

    if (m_canvas)
        m_canvas->update();
}

// ── Event filter (canvas painting) ──

bool AudioVisualizerPopup::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_canvas && event->type() == QEvent::Paint) {
        QPainter p(m_canvas);
        p.setRenderHint(QPainter::Antialiasing);

        const int canvasX = 20;
        const int canvasY = 8;
        const int canvasW = m_canvas->width() - 40;
        const int canvasH = m_canvas->height() - 16;

        ThemeColors tc = getThemeColors(m_theme);

        switch (m_mode) {

        case Mode::Spectrum: {
            const int barCount = 64;
            const int gap = 2;
            const int barW = std::max(2, (canvasW - gap * (barCount - 1)) / barCount);

            for (int i = 0; i < barCount; ++i) {
                float val = m_visData.spectrum[i];
                int barH = static_cast<int>(val * (canvasH - 10));
                int x = canvasX + i * (barW + gap);
                int y = canvasY + canvasH - 10 - barH;

                QLinearGradient bg(0, canvasY + canvasH - 10, 0, canvasY);
                bg.setColorAt(0, tc.primary);
                bg.setColorAt(0.5, tc.secondary);
                bg.setColorAt(1, tc.glow);

                QPainterPath barPath;
                barPath.addRoundedRect(x, y, barW, barH, 1.5, 1.5);
                p.fillPath(barPath, bg);

                if (m_visData.peakHold[i] > m_peaks[i].pos) {
                    m_peaks[i].pos = m_visData.peakHold[i];
                    m_peaks[i].frames = 0;
                } else {
                    m_peaks[i].frames++;
                    if (m_peaks[i].frames > 3) {
                        m_peaks[i].pos -= 0.003f;
                        if (m_peaks[i].pos < 0) m_peaks[i].pos = 0;
                    }
                }
                int peakY = canvasY + canvasH - 10 - static_cast<int>(m_peaks[i].pos * (canvasH - 10));
                p.fillRect(x, peakY, barW, 2, QColor(255, 255, 255, 160));
            }
            break;
        }

        case Mode::Waveform: {
            QPainterPath path;
            int midY = canvasY + canvasH / 2;
            float hScale = canvasH * 0.45f;

            std::memmove(m_waveformHistory, m_waveformHistory + 16,
                         (512 - 16) * sizeof(float));
            for (int i = 0; i < 16 && i < 256; ++i)
                m_waveformHistory[512 - 16 + i] = m_visData.waveform[i];

            float xStep = canvasW / 512.0f;
            path.moveTo(canvasX, midY + m_waveformHistory[0] * hScale);
            for (int i = 1; i < 512; ++i) {
                float x = canvasX + i * xStep;
                float y = midY + m_waveformHistory[i] * hScale;
                path.lineTo(x, y);
            }

            QPen wavePen(tc.primary, 1.5);
            p.setPen(wavePen);
            p.drawPath(path);

            p.setPen(QColor(255, 255, 255, 20));
            p.drawLine(canvasX, midY, canvasX + canvasW, midY);
            break;
        }

        case Mode::Circular: {
            int cx = canvasX + canvasW / 2;
            int cy = canvasY + canvasH / 2;

            p.save();
            p.translate(cx, cy);

            for (const auto &rr : m_rings) {
                QColor sc = tc.primary;
                sc.setAlpha(static_cast<int>(rr.opacity * 180));
                p.setPen(QPen(sc, 1.5f));

                QRadialGradient rg(0, 0, rr.radius);
                QColor fc = tc.secondary;
                int alpha = static_cast<int>(rr.opacity * 30);
                rg.setColorAt(0, QColor(fc.red(), fc.green(), fc.blue(), alpha));
                rg.setColorAt(1, QColor(fc.red(), fc.green(), fc.blue(), 0));

                QPainterPath cp;
                cp.addEllipse(QPointF(0, 0), rr.radius, rr.radius);
                p.fillPath(cp, rg);
                p.drawPath(cp);
            }

            p.restore();
            break;
        }
        }

        return true;
    }
    return QWidget::eventFilter(obj, event);
}

// ── Mouse events (drag to move) ──

void AudioVisualizerPopup::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStart = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
    QWidget::mousePressEvent(event);
}

void AudioVisualizerPopup::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragStart);
        event->accept();
    }
    QWidget::mouseMoveEvent(event);
}

void AudioVisualizerPopup::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton)
        m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}
