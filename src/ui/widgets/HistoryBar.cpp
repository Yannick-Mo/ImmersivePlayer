#include "HistoryBar.h"
#include "ui/SessionManager.h"
#include "common/Constants.h"
#include <QDateTime>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>

HistoryBar::HistoryBar(SessionManager *manager, QWidget *parent)
    : QWidget(parent), m_manager(manager)
{
    setMinimumHeight(100);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto *header = new QWidget(this);
    auto *hdrLayout = new QHBoxLayout(header);
    hdrLayout->setContentsMargins(24, 10, 24, 6);

    auto *title = new QLabel(QStringLiteral("\xf0\x9f\x93\x8b \xe6\x9c\x80\xe8\xbf\x91\xe6\x92\xad\xe6\x94\xbe"), header);
    title->setStyleSheet("font-size: 11px; color: rgba(255,255,255,0.2); letter-spacing: 2px; font-weight: 500;");

    auto *viewAll = new QPushButton("\xe6\x9f\xa5\xe7\x9c\x8b\xe5\x85\xa8\xe9\x83\xa8 \xe2\x86\x92", header);
    viewAll->setFixedHeight(24);
    viewAll->setFlat(true);
    viewAll->setCursor(Qt::PointingHandCursor);
    viewAll->setStyleSheet(
        "QPushButton { font-size: 10px; color: rgba(255,255,255,0.12); background: transparent; border: none; padding: 4px 8px; }"
        "QPushButton:hover { color: #00d4ff; }"
        "QPushButton:pressed { color: #0099cc; }");

    hdrLayout->addWidget(title);
    hdrLayout->addStretch();
    hdrLayout->addWidget(viewAll);
    header->setFixedHeight(30);

    connect(viewAll, &QPushButton::clicked, this, &HistoryBar::viewAllClicked);
    mainLayout->addWidget(header);

    m_itemsLayout = new QHBoxLayout;
    m_itemsLayout->setContentsMargins(24, 2, 24, 10);
    m_itemsLayout->setSpacing(10);

    mainLayout->addLayout(m_itemsLayout, 1);

    setStyleSheet("background: transparent;");

    connect(m_manager, &SessionManager::historyChanged, this, [this]() { refresh(); });
    refresh();
}

void HistoryBar::refresh() {
    QLayoutItem *child;
    while ((child = m_itemsLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    const auto &history = m_manager->history();
    int count = qMin(4, (int)history.size());
    for (int i = 0; i < count; i++) {
        m_itemsLayout->addWidget(createHistoryItemWidget(history[i]), 1);
    }

    if (count == 0) {
        auto *empty = new QLabel("\xe6\x9a\x82\xe6\x97\xa0\xe6\x92\xad\xe6\x94\xbe\xe8\xae\xb0\xe5\xbd\x95", this);
        empty->setStyleSheet("font-size: 10px; color: rgba(255,255,255,0.08);");
        m_itemsLayout->addWidget(empty, 1);
    }
}

QWidget* HistoryBar::createHistoryItemWidget(const HistoryItem &item) {
    auto *btn = new QPushButton(this);
    btn->setFlat(true);
    btn->setMinimumWidth(150);
    btn->setMinimumHeight(68);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        "QPushButton { background: #18191d; border: none; "
        "border-radius: 10px; }"
        "QPushButton:hover { background: #1e1f24; }");
    btn->setText(QString());

    auto *layout = new QHBoxLayout(btn);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(14);
    layout->setAlignment(Qt::AlignVCenter);

    // 场景图标
    QString sceneIcon, iconBg, iconBorder;
    switch (item.scene) {
        case SceneType::Concert:
            sceneIcon = QStringLiteral("\xf0\x9f\x8e\xa4");
            iconBg = "rgba(255,0,100,0.06)";
            iconBorder = "rgba(255,0,100,0.06)";
            break;
        case SceneType::TechPlaza:
            sceneIcon = QStringLiteral("\xf0\x9f\x8f\x99\xef\xb8\x8f");
            iconBg = "rgba(0,212,255,0.06)";
            iconBorder = "rgba(0,212,255,0.06)";
            break;
        case SceneType::Cinema:
            sceneIcon = QStringLiteral("\xf0\x9f\x8e\xac");
            iconBg = "rgba(255,180,50,0.06)";
            iconBorder = "rgba(255,180,50,0.06)";
            break;
        case SceneType::Normal:
            sceneIcon = QStringLiteral("\xe2\x96\xb6\xef\xb8\x8f");
            iconBg = "rgba(102,102,255,0.06)";
            iconBorder = "rgba(102,102,255,0.06)";
            break;
    }
    auto *icon = new QLabel(sceneIcon, btn);
    icon->setFixedSize(44, 44);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QString("font-size: 20px; background: %1; "
                                "border: 1px solid %2; border-radius: 10px;")
                        .arg(iconBg, iconBorder));
    icon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // 计算相对时间
    qint64 diff = QDateTime::currentSecsSinceEpoch() - item.timestamp;
    QString timeAgo;
    if (diff < 60) timeAgo = "\xe5\x88\x9a\xe5\x88\x9a";               // "刚刚"
    else if (diff < 3600) timeAgo = QString("%1 \xe5\x88\x86\xe9\x92\x9f\xe5\x89\x8d").arg(diff / 60);   // "X分钟前"
    else if (diff < 86400) timeAgo = QString("%1 \xe5\xb0\x8f\xe6\x97\xb6\xe5\x89\x8d").arg(diff / 3600); // "X小时前"
    else timeAgo = QString("%1 \xe5\xa4\xa9\xe5\x89\x8d").arg(diff / 86400);                            // "X天前"

    // 中间文本列布局
    auto *textCol = new QVBoxLayout;
    textCol->setSpacing(2);
    textCol->setAlignment(Qt::AlignVCenter);

    // 文件名标签
    auto *nameLabel = new QLabel(item.file.fileName, btn);
    nameLabel->setStyleSheet("font-size: 12px; font-weight: 500; color: #d4dcec; border: none;");
    nameLabel->setFrameStyle(QFrame::NoFrame);
    nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // 元信息标签（场景名 · 相对时间）
    auto *metaLabel = new QLabel(
        QString("%1 \xc2\xb7 %2").arg(SceneTypeName(item.scene)).arg(timeAgo), btn);
    metaLabel->setStyleSheet("font-size: 10px; color: rgba(255,255,255,0.2); border: none;");
    metaLabel->setFrameStyle(QFrame::NoFrame);
    metaLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // 将标签加入到垂直布局中
    textCol->addWidget(nameLabel);
    textCol->addWidget(metaLabel);

    // 右侧圆形播放按钮
    auto *arrow = new QLabel(QStringLiteral("\xe2\x96\xb6"), btn);
    arrow->setFixedSize(32, 32);
    arrow->setAlignment(Qt::AlignCenter);
    arrow->setStyleSheet(
        "font-size: 16px; font-weight: bold; color: white; "
        "background: rgba(0,212,255,0.2); border-radius: 16px; "
        "border: 1px solid rgba(0,212,255,0.3);"
    );
    arrow->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // 组装水平布局
    layout->addWidget(icon, 0);
    layout->addLayout(textCol, 1);
    layout->addWidget(arrow, 0);

    connect(btn, &QPushButton::clicked, this, [this, item]() {
        emit historyItemClicked(item);
    });

    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(btn, &QPushButton::customContextMenuRequested, this, [this, item](const QPoint &pos) {
        QMenu menu;
        menu.setStyleSheet(
            "QMenu { background: #1a1b1f; border: 1px solid rgba(255,255,255,0.06); "
            "border-radius: 8px; padding: 4px; }"
            "QMenu::item { color: rgba(255,255,255,0.5); font-size: 11px; "
            "padding: 6px 20px; border-radius: 4px; }"
            "QMenu::item:selected { background: rgba(255,68,68,0.12); color: #ff4444; }"
            "QMenu::item:pressed { background: rgba(255,68,68,0.25); color: white; }");
        auto *delAction = menu.addAction(QStringLiteral("\xe5\x88\xa0\xe9\x99\xa4\xe8\xae\xb0\xe5\xbd\x95"));
        if (menu.exec(static_cast<QWidget*>(sender())->mapToGlobal(pos)) == delAction) {
            const auto &hist = m_manager->history();
            for (int i = 0; i < hist.size(); i++) {
                if (hist[i].file.filePath == item.file.filePath &&
                    hist[i].scene == item.scene &&
                    hist[i].timestamp == item.timestamp) {
                    m_manager->removeHistory(i);
                    break;
                }
            }
        }
    });

    return btn;
}

