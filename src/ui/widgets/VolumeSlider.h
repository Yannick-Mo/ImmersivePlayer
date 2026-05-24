#pragma once
#include <QWidget>
#include <QSlider>

class VolumeSlider : public QWidget {
    Q_OBJECT
public:
    explicit VolumeSlider(QWidget *parent = nullptr);
    ~VolumeSlider();

    void setVolume(double vol);
    double volume() const;

signals:
    void volumeChanged(double volume);

private:
    QSlider *m_slider;
};
