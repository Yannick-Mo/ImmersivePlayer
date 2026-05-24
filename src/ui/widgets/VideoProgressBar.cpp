#include "VideoProgressBar.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

VideoProgressBar::VideoProgressBar(QWidget *parent) : QWidget(parent) {
    setFixedHeight(48);
    setMouseTracking(true);
}

void VideoProgressBar::setProgress(double ratio) {
    m_progress = qBound(0.0, ratio, 1.0);
    update();
}

void VideoProgressBar::setBuffered(double ratio) {
    m_buffered = qBound(0.0, ratio, 1.0);
    update();
}

void VideoProgressBar::setPreviewText(const QString &text) {
    m_previewText = text;
    update();
}

void VideoProgressBar::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int barY = height() / 2 - 2;
    const int barH = 4;
    const int r = barH / 2;

    // Background track
    p.fillRect(0, barY, width(), barH, QColor(255, 255, 255, 15));

    // Buffered
    if (m_buffered > 0) {
        p.fillRect(0, barY, width() * m_buffered, barH, QColor(255, 255, 255, 10));
    }

    // Played (blue -> purple gradient)
    if (m_progress > 0) {
        QLinearGradient grad(0, 0, width() * m_progress, 0);
        grad.setColorAt(0, QColor(0, 212, 255));
        grad.setColorAt(1, QColor(102, 68, 255));
        QPainterPath playedPath;
        playedPath.addRoundedRect(0, barY, width() * m_progress, barH, r, r);
        p.fillPath(playedPath, grad);
    }

    // Drag dot
    if (m_progress > 0) {
        int dotX = width() * m_progress - 5;
        int dotY = barY - 3;
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawEllipse(dotX, dotY, 10, 10);
    }

    // Hover preview time
    if (m_hoverPos >= 0 && !m_previewText.isEmpty()) {
        QFont f = p.font();
        f.setPointSize(8);
        p.setFont(f);
        QFontMetrics fm(f);
        int tw = fm.horizontalAdvance(m_previewText) + 10;
        int tx = qBound(0, (int)(m_hoverPos * width() - tw/2), width() - tw);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 200));
        p.drawRoundedRect(tx, barY - 22, tw, 16, 3, 3);
        p.setPen(Qt::white);
        p.drawText(tx + 5, barY - 10, m_previewText);
    }
}

void VideoProgressBar::mouseMoveEvent(QMouseEvent *event) {
    if (width() <= 0) return;
    m_hoverPos = event->position().x() / width();
    emit previewAt(m_hoverPos);
    update();
}

void VideoProgressBar::mousePressEvent(QMouseEvent *event) {
    if (width() <= 0) return;
    double ratio = event->position().x() / width();
    emit seekRequested(qBound(0.0, ratio, 1.0));
    update();
}

void VideoProgressBar::leaveEvent(QEvent *event) {
    Q_UNUSED(event);
    m_hoverPos = -1.0;
    update();
}
