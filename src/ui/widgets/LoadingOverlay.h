#pragma once
#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>

class LoadingOverlay : public QWidget {
    Q_OBJECT
public:
    explicit LoadingOverlay(QWidget *parent = nullptr);

public slots:
    void showWithDelay(int delayMs = 500);
    void hideImmediately();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onAnimTick();
    void onDelayedShow();

private:
    QTimer m_animTimer;
    QTimer m_delayTimer;
    int m_angle = 0;
    bool m_delayedShowPending = false;
};
