#include "SceneControlPanel.h"
#include "ui/widgets/VideoProgressBar.h"
#include "common/Constants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>

SceneControlPanel::SceneControlPanel(QWidget *parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(560);
    setupUI();
}

void SceneControlPanel::setupUI() {
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *panel = new QWidget(this);
    panel->setObjectName("panel");
    panel->setStyleSheet(
        "#panel { background: rgba(6,6,12,0.88); border: 1px solid rgba(255,255,255,0.06);"
        "border-radius: 14px; }");
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Header: file info + waveform + close ──
    auto *header = new QWidget(panel);
    header->setStyleSheet("background: transparent;");
    auto *hdrLayout = new QHBoxLayout(header);
    hdrLayout->setContentsMargins(18, 12, 14, 8);

    m_titleLabel = new QLabel(QStringLiteral("未播放"), header);
    m_titleLabel->setStyleSheet("font-size: 12px; color: rgba(255,255,255,0.6); font-weight: 500; border: none;");

    m_infoLabel = new QLabel("", header);
    m_infoLabel->setStyleSheet("font-size: 10px; color: rgba(255,255,255,0.2); border: none;");

    hdrLayout->addWidget(m_titleLabel);
    hdrLayout->addSpacing(8);
    hdrLayout->addWidget(m_infoLabel);
    hdrLayout->addStretch();

    m_waveformBtn = new QPushButton(QStringLiteral("\xe3\x80\xb0"), header);
    m_waveformBtn->setFixedSize(26, 26);
    m_waveformBtn->setFlat(true);
    m_waveformBtn->setCursor(Qt::PointingHandCursor);
    m_waveformBtn->setToolTip(QStringLiteral("\xe6\xb3\xa2\xe5\xbd\xa2"));
    m_waveformBtn->setStyleSheet(
        "QPushButton { font-size: 12px; color: rgba(255,255,255,0.35); background: transparent; "
        "border: none; border-radius: 6px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.7); background: rgba(255,255,255,0.06); }"
        "QPushButton:pressed { color: rgba(255,255,255,0.5); }");

    m_closeBtn = new QPushButton(QStringLiteral("\xe2\x9c\x95"), header);
    m_closeBtn->setFixedSize(24, 24);
    m_closeBtn->setFlat(true);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(
        "QPushButton { font-size: 12px; color: rgba(255,255,255,0.35); background: transparent; "
        "border: none; border-radius: 6px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.7); background: rgba(255,255,255,0.06); }"
        "QPushButton:pressed { color: rgba(255,255,255,0.5); }");

    hdrLayout->addWidget(m_waveformBtn);
    hdrLayout->addSpacing(4);
    hdrLayout->addWidget(m_closeBtn);

    layout->addWidget(header);

    // ── Separator ──
    auto *sep1 = new QWidget(panel);
    sep1->setFixedHeight(1);
    sep1->setStyleSheet("background: rgba(255,255,255,0.03);");
    layout->addWidget(sep1);

    // ── Progress bar ──
    auto *progressRow = new QWidget(panel);
    progressRow->setStyleSheet("background: transparent;");
    auto *progLayout = new QVBoxLayout(progressRow);
    progLayout->setContentsMargins(0, 0, 0, 4);

    m_progressBar = new VideoProgressBar(progressRow);
    m_progressBar->setFixedHeight(40);
    progLayout->addWidget(m_progressBar);

    auto *timeRow = new QWidget(progressRow);
    timeRow->setStyleSheet("background: transparent;");
    auto *timeLayout = new QHBoxLayout(timeRow);
    timeLayout->setContentsMargins(18, 0, 18, 0);

    m_timeLabel = new QLabel("00:00:00", timeRow);
    m_timeLabel->setStyleSheet("font-size: 11px; color: rgba(255,255,255,0.25); border: none;");

    m_durationLabel = new QLabel("/ 00:00:00", timeRow);
    m_durationLabel->setStyleSheet("font-size: 11px; color: rgba(255,255,255,0.12); border: none;");

    timeLayout->addWidget(m_timeLabel);
    timeLayout->addWidget(m_durationLabel);
    timeLayout->addStretch();

    progLayout->addWidget(timeRow);
    layout->addWidget(progressRow);

    // ── Separator ──
    auto *sep2 = new QWidget(panel);
    sep2->setFixedHeight(1);
    sep2->setStyleSheet("background: rgba(255,255,255,0.03);");
    layout->addWidget(sep2);

    // ── Controls row ──
    auto *ctrlRow = new QWidget(panel);
    ctrlRow->setStyleSheet("background: transparent;");
    auto *ctrlLayout = new QHBoxLayout(ctrlRow);
    ctrlLayout->setContentsMargins(14, 8, 14, 10);
    ctrlLayout->setSpacing(6);

    // Speed button (clickable)
    m_speedBtn = new QPushButton("1.0 \xe2\x96\xbe", ctrlRow);
    m_speedBtn->setFixedHeight(28);
    m_speedBtn->setFixedWidth(52);
    m_speedBtn->setCursor(Qt::PointingHandCursor);
    m_speedBtn->setStyleSheet(
        "QPushButton { font-size: 10px; color: rgba(255,255,255,0.4); background: transparent; "
        "border: 1px solid rgba(255,255,255,0.06); border-radius: 5px; padding: 0 4px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.75); border-color: rgba(255,255,255,0.15); }"
        "QPushButton:pressed { color: rgba(255,255,255,0.55); border-color: rgba(255,255,255,0.1); }");

    ctrlLayout->addWidget(m_speedBtn);
    ctrlLayout->addSpacing(4);

    // Prev / Play / Next
    m_prevBtn = new QPushButton(QStringLiteral("\xe2\x8f\xae"), ctrlRow);
    m_playBtn = new QPushButton(QStringLiteral("\xe2\x8f\xb8"), ctrlRow);
    m_nextBtn = new QPushButton(QStringLiteral("\xe2\x8f\xad"), ctrlRow);

    auto smallBtnStyle = QString(
        "QPushButton { background: transparent; border: none; font-size: 14px; "
        "color: rgba(255,255,255,0.35); border-radius: 6px; padding: 2px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.7); }"
        "QPushButton:pressed { color: rgba(255,255,255,0.5); }");
    m_prevBtn->setStyleSheet(smallBtnStyle);
    m_nextBtn->setStyleSheet(smallBtnStyle);
    m_prevBtn->setFixedSize(30, 30);
    m_nextBtn->setFixedSize(30, 30);
    m_prevBtn->setCursor(Qt::PointingHandCursor);
    m_nextBtn->setCursor(Qt::PointingHandCursor);

    m_playBtn->setFixedSize(40, 40);
    m_playBtn->setCursor(Qt::PointingHandCursor);
    m_playBtn->setStyleSheet(
        "QPushButton { background: rgba(0,212,255,0.08); border: 1px solid rgba(0,212,255,0.1);"
        "border-radius: 20px; font-size: 16px; color: #00d4ff; }"
        "QPushButton:hover { background: rgba(0,212,255,0.15); }");

    ctrlLayout->addWidget(m_prevBtn);
    ctrlLayout->addWidget(m_playBtn);
    ctrlLayout->addWidget(m_nextBtn);

    // Playlist button
    m_playlistBtn = new QPushButton(QStringLiteral("\xf0\x9f\x93\x8b"), ctrlRow);
    m_playlistBtn->setFixedSize(30, 30);
    m_playlistBtn->setFlat(true);
    m_playlistBtn->setCursor(Qt::PointingHandCursor);
    m_playlistBtn->setToolTip(QStringLiteral("\xe8\xa7\x86\xe9\xa2\x91\xe5\x88\x97\xe8\xa1\xa8"));
    m_playlistBtn->setStyleSheet(
        "QPushButton { font-size: 14px; background: transparent; border: none; "
        "color: rgba(255,255,255,0.35); border-radius: 6px; padding: 2px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.7); }"
        "QPushButton:pressed { color: rgba(255,255,255,0.5); }");

    ctrlLayout->addWidget(m_playlistBtn);
    ctrlLayout->addSpacing(8);

    // Volume: mute button + slider
    m_muteBtn = new QPushButton(QStringLiteral("\xf0\x9f\x94\x8a"), ctrlRow);
    m_muteBtn->setFixedSize(24, 24);
    m_muteBtn->setFlat(true);
    m_muteBtn->setCursor(Qt::PointingHandCursor);
    m_muteBtn->setStyleSheet(
        "QPushButton { font-size: 13px; color: rgba(255,255,255,0.35); background: transparent; "
        "border: none; border-radius: 4px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.7); }"
        "QPushButton:pressed { color: rgba(255,255,255,0.5); }");

    m_volSlider = new QSlider(Qt::Horizontal, ctrlRow);
    m_volSlider->setRange(0, 100);
    m_volSlider->setValue(70);
    m_volSlider->setFixedWidth(60);
    m_volSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 3px; background: rgba(255,255,255,0.04); border-radius: 1px; }"
        "QSlider::handle:horizontal { width: 10px; height: 10px; background: rgba(255,255,255,0.25); "
        "border-radius: 5px; margin: -4px 0; }"
        "QSlider::handle:hover { background: rgba(255,255,255,0.4); }"
        "QSlider::sub-page:horizontal { background: rgba(0,212,255,0.5); border-radius: 1px; }");

    m_muteBtn->setToolTip(QStringLiteral("\xe9\x9d\x99\xe9\x9f\xb3"));
    m_volSlider->setToolTip(QStringLiteral("\xe9\x9f\xb3\xe9\x87\x8f"));

    ctrlLayout->addWidget(m_muteBtn);
    ctrlLayout->addWidget(m_volSlider);
    ctrlLayout->addStretch();

    // Fullscreen + settings
    m_fullscreenBtn = new QPushButton(QStringLiteral("\xe2\x9b\xb6"), ctrlRow);
    m_fullscreenBtn->setFixedSize(28, 28);
    m_fullscreenBtn->setFlat(true);
    m_fullscreenBtn->setCursor(Qt::PointingHandCursor);
    m_fullscreenBtn->setToolTip(QStringLiteral("\xe5\x85\xa8\xe5\xb1\x8f"));
    m_fullscreenBtn->setStyleSheet(
        "QPushButton { font-size: 13px; color: rgba(255,255,255,0.35); background: transparent; "
        "border: none; border-radius: 6px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.7); background: rgba(255,255,255,0.06); }"
        "QPushButton:pressed { color: rgba(255,255,255,0.5); }");

    ctrlLayout->addWidget(m_fullscreenBtn);

    layout->addWidget(ctrlRow);

    outerLayout->addWidget(panel);

    // ── Signal connections ──
    connect(m_playBtn, &QPushButton::clicked, this, &SceneControlPanel::playPauseRequested);
    connect(m_speedBtn, &QPushButton::clicked, this, [this]() {
        emit speedMenuRequested(m_speedBtn->mapToGlobal(
            QPoint(m_speedBtn->width() / 2, m_speedBtn->height() + 4)));
    });
    connect(m_progressBar, &VideoProgressBar::seekRequested, this, &SceneControlPanel::seekRequested);
    connect(m_prevBtn, &QPushButton::clicked, this, &SceneControlPanel::prevRequested);
    connect(m_nextBtn, &QPushButton::clicked, this, &SceneControlPanel::nextRequested);
    connect(m_muteBtn, &QPushButton::clicked, this, &SceneControlPanel::muteToggled);
    connect(m_volSlider, &QSlider::valueChanged, this, [this](int val) {
        emit volumeChanged(val / 100.0);
    });
    connect(m_fullscreenBtn, &QPushButton::clicked, this, &SceneControlPanel::fullscreenRequested);
    connect(m_waveformBtn, &QPushButton::clicked, this, &SceneControlPanel::waveformToggleRequested);
    connect(m_playlistBtn, &QPushButton::clicked, this, &SceneControlPanel::showPlaylistRequested);
    connect(m_closeBtn, &QPushButton::clicked, this, &SceneControlPanel::closeRequested);
}

void SceneControlPanel::updateMediaInfo(const MediaInfo &info) {
    m_titleLabel->setText(info.fileName);
    QString extra;
    if (info.width > 0 && info.height > 0)
        extra = QString(" \xc2\xb7 %1\xc3\x97%2").arg(info.width).arg(info.height);
    m_infoLabel->setText(QString("%1%2").arg(SceneTypeName(info.scene)).arg(extra));
    m_durationLabel->setText("/ " + formatTime(info.duration));
}

void SceneControlPanel::updatePosition(double posSec, double durationSec) {
    m_timeLabel->setText(formatTime(posSec));
    m_durationLabel->setText("/ " + formatTime(durationSec));
}

void SceneControlPanel::updateProgress(double ratio) {
    m_progressBar->setProgress(ratio);
}

void SceneControlPanel::setPlayButtonState(bool playing) {
    m_playBtn->setText(playing ? QStringLiteral("\xe2\x8f\xb8") : QStringLiteral("\xe2\x96\xb6"));
}

void SceneControlPanel::setSpeed(double speed) {
    QString text;
    if (speed == int(speed))
        text = QString::number(int(speed)) + ".0";
    else
        text = QString::number(speed, 'f', 2);
    m_speedBtn->setText(text + " \xe2\x96\xbe");
}

void SceneControlPanel::setVolume(double vol) {
    m_volSlider->blockSignals(true);
    m_volSlider->setValue(qRound(vol * 100));
    m_volSlider->blockSignals(false);
}

void SceneControlPanel::setMuted(bool muted) {
    m_muteBtn->setText(muted ? QStringLiteral("\xf0\x9f\x94\x87") : QStringLiteral("\xf0\x9f\x94\x8a"));
}

void SceneControlPanel::setPrevNextEnabled(bool hasPrev, bool hasNext) {
    m_prevBtn->setEnabled(hasPrev);
    m_nextBtn->setEnabled(hasNext);
    auto alpha = [](bool en) { return en ? "0.35" : "0.06"; };
    m_prevBtn->setStyleSheet(QString(
        "QPushButton { background: transparent; border: none; font-size: 14px; "
        "color: rgba(255,255,255,%1); border-radius: 6px; padding: 2px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.7); }"
        "QPushButton:pressed { color: rgba(255,255,255,0.5); }").arg(alpha(hasPrev)));
    m_nextBtn->setStyleSheet(QString(
        "QPushButton { background: transparent; border: none; font-size: 14px; "
        "color: rgba(255,255,255,%1); border-radius: 6px; padding: 2px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.7); }"
        "QPushButton:pressed { color: rgba(255,255,255,0.5); }").arg(alpha(hasNext)));
}

void SceneControlPanel::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_userDragged = true;
        m_dragStart = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
    QWidget::mousePressEvent(event);
}

void SceneControlPanel::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragStart);
        event->accept();
    }
    QWidget::mouseMoveEvent(event);
}

void SceneControlPanel::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton)
        m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

QString SceneControlPanel::formatTime(double sec) const {
    if (sec < 0 || !std::isfinite(sec)) return "00:00:00";
    int h = (int)sec / 3600;
    int m = ((int)sec % 3600) / 60;
    int s = (int)sec % 60;
    return QString("%1:%2:%3").arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}
