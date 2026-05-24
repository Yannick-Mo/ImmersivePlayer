#include "TitleBar.h"
#include "common/Constants.h"
#include <QApplication>
#include <QScreen>
#include <QGuiApplication>

TitleBar::TitleBar(QWidget *parent) : QWidget(parent) {
    setFixedHeight(56);
    setStyleSheet("background: #101114;");

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(28, 0, 0, 0);
    layout->setSpacing(0);

    auto *logo = new QLabel("IP", this);
    logo->setStyleSheet("color: #00d4ff; font-weight: 800; font-size: 16px;"
                        "border: 1.5px solid #00d4ff; border-radius: 8px; padding: 4px 10px;");

    m_titleLabel = new QLabel("IMMERSIVE PLAYER", this);
    m_titleLabel->setStyleSheet("font-weight: 700; font-size: 15px; letter-spacing: 3px; color: #d0d8e8;"
                                "background: transparent; border: none;");

    auto *version = new QLabel(QString("v%1").arg(Constants::AppVersion), this);
    version->setStyleSheet("font-size: 9px; color: rgba(255,255,255,0.12); background: transparent;");

    for (auto *w : {logo, m_titleLabel, version})
        w->setAttribute(Qt::WA_TransparentForMouseEvents);

    layout->addWidget(logo);
    layout->addSpacing(10);
    layout->addWidget(m_titleLabel);
    layout->addSpacing(4);
    layout->addWidget(version);
    layout->addSpacing(24);

    auto *sceneTab = new QLabel(QStringLiteral("\360\237\216\256 \345\234\272\346\231\257"), this);
    sceneTab->setStyleSheet("font-size: 11px; color: #00d4ff; padding: 0 12px;");
    sceneTab->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *mediaTab = new QPushButton(QStringLiteral("\360\237\223\201 \345\252\222\344\275\223\345\272\223"), this);
    mediaTab->setFlat(true);
    mediaTab->setCursor(Qt::PointingHandCursor);
    mediaTab->setStyleSheet(
        "QPushButton { font-size: 11px; color: rgba(255,255,255,0.25); padding: 0 12px; border: none; background: transparent; }"
        "QPushButton:hover { color: #00d4ff; }");

    auto *settingsTab = new QLabel(QStringLiteral("\342\232\231 \350\256\276\347\275\256"), this);
    settingsTab->setStyleSheet("font-size: 11px; color: rgba(255,255,255,0.25); padding: 0 12px;");
    settingsTab->setAttribute(Qt::WA_TransparentForMouseEvents);

    connect(mediaTab, &QPushButton::clicked, this, &TitleBar::mediaLibraryClicked);

    layout->addWidget(sceneTab);
    layout->addWidget(mediaTab);
    layout->addWidget(settingsTab);
    layout->addStretch();

    auto makeBtn = [this](const QString &text, int w) {
        auto *btn = new QPushButton(text, this);
        btn->setFixedSize(w, 56);
        btn->setCursor(Qt::ArrowCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        return btn;
    };

    m_minBtn = makeBtn(QStringLiteral("\342\200\225"), 46);
    m_maxBtn = makeBtn(QStringLiteral("\342\226\241"), 46);
    m_closeBtn = makeBtn(QStringLiteral("\342\234\225"), 46);

    m_minBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 0;"
        "font-size: 11px; color: rgba(255,255,255,0.30); }"
        "QPushButton:hover { color: rgba(255,255,255,0.80); }");
    m_maxBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 0;"
        "font-size: 11px; color: rgba(255,255,255,0.30); }"
        "QPushButton:hover { color: rgba(255,255,255,0.80); }");
    m_closeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 0;"
        "font-size: 12px; color: rgba(255,255,255,0.30); }"
        "QPushButton:hover { color: white; }"
        "QPushButton:pressed { font-size: 15px; color: white; }");

    connect(m_minBtn, &QPushButton::clicked, this, &TitleBar::minimizeClicked);
    connect(m_maxBtn, &QPushButton::clicked, this, &TitleBar::maximizeClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &TitleBar::closeClicked);

    layout->addWidget(m_minBtn);
    layout->addWidget(m_maxBtn);
    layout->addWidget(m_closeBtn);
}

void TitleBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && window()) {
        QPoint clickLocal = event->pos();

        if (window()->isMaximized()) {
            QCursor cur;
            QPoint cursorScreen = cur.pos();

            QRect maxGeo = window()->geometry();
            double relX = maxGeo.width()  > 0 ? (double)(cursorScreen.x() - maxGeo.x()) / maxGeo.width()  : 0.5;
            double relY = maxGeo.height() > 0 ? (double)(cursorScreen.y() - maxGeo.y()) / maxGeo.height() : 0.5;

            window()->showNormal();
            QApplication::processEvents();

            QRect normalGeo = window()->geometry();
            window()->move(cursorScreen.x() - (int)(normalGeo.width()  * relX),
                           cursorScreen.y() - (int)(normalGeo.height() * relY));

            m_dragging = true;
            m_dragPos = cursorScreen;
        } else {
            QCursor cur;
            m_dragging = true;
            m_dragPos = cur.pos();
        }
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging && window()) {
        QPoint current = event->globalPosition().toPoint();
        QPoint delta = current - m_dragPos;
        window()->move(window()->pos() + delta);
        m_dragPos = current;
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton)
        m_dragging = false;
}

void TitleBar::updateMaximizeButton(bool maximized) {
    m_maxBtn->setText(maximized
        ? QStringLiteral("\342\247\211")
        : QStringLiteral("\342\226\241"));
}
