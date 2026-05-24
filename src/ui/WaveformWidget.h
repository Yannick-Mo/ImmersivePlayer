#pragma once
#include <QWidget>
#include <QVector>
#include <QMutex>
#include <QTimer>

class WaveformWidget : public QWidget {
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget *parent = nullptr);

public slots:
    void feedData(const QVector<double> &samples);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<double> m_amplitudes;
    QMutex m_mutex;
    QTimer m_updateTimer;

    static constexpr int kBarCount = 64;
    static constexpr int kMaxAmplitude = 100;
};
