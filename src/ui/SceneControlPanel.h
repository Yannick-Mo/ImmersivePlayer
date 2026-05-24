#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include "common/Types.h"

class VideoProgressBar;

class SceneControlPanel : public QWidget {
    Q_OBJECT
public:
    explicit SceneControlPanel(QWidget *parent = nullptr);

    void updateMediaInfo(const MediaInfo &info);
    void updatePosition(double posSec, double durationSec);
    void updateProgress(double ratio);
    void setPlayButtonState(bool playing);
    void setSpeed(double speed);
    void setVolume(double vol);
    void setMuted(bool muted);
    void setPrevNextEnabled(bool hasPrev, bool hasNext);

    bool isUserDragged() const { return m_userDragged; }
    void resetAutoPosition() { m_userDragged = false; }

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

signals:
    void playPauseRequested();
    void seekRequested(double ratio);
    void speedMenuRequested(QPoint anchorGlobalPos);
    void volumeChanged(double volume);
    void muteToggled();
    void prevRequested();
    void nextRequested();
    void fullscreenRequested();
    void waveformToggleRequested();
    void showPlaylistRequested();
    void closeRequested();

private:
    void setupUI();
    QString formatTime(double sec) const;

    VideoProgressBar *m_progressBar;
    QLabel *m_titleLabel;
    QLabel *m_infoLabel;
    QLabel *m_timeLabel;
    QLabel *m_durationLabel;
    QPushButton *m_playBtn;
    QPushButton *m_prevBtn;
    QPushButton *m_nextBtn;
    QPushButton *m_speedBtn;
    QPushButton *m_muteBtn;
    QSlider *m_volSlider;
    QPushButton *m_fullscreenBtn;
    QPushButton *m_waveformBtn;
    QPushButton *m_playlistBtn;
    QPushButton *m_closeBtn;

    QPoint m_dragStart;
    bool m_dragging = false;
    bool m_userDragged = false;
};
