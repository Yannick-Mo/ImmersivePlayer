#include "ui/widgets/VolumeSlider.h"
#include <QHBoxLayout>
#include <QLabel>

VolumeSlider::VolumeSlider(QWidget *parent) : QWidget(parent) {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto *icon = new QLabel(QStringLiteral("\U0001F50A"), this);  // 🔊
    icon->setStyleSheet("font-size: 10px; color: rgba(255,255,255,0.2);");

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 100);
    m_slider->setValue(80);
    m_slider->setFixedWidth(70);
    m_slider->setFixedHeight(4);
    m_slider->setStyleSheet(
        "QSlider::groove:horizontal { background: rgba(255,255,255,0.08); height: 2px; border-radius: 1px; }"
        "QSlider::handle:horizontal { background: #00d4ff; width: 8px; height: 8px; margin: -3px 0; border-radius: 4px; }"
        "QSlider::sub-page:horizontal { background: rgba(0,212,255,0.3); height: 2px; border-radius: 1px; }"
    );

    connect(m_slider, &QSlider::valueChanged, this, [this](int val) {
        emit volumeChanged(val / 100.0);
    });

    layout->addWidget(icon);
    layout->addWidget(m_slider);
}

VolumeSlider::~VolumeSlider() {}

void VolumeSlider::setVolume(double vol) {
    m_slider->setValue(qBound(0, (int)(vol * 100), 100));
}

double VolumeSlider::volume() const {
    return m_slider->value() / 100.0;
}
