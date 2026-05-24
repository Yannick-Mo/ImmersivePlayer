#include "LoadingOverlay.h"
#include <QPainter>

LoadingOverlay::LoadingOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    hide();

    connect(&m_animTimer, &QTimer::timeout, this, &LoadingOverlay::onAnimTick);
    connect(&m_delayTimer, &QTimer::timeout, this, &LoadingOverlay::onDelayedShow);
    m_delayTimer.setSingleShot(true);
}

void LoadingOverlay::showWithDelay(int delayMs) {
    if (m_delayedShowPending) return;
    m_delayedShowPending = true;
    m_delayTimer.start(delayMs);
}

void LoadingOverlay::hideImmediately() {
    m_delayedShowPending = false;
    m_delayTimer.stop();
    if (isVisible()) {
        hide();
        m_animTimer.stop();
    }
}

void LoadingOverlay::onDelayedShow() {
    if (!m_delayedShowPending) return;
    m_angle = 0;
    m_animTimer.start(50);
    show();
    raise();
    QWidget *p = parentWidget();
    if (p) {
        resize(p->size());
        move(0, 0);
    }
}

void LoadingOverlay::onAnimTick() {
    m_angle = (m_angle + 30) % 360;
    update();
}

void LoadingOverlay::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.fillRect(rect(), QColor(6, 6, 12, 179));

    int cx = width() / 2;
    int cy = height() / 2;
    int r = 18;

    p.setPen(QPen(QColor(0, 212, 255, 200), 3, Qt::SolidLine, Qt::RoundCap));
    int startAngle = m_angle * 16;
    int spanAngle = 270 * 16;
    p.drawArc(cx - r, cy - r, r * 2, r * 2, startAngle, spanAngle);

    p.setPen(QColor(255, 255, 255, 140));
    QFont f = p.font();
    f.setPointSize(10);
    p.setFont(f);
    p.drawText(QRect(cx - 60, cy + 24, 120, 20), Qt::AlignCenter, QStringLiteral("\xe5\x8a\xa0\xe8\xbd\xbd\xe4\xb8\xad\xe2\x80\xa6"));
}
