#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVector>
#include <QEvent>
#include <memory>
#include "audio/AudioVisualizer.h"

class AudioRenderer;

class AudioVisualizerPopup : public QWidget {
    Q_OBJECT
public:
    enum class Mode { Spectrum, Waveform, Circular };
    enum class Theme { IceBlue, Amber, Aurora, Monochrome };

    explicit AudioVisualizerPopup(QWidget *parent = nullptr);

    void setAudioSource(AudioRenderer *renderer);
    void setVolume(double vol);
    void setMuted(bool muted);
    void detachSource();

signals:
    void volumeChanged(double volume);
    void muteToggled();
    void fullscreenRequested();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void setupUI();
    void refreshVisual();

    Mode m_mode = Mode::Spectrum;
    Theme m_theme = Theme::IceBlue;
    float m_sensitivity = 1.0f;

    AudioVisualizer *m_visualizer;
    AudioRenderer *m_audioSource = nullptr;
    QTimer *m_refreshTimer;

    // Top bar controls
    QPushButton *m_spectrumBtn;
    QPushButton *m_waveformBtn;
    QPushButton *m_circularBtn;
    QPushButton *m_themeBtn;
    QSlider *m_sensitivitySlider;
    QPushButton *m_closeBtn;

    // Bottom controls
    QPushButton *m_muteBtn;
    QSlider *m_volSlider;
    QLabel *m_timeLabel;
    QPushButton *m_fullscreenBtn;

    // Drag state
    QPoint m_dragStart;
    bool m_dragging = false;

    // Canvas
    QWidget *m_canvas = nullptr;

    // Visual data cache
    AudioVisualizer::VisualData m_visData;
    float m_waveformHistory[512]{};
    float m_circularRotation = 0.0f;
    struct { float pos = 0.0f; int frames = 0; } m_peaks[64]{};

    // Ring particle system for Circular mode
    struct Ring {
        float radius = 0.0f;
        float speed = 1.0f;
        float opacity = 1.0f;
        float maxRadius = 100.0f;
        int band = 0;
    };
    QVector<Ring> m_rings;
    float m_spawnAccum[6]{};
};
