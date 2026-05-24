#pragma once
#include "VideoManager.h"
#include <QObject>
#include <QTimer>
#include <random>
#include <vector>
#include <functional>

class StressTest : public QObject {
    Q_OBJECT
public:
    enum TestPhase {
        PHASE_PAUSE_PLAY,
        PHASE_SPEED_CHANGE,
        PHASE_SEEK,
        PHASE_COMBO,
        PHASE_FILE_SWITCH
    };

    StressTest(VideoManager* mgr, QObject* parent = nullptr)
        : QObject(parent), m_mgr(mgr) {}

    void startAll(int durationMs = 30000) {
        m_startTime = std::chrono::steady_clock::now();
        m_duration = durationMs;
        m_phase = PHASE_PAUSE_PLAY;
        m_operationCount = 0;

        m_timer = new QTimer(this);
        m_timer->setTimerType(Qt::PreciseTimer);
        connect(m_timer, &QTimer::timeout, this, &StressTest::tick);
        m_timer->start(1); // 1ms interval for max stress

        QTimer::singleShot(durationMs, this, [this]() {
            m_timer->stop();
            emit finished(m_operationCount, m_errorCount);
        });
    }

signals:
    void finished(int ops, int errors);

private:
    void tick() {
        if (!m_mgr || !m_mgr->isRunning()) return;

        int elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - m_startTime).count();
        if (elapsed > m_duration) return;

        // Cycle through phases
        if (elapsed > m_phaseDuration * (static_cast<int>(m_phase) + 1)) {
            m_phase = static_cast<TestPhase>((m_phase + 1) % 5);
            if (m_phase == PHASE_PAUSE_PLAY)
                m_phaseDuration = std::min(m_phaseDuration + 2000, m_duration);
        }

        try {
            switch (m_phase) {
            case PHASE_PAUSE_PLAY:
                m_mgr->togglePause();
                break;
            case PHASE_SPEED_CHANGE: {
                static int speedIdx = 0;
                const double speeds[] = {0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 1.0};
                m_mgr->applySpeed(speeds[speedIdx % 7]);
                speedIdx++;
                break;
            }
            case PHASE_SEEK: {
                double r = static_cast<double>(rand()) / RAND_MAX;
                m_mgr->seek(r);
                break;
            }
            case PHASE_COMBO:
                if (rand() % 2) m_mgr->togglePause();
                else m_mgr->seek(static_cast<double>(rand()) / RAND_MAX);
                break;
            case PHASE_FILE_SWITCH:
                break;
            }
            m_operationCount++;
        } catch (...) {
            m_errorCount++;
        }
    }

    VideoManager* m_mgr;
    QTimer* m_timer = nullptr;
    TestPhase m_phase = PHASE_PAUSE_PLAY;
    int m_phaseDuration = 5000;
    int m_duration = 30000;
    int m_operationCount = 0;
    int m_errorCount = 0;
    std::chrono::steady_clock::time_point m_startTime;
};
