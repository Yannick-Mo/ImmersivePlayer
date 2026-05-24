#include "HistoryDialog.h"
#include "ui/SessionManager.h"
#include "ui/widgets/ConfirmDialog.h"
#include "common/Constants.h"
#include <QDateTime>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QMouseEvent>
#include <QPointer>
#include <QTimer>

HistoryDialog::HistoryDialog(SessionManager *manager, QWidget *parent)
    : QDialog(parent), m_manager(manager)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setFixedSize(520, 520);
    setStyleSheet(QString("background-color: %1;").arg(Constants::ColorBg.name()));
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ===== Header =====
    auto *header = new QWidget(this);
    header->setFixedHeight(52);
    header->setStyleSheet("border-bottom: 1px solid rgba(255,255,255,0.04);");
    auto *hdrLayout = new QHBoxLayout(header);
    hdrLayout->setContentsMargins(24, 0, 24, 0);

    auto *title = new QLabel(QStringLiteral("\xf0\x9f\x93\x8b \xe6\x89\x80\xe6\x9c\x89\xe6\x92\xad\xe6\x94\xbe\xe8\xae\xb0\xe5\xbd\x95"), header);
    title->setStyleSheet("font-size: 13px; font-weight: 500; color: rgba(255,255,255,0.7); letter-spacing: 2px; border: none;");

    auto *clearBtn = new QPushButton(QStringLiteral("\xe6\xb8\x85\xe7\xa9\xba"), header);
    clearBtn->setFixedSize(48, 24);
    clearBtn->setFlat(true);
    clearBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setStyleSheet(
        "QPushButton { font-size: 10px; color: rgba(255,255,255,0.15); background: transparent; border: 1px solid rgba(255,255,255,0.06); border-radius: 6px; }"
        "QPushButton:hover { color: #ff4444; border-color: rgba(255,68,68,0.3); background: rgba(255,68,68,0.06); }"
        "QPushButton:pressed { color: white; background: rgba(255,68,68,0.25); border-color: rgba(255,68,68,0.5); }");

    auto *closeBtn = new QPushButton(QStringLiteral("\xe2\x9c\x95"), header);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setFlat(true);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        "QPushButton { font-size: 15px; color: rgba(255,255,255,0.15); background: transparent; border: none; border-radius: 8px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.5); background: rgba(255,255,255,0.06); }");

    hdrLayout->addWidget(title);
    hdrLayout->addStretch();
    hdrLayout->addWidget(clearBtn);
    hdrLayout->addSpacing(8);
    hdrLayout->addWidget(closeBtn);

    header->installEventFilter(this);
    mainLayout->addWidget(header);

    // ===== Scrollable list =====
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { width: 6px; background: transparent; margin: 0; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.07); border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(255,255,255,0.14); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }");

    auto *listContainer = new QWidget(scrollArea);
    listContainer->setStyleSheet("background: transparent;");
    m_listLayout = new QVBoxLayout(listContainer);
    m_listLayout->setContentsMargins(24, 12, 20, 20);
    m_listLayout->setSpacing(6);
    m_listLayout->addStretch();

    scrollArea->setWidget(listContainer);
    mainLayout->addWidget(scrollArea, 1);

    // ===== Connections =====
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    QPointer<QPushButton> clearBtnPtr = clearBtn;
    connect(clearBtn, &QPushButton::clicked, this, [this, clearBtnPtr]() {
        bool ok = ConfirmDialog::confirm(
            this,
            QStringLiteral("\xe6\xb8\x85\xe7\xa9\xba\xe6\x92\xad\xe6\x94\xbe\xe8\xae\xb0\xe5\xbd\x95"),
            QStringLiteral("\xe7\xa1\xae\xe5\xae\x9a\xe8\xa6\x81\xe6\xb8\x85\xe7\xa9\xba\xe6\x89\x80\xe6\x9c\x89\xe6\x92\xad\xe6\x94\xbe\xe8\xae\xb0\xe5\xbd\x95\xe5\x90\x97\xef\xbc\x9f\xe6\xad\xa4\xe6\x93\x8d\xe4\xbd\x9c\xe4\xb8\x8d\xe5\x8f\xaf\xe6\x81\xa2\xe5\xa4\x8d\xe3\x80\x82"),
            QStringLiteral("\xe7\xa1\xae\xe8\xae\xa4\xe6\xb8\x85\xe7\xa9\xba"));
        if (ok) {
            if (clearBtnPtr) {
                clearBtnPtr->setStyleSheet(
                    "QPushButton { font-size: 10px; color: white; background: rgba(255,68,68,0.25); "
                    "border: 1px solid rgba(255,68,68,0.5); border-radius: 6px; }");
                QTimer::singleShot(250, this, [clearBtnPtr]() {
                    if (clearBtnPtr) {
                        clearBtnPtr->setStyleSheet(
                            "QPushButton { font-size: 10px; color: rgba(255,255,255,0.15); background: transparent; border: 1px solid rgba(255,255,255,0.06); border-radius: 6px; }"
                            "QPushButton:hover { color: #ff4444; border-color: rgba(255,68,68,0.3); background: rgba(255,68,68,0.06); }"
                            "QPushButton:pressed { color: white; background: rgba(255,68,68,0.25); border-color: rgba(255,68,68,0.5); }");
                    }
                });
            }
            m_manager->clearHistory();
        }
    });
    connect(m_manager, &SessionManager::historyChanged, this, [this]() { refresh(); });
    connect(this, &HistoryDialog::historyItemClicked, this, &HistoryDialog::onItemClicked);

    refresh();
}

void HistoryDialog::refresh() {
    QLayoutItem *child;
    while ((child = m_listLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    const auto &history = m_manager->history();
    if (history.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("\xe6\x9a\x82\xe6\x97\xa0\xe6\x92\xad\xe6\x94\xbe\xe8\xae\xb0\xe5\xbd\x95"), this);
        empty->setStyleSheet("font-size: 11px; color: rgba(255,255,255,0.1); border: none;");
        empty->setAlignment(Qt::AlignCenter);
        m_listLayout->addWidget(empty);
        m_listLayout->addStretch();
        return;
    }

    for (const auto &h : history) {
        m_listLayout->addWidget(createHistoryItemWidget(h));
    }
    m_listLayout->addStretch();
}

QWidget* HistoryDialog::createHistoryItemWidget(const HistoryItem &item) {
    auto *btn = new QPushButton(this);
    btn->setFlat(true);
    btn->setFixedHeight(60);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        "QPushButton { background: #18191d; border: 1px solid transparent; border-radius: 12px; }"
        "QPushButton:hover { background: #1e1f24; border-color: rgba(0,212,255,0.08); }");
    btn->setText(QString());

    auto *layout = new QHBoxLayout(btn);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(14);
    layout->setAlignment(Qt::AlignVCenter);

    // Scene icon
    QString sceneIcon;
    QString iconBgColor, iconBorderColor;
    switch (item.scene) {
        case SceneType::Concert:
            sceneIcon = QStringLiteral("\xf0\x9f\x8e\xa4");
            iconBgColor = "rgba(255,0,100,0.06)";
            iconBorderColor = "rgba(255,0,100,0.1)";
            break;
        case SceneType::TechPlaza:
            sceneIcon = QStringLiteral("\xf0\x9f\x8f\x99\xef\xb8\x8f");
            iconBgColor = "rgba(0,212,255,0.06)";
            iconBorderColor = "rgba(0,212,255,0.1)";
            break;
        case SceneType::Cinema:
            sceneIcon = QStringLiteral("\xf0\x9f\x8e\xac");
            iconBgColor = "rgba(255,180,50,0.06)";
            iconBorderColor = "rgba(255,180,50,0.1)";
            break;
        case SceneType::Normal:
            sceneIcon = QStringLiteral("\xe2\x96\xb6\xef\xb8\x8f");
            iconBgColor = "rgba(102,102,255,0.06)";
            iconBorderColor = "rgba(102,102,255,0.1)";
            break;
    }
    auto *icon = new QLabel(sceneIcon, btn);
    icon->setFixedSize(40, 40);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QString("font-size: 18px; background: %1; border: 1px solid %2; border-radius: 8px;")
                        .arg(iconBgColor, iconBorderColor));

    // Info column
    auto *textCol = new QVBoxLayout;
    textCol->setSpacing(2);
    textCol->setAlignment(Qt::AlignVCenter);

    auto *nameLabel = new QLabel(item.file.fileName, btn);
    nameLabel->setStyleSheet("font-size: 13px; font-weight: 500; color: #d4dcec; border: none;");
    nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    qint64 diff = QDateTime::currentSecsSinceEpoch() - item.timestamp;
    QString timeAgo;
    if (diff < 60) timeAgo = QStringLiteral("\xe5\x88\x9a\xe5\x88\x9a");
    else if (diff < 3600) timeAgo = QString("%1 \xe5\x88\x86\xe9\x92\x9f\xe5\x89\x8d").arg(diff / 60);
    else if (diff < 86400) timeAgo = QString("%1 \xe5\xb0\x8f\xe6\x97\xb6\xe5\x89\x8d").arg(diff / 3600);
    else timeAgo = QString("%1 \xe5\xa4\xa9\xe5\x89\x8d").arg(diff / 86400);

    auto *metaLabel = new QLabel(
        QString("%1 \xc2\xb7 %2").arg(SceneTypeName(item.scene)).arg(timeAgo), btn);
    metaLabel->setStyleSheet("font-size: 11px; color: rgba(255,255,255,0.2); border: none;");
    metaLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    textCol->addWidget(nameLabel);
    textCol->addWidget(metaLabel);

    // Scene tag
    QString tagColor, tagBg;
    switch (item.scene) {
        case SceneType::Concert:   tagColor = "#ff0064"; tagBg = "rgba(255,0,100,0.12)"; break;
        case SceneType::TechPlaza: tagColor = "#00d4ff"; tagBg = "rgba(0,212,255,0.12)"; break;
        case SceneType::Cinema:    tagColor = "#ffb432"; tagBg = "rgba(255,180,50,0.12)"; break;
        case SceneType::Normal:    tagColor = "#6666ff"; tagBg = "rgba(102,102,255,0.12)"; break;
    }
    auto *tag = new QLabel(SceneTypeName(item.scene), btn);
    tag->setStyleSheet(QString("font-size: 10px; font-weight: 500; color: %1; background: %2; "
                               "border: none; border-radius: 10px; padding: 3px 10px;")
                       .arg(tagColor, tagBg));
    tag->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Play arrow
    auto *arrow = new QLabel(QStringLiteral("\xe2\x96\xb6"), btn);
    arrow->setFixedSize(30, 30);
    arrow->setAlignment(Qt::AlignCenter);
    arrow->setStyleSheet(
        "font-size: 12px; font-weight: bold; color: white; "
        "background: rgba(0,212,255,0.15); border-radius: 15px; "
        "border: 1px solid rgba(0,212,255,0.25);");
    arrow->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Delete button
    auto *delBtn = new QPushButton(QStringLiteral("\xc3\x97"), btn);
    delBtn->setFixedSize(26, 26);
    delBtn->setFlat(true);
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setStyleSheet(
        "QPushButton { font-size: 14px; color: rgba(255,255,255,0.08); background: transparent; "
        "border: none; border-radius: 13px; }"
        "QPushButton:hover { color: #ff4444; background: rgba(255,68,68,0.12); }");

    layout->addWidget(icon, 0);
    layout->addLayout(textCol, 1);
    layout->addWidget(tag, 0);
    layout->addWidget(arrow, 0);
    layout->addSpacing(4);
    layout->addWidget(delBtn, 0);

    HistoryItem h = item;
    connect(btn, &QPushButton::clicked, this, [this, h]() {
        emit historyItemClicked(h);
    });

    // Find index of this item in history
    QPointer<QPushButton> delBtnPtr = delBtn;
    connect(delBtn, &QPushButton::clicked, this, [this, h, delBtnPtr]() {
        const auto &hist = m_manager->history();
        for (int i = 0; i < hist.size(); i++) {
            if (hist[i].file.filePath == h.file.filePath &&
                hist[i].scene == h.scene &&
                hist[i].timestamp == h.timestamp) {
                m_manager->removeHistory(i);
                break;
            }
        }
    });

    return btn;
}

void HistoryDialog::onItemClicked(const HistoryItem &item) {
    close();
}

bool HistoryDialog::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            m_dragPos = me->globalPosition().toPoint() - frameGeometry().topLeft();
            m_dragging = true;
            return true;
        }
    } else if (event->type() == QEvent::MouseMove && m_dragging) {
        auto *me = static_cast<QMouseEvent*>(event);
        move(me->globalPosition().toPoint() - m_dragPos);
        return true;
    } else if (event->type() == QEvent::MouseButtonRelease) {
        m_dragging = false;
        return true;
    }
    return QDialog::eventFilter(obj, event);
}
