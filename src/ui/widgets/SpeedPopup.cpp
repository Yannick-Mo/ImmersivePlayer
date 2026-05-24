#include "SpeedPopup.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QKeyEvent>
#include <QScreen>
#include <QGuiApplication>

static const double kSpeeds[] = {
    0.50, 0.75, 1.00, 1.25, 1.50,
    1.75, 2.00, 2.50, 3.00, 4.00
};
static constexpr int kSpeedCount = sizeof(kSpeeds) / sizeof(kSpeeds[0]);

static QString formatSpeed(double s) {
    if (s == int(s))
        return QString::number(int(s)) + ".00x";
    if (s * 100 == int(s * 100))
        return QString::number(s, 'f', 2) + "x";
    return QString::number(s, 'f', s >= 1.0 ? 2 : 2) + "x";
}

SpeedPopup::SpeedPopup(QWidget *parent)
    : QFrame(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Popup | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    setFixedWidth(180);

    setupUI();
}

void SpeedPopup::setupUI() {
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *panel = new QWidget(this);
    panel->setObjectName("speedPanel");
    panel->setStyleSheet(
        "#speedPanel {"
        "  background: rgba(10, 10, 22, 0.96);"
        "  border: 1px solid rgba(255, 255, 255, 0.08);"
        "  border-radius: 10px;"
        "}");
    auto *grid = new QGridLayout(panel);
    grid->setContentsMargins(6, 6, 6, 6);
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(2);

    int mid = (kSpeedCount + 1) / 2;  // 7 items in column 0, 6 in column 1
    QString baseStyle =
        "QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 8px;"
        "  font-size: 12px;"
        "  color: rgba(255,255,255,0.45);"
        "  text-align: left;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(255,255,255,0.06);"
        "  color: rgba(255,255,255,0.7);"
        "}";

    for (int i = 0; i < kSpeedCount; ++i) {
        auto *btn = new QPushButton(formatSpeed(kSpeeds[i]), panel);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(baseStyle);
        btn->setProperty("speedIndex", i);

        int col = (i < mid) ? 0 : 1;
        int row = (i < mid) ? i : (i - mid);
        grid->addWidget(btn, row, col);

        connect(btn, &QPushButton::clicked, this, [this, i]() {
            emit speedSelected(kSpeeds[i]);
            close();
        });

        m_buttons.append(btn);
    }

    outerLayout->addWidget(panel);
}

void SpeedPopup::showAt(const QPoint &globalPos) {
    setCurrentSpeed(m_currentIndex >= 0 ? kSpeeds[m_currentIndex] : 1.0);

    adjustSize();

    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen) screen = QGuiApplication::primaryScreen();
    QRect screenGeom = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    int x = globalPos.x() - width() / 2;
    int y = globalPos.y();

    if (x + width() > screenGeom.right())
        x = screenGeom.right() - width();
    if (x < screenGeom.left())
        x = screenGeom.left();
    if (y + height() > screenGeom.bottom())
        y = globalPos.y() - height();

    move(x, y);
    show();
    raise();
    setFocus();
}

void SpeedPopup::setCurrentSpeed(double speed) {
    int idx = -1;
    for (int i = 0; i < kSpeedCount; ++i) {
        if (qFuzzyCompare(kSpeeds[i], speed)) {
            idx = i;
            break;
        }
    }
    // If exact match not found, find closest
    if (idx < 0) {
        double minDiff = 999.0;
        for (int i = 0; i < kSpeedCount; ++i) {
            double diff = qAbs(kSpeeds[i] - speed);
            if (diff < minDiff) {
                minDiff = diff;
                idx = i;
            }
        }
    }
    setSelected(idx);
}

void SpeedPopup::setSelected(int index) {
    m_currentIndex = index;
    QString normalStyle =
        "QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 8px;"
        "  font-size: 12px;"
        "  color: rgba(255,255,255,0.45);"
        "  text-align: left;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(255,255,255,0.06);"
        "  color: rgba(255,255,255,0.7);"
        "}";
    QString selectedStyle =
        "QPushButton {"
        "  background: rgba(0,212,255,0.08);"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 8px;"
        "  font-size: 12px;"
        "  color: #00d4ff;"
        "  font-weight: 600;"
        "  text-align: left;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(0,212,255,0.14);"
        "  color: #33ddff;"
        "}";

    for (int i = 0; i < m_buttons.size(); ++i) {
        m_buttons[i]->setStyleSheet(i == index ? selectedStyle : normalStyle);
    }
}

void SpeedPopup::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        close();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_currentIndex >= 0) {
            emit speedSelected(kSpeeds[m_currentIndex]);
            close();
        }
    } else if (event->key() == Qt::Key_Up) {
        int mid = (kSpeedCount + 1) / 2;
        QVector<int> order;
        // Traverse column 0 then column 1
        for (int col = 0; col < 2; ++col) {
            for (int row = 0; row < mid; ++row) {
                int idx = (col == 0) ? row : (row + mid);
                if (idx < kSpeedCount)
                    order.append(idx);
            }
        }
        int cur = order.indexOf(m_currentIndex);
        if (cur > 0)
            setSelected(order[cur - 1]);
    } else if (event->key() == Qt::Key_Down) {
        int mid = (kSpeedCount + 1) / 2;
        QVector<int> order;
        for (int col = 0; col < 2; ++col) {
            for (int row = 0; row < mid; ++row) {
                int idx = (col == 0) ? row : (row + mid);
                if (idx < kSpeedCount)
                    order.append(idx);
            }
        }
        int cur = order.indexOf(m_currentIndex);
        if (cur < order.size() - 1)
            setSelected(order[cur + 1]);
    } else if (event->key() == Qt::Key_Right) {
        int mid = (kSpeedCount + 1) / 2;
        if (m_currentIndex < mid) {
            int rightIdx = m_currentIndex + mid;
            if (rightIdx < kSpeedCount)
                setSelected(rightIdx);
        }
    } else if (event->key() == Qt::Key_Left) {
        int mid = (kSpeedCount + 1) / 2;
        if (m_currentIndex >= mid) {
            setSelected(m_currentIndex - mid);
        }
    } else {
        QFrame::keyPressEvent(event);
    }
}
