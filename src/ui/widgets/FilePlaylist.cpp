#include "FilePlaylist.h"
#include "ui/SessionManager.h"
#include "common/Types.h"
#include "common/Constants.h"
#include <QScrollArea>
#include <QHBoxLayout>
#include <QMenu>

FilePlaylist::FilePlaylist(SessionManager *manager, QWidget *parent)
    : QWidget(parent), m_manager(manager)
{
    setMinimumWidth(120);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header
    auto *header = new QWidget(this);
    header->setFixedHeight(44);
    auto *hdrLayout = new QHBoxLayout(header);
    hdrLayout->setContentsMargins(16, 0, 16, 0);

    auto *title = new QLabel(QStringLiteral("\u64ad\u653e\u5217\u8868"), header);
    title->setStyleSheet("font-size: 11px; color: rgba(255,255,255,0.25); letter-spacing: 2px; font-weight: 500;");

    auto *addBtn = new QPushButton("+ \u6dfb\u52a0\u6587\u4ef6", header);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(
        "QPushButton { font-size: 10px; color: #00d4ff; background: rgba(0,212,255,0.06);"
        "border: 1px solid rgba(0,212,255,0.08); border-radius: 5px; padding: 3px 12px; }"
        "QPushButton:hover { background: rgba(0,212,255,0.10); }");
    connect(addBtn, &QPushButton::clicked, this, &FilePlaylist::addFilesRequested);

    auto *netBtn = new QPushButton(QStringLiteral("\U0001F310 \u7f51\u7edc"), header);
    netBtn->setCursor(Qt::PointingHandCursor);
    netBtn->setStyleSheet(
        "QPushButton { font-size: 10px; color: #00d4ff; background: rgba(0,212,255,0.06);"
        "border: 1px solid rgba(0,212,255,0.08); border-radius: 5px; padding: 3px 12px; }"
        "QPushButton:hover { background: rgba(0,212,255,0.10); }");
    connect(netBtn, &QPushButton::clicked, this, &FilePlaylist::networkRequested);

    hdrLayout->addWidget(title);
    hdrLayout->addStretch();
    hdrLayout->addWidget(netBtn);
    hdrLayout->addSpacing(6);
    hdrLayout->addWidget(addBtn);
    mainLayout->addWidget(header);

    // Separator
    auto *sep = new QWidget(this);
    sep->setFixedHeight(1);
    sep->setStyleSheet("background: rgba(255,255,255,0.04);");
    mainLayout->addWidget(sep);

    // Scrollable list
    m_listContainer = new QWidget(this);
    m_listLayout = new QVBoxLayout(m_listContainer);
    m_listLayout->setContentsMargins(0, 4, 0, 4);
    m_listLayout->setSpacing(2);

    auto *fileScroll = new QScrollArea(this);
    fileScroll->setWidgetResizable(true);
    fileScroll->setFrameShape(QFrame::NoFrame);
    fileScroll->setStyleSheet(
        "QScrollArea { background: transparent; }"
        "QScrollBar:vertical { width: 6px; background: transparent; margin: 0; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.07); border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(255,255,255,0.14); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }");
    fileScroll->setWidget(m_listContainer);
    mainLayout->addWidget(fileScroll, 1);

    // Count label
    m_countLabel = new QLabel(this);
    m_countLabel->setContentsMargins(16, 6, 16, 8);
    m_countLabel->setStyleSheet("font-size: 10px; color: rgba(255,255,255,0.12);");
    mainLayout->addWidget(m_countLabel);

    // Connections
    connect(m_manager, &SessionManager::filesChanged, this, &FilePlaylist::refresh);
    connect(m_manager, &SessionManager::selectionChanged, this, [this](int) { refresh(); });

    refresh();
}

void FilePlaylist::refresh() {
    // Clear existing items
    QLayoutItem *child;
    while ((child = m_listLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    const auto &files = m_manager->files();
    int sel = m_manager->selectedIndex();

    for (int i = 0; i < files.size(); i++) {
        auto *w = createFileItemWidget(files[i], i, i == sel);
        m_listLayout->addWidget(w);
    }

    m_listLayout->addStretch();
    m_countLabel->setText(QString(QStringLiteral("\u5171 %1 \u4e2a\u6587\u4ef6")).arg(files.size()));
    m_countLabel->setVisible(!files.isEmpty());
}

QWidget* FilePlaylist::createFileItemWidget(const FileItem &item, int index, bool selected) {
    auto *btn = new QPushButton(m_listContainer);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(48);
    QString bg = selected ? "rgba(0,212,255,0.15)" : "transparent";
    QString hoverBg = selected ? "rgba(0,212,255,0.22)" : "rgba(0,212,255,0.06)";
    btn->setStyleSheet(QString(
        "QPushButton { text-align: left; padding: 0; background: %1; border: none;"
        "border-left: 2px solid %2; border-radius: 0 8px 8px 0; margin: 0 8px; }"
        "QPushButton:hover { background: %3; }"
    ).arg(bg)
     .arg(selected ? "#00d4ff" : "transparent")
     .arg(hoverBg));

    auto *layout = new QHBoxLayout(btn);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(10);

    QString iconEmoji = SessionManager::isNetworkUrl(item.filePath)
        ? QStringLiteral("\U0001F310")
        : QStringLiteral("\U0001F3AC");
    auto *icon = new QLabel(iconEmoji, btn);
    icon->setFixedSize(24, 24);
    icon->setAlignment(Qt::AlignCenter);
    icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    icon->setStyleSheet(QString("font-size: 14px; background: %1; border-radius: 6px;")
        .arg(selected ? "rgba(0,212,255,0.10)" : "rgba(255,255,255,0.03)"));

    auto *nameLabel = new QLabel(item.fileName, btn);
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    nameLabel->setStyleSheet(QString("font-size: 12px; font-weight: %1; color: %2;")
        .arg(selected ? "500" : "400")
        .arg(selected ? "#d4dcec" : "rgba(255,255,255,0.55)"));

    layout->addWidget(icon);
    layout->addWidget(nameLabel, 1);

    if (selected) {
        auto *tag = new QLabel(QStringLiteral("\u5df2\u9009"), btn);
        tag->setStyleSheet("font-size: 9px; color: #00d4ff; background: rgba(0,212,255,0.08);"
                          "padding: 1px 7px; border-radius: 3px;");
        layout->addWidget(tag);
    }

    connect(btn, &QPushButton::clicked, this, [this, index]() {
        m_manager->selectFile(index);
    });

    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(btn, &QPushButton::customContextMenuRequested, this, [this, index, btn](const QPoint &pos) {
        const auto &files = m_manager->files();
        if (index < 0 || index >= files.size()) return;

        bool isNet = SessionManager::isNetworkUrl(files[index].filePath);

        QMenu menu(btn);
        menu.setStyleSheet(
            "QMenu { background: #18191d; border: 1px solid rgba(255,255,255,0.06); border-radius: 8px; padding: 4px; }"
            "QMenu::item { font-size: 11px; color: rgba(255,255,255,0.55); padding: 6px 16px; border-radius: 4px; }"
            "QMenu::item:selected { color: #00d4ff; background: rgba(0,212,255,0.08); }"
            "QMenu::item:disabled { color: rgba(255,255,255,0.15); }");

        bool alreadyInLib = false;
        for (const auto &lib : m_manager->libraryFiles()) {
            if (lib.originalPath == files[index].filePath || lib.libraryPath == files[index].filePath) {
                alreadyInLib = true;
                break;
            }
        }

        if (alreadyInLib) {
            auto *action = menu.addAction(QStringLiteral("\u5df2\u5728\u5a92\u4f53\u5e93\u4e2d"));
            action->setEnabled(false);
        } else if (isNet) {
            auto *action = menu.addAction(QStringLiteral("\u6dfb\u52a0\u5230\u5a92\u4f53\u5e93"));
            connect(action, &QAction::triggered, this, [this, index]() {
                const auto &files = m_manager->files();
                if (index >= 0 && index < files.size())
                    m_manager->addNetworkToLibrary(files[index].filePath);
            });
        } else {
            auto *action = menu.addAction(QStringLiteral("\u6dfb\u52a0\u5230\u5a92\u4f53\u5e93"));
            connect(action, &QAction::triggered, this, [this, index]() {
                const auto &files = m_manager->files();
                if (index >= 0 && index < files.size())
                    m_manager->addToLibrary(files[index].filePath);
            });
        }
        menu.addSeparator();

        auto *removeAction = menu.addAction(QStringLiteral("\u79fb\u9664\u64ad\u653e\u5217\u8868"));
        connect(removeAction, &QAction::triggered, this, [this, index]() {
            m_manager->removeFile(index);
        });

        menu.exec(btn->mapToGlobal(pos));
    });

    return btn;
}
