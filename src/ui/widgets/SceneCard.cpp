#include "SceneCard.h"
#include "common/Constants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>

SceneCard::SceneCard(SceneType type, QWidget *parent)
    : QFrame(parent), m_type(type) {
    switch (type) {
        case SceneType::Concert:   m_themeColor = Constants::ColorConcert; break;
        case SceneType::TechPlaza: m_themeColor = Constants::ColorTechPlaza; break;
        case SceneType::Cinema:    m_themeColor = Constants::ColorCinema; break;
        case SceneType::Normal:    m_themeColor = Constants::ColorNormal; break;
    }

    setFixedSize(240, 180);
    setCursor(Qt::PointingHandCursor);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedHeight(110);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    QFont iconFont = m_iconLabel->font();
    iconFont.setPointSize(28);
    m_iconLabel->setFont(iconFont);

    auto *textWidget = new QWidget(this);
    textWidget->setStyleSheet("background: transparent;");
    auto *textLayout = new QVBoxLayout(textWidget);
    textLayout->setContentsMargins(14, 10, 14, 14);
    textLayout->setSpacing(4);

    m_titleLabel = new QLabel(SceneTypeName(type), textWidget);
    m_titleLabel->setStyleSheet("color: #c8d8ff; font-size: 13px; font-weight: 500;");

    auto *titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(6);
    titleRow->addWidget(m_titleLabel);

    const char *status = (type == SceneType::Normal || type == SceneType::Concert || type == SceneType::Cinema || type == SceneType::TechPlaza) ? "可用" : "";
    m_statusLabel = new QLabel(status, textWidget);
    m_statusLabel->setStyleSheet("color: rgba(255,255,255,0.35); font-size: 9px;");
    titleRow->addWidget(m_statusLabel);
    titleRow->addStretch();
    textLayout->addLayout(titleRow);

    m_descLabel = new QLabel(textWidget);
    m_descLabel->setStyleSheet("color: rgba(255,255,255,0.2); font-size: 10px;");
    m_descLabel->setWordWrap(true);
    textLayout->addWidget(m_descLabel);
    layout->addWidget(m_iconLabel);
    layout->addWidget(textWidget);

    switch (type) {
        case SceneType::Concert:
            m_iconLabel->setText("🎤");
            m_descLabel->setText("舞台主屏体验 · 观众互动光效");
            break;
        case SceneType::TechPlaza:
            m_iconLabel->setText("🏙️");
            m_descLabel->setText("建筑立面大屏投影 · 科技感沉浸");
            break;
        case SceneType::Cinema:
            m_iconLabel->setText("🎬");
            m_descLabel->setText("经典黑暗影院 · 银幕投影氛围");
            break;
        case SceneType::Normal:
            m_iconLabel->setText("▶️");
            m_descLabel->setText("标准全功能播放器 · 无虚拟场景");
            break;
    }
}

void SceneCard::setEnabled(bool enabled) {
    m_enabled = enabled;
    setCursor(enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
    m_descLabel->setStyleSheet(
        QString("color: rgba(255,255,255,%1); font-size: 10px;")
            .arg(enabled ? "0.2" : "0.1")
    );
    update();
}

void SceneCard::mousePressEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    if (m_enabled) {
        emit clicked(m_type);
    }
}

void SceneCard::enterEvent(QEnterEvent *event) {
    Q_UNUSED(event);
    if (m_enabled) {
        m_hovered = true;
        update();
    }
}

void SceneCard::leaveEvent(QEvent *event) {
    Q_UNUSED(event);
    m_hovered = false;
    update();
}

void SceneCard::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(rect(), 12, 12);

    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0, QColor(m_themeColor.red()/12, m_themeColor.green()/12, m_themeColor.blue()/12, m_enabled ? 250 : 80));
    bg.setColorAt(1, QColor(4, 4, 8, 252));
    p.fillPath(path, bg);

    QLinearGradient lineGrad(0, 0, width(), 0);
    lineGrad.setColorAt(0, Qt::transparent);
    QColor accent = m_themeColor;
    accent.setAlpha(m_enabled ? 120 : 10);
    lineGrad.setColorAt(0.5, accent);
    lineGrad.setColorAt(1, Qt::transparent);
    p.fillRect(0, 0, width(), 2, lineGrad);

    if (m_hovered) {
        QColor glow = m_themeColor;
        glow.setAlpha(8);
        p.fillPath(path, glow);
    }

    QColor borderColor = m_themeColor;
    borderColor.setAlpha(m_enabled ? (m_hovered ? 100 : 12) : 4);
    p.setPen(QPen(borderColor, 1));
    p.drawPath(path);
}
