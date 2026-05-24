#include "MediaLibraryDialog.h"
#include "ui/SessionManager.h"
#include "common/Constants.h"
#include <QDateTime>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QMouseEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QResizeEvent>

// ===== QLabel with automatic text elision =====
class ElidedLabel : public QLabel {
public:
    explicit ElidedLabel(const QString &text, QWidget *parent = nullptr)
        : QLabel(parent), m_fullText(text)
    {
        setText(text);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }

    const QString& fullText() const { return m_fullText; }
    void setFullText(const QString &text) {
        m_fullText = text;
        updateElision();
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QLabel::resizeEvent(event);
        updateElision();
    }

private:
    void updateElision() {
        QFontMetrics metrics(font());
        setText(metrics.elidedText(m_fullText, Qt::ElideRight, width()));
    }
    QString m_fullText;
};

MediaLibraryDialog::MediaLibraryDialog(SessionManager *manager, QWidget *parent)
    : QDialog(parent), m_manager(manager)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setFixedSize(520, 480);
    setStyleSheet(QString("background-color: %1;").arg(Constants::ColorBg.name()));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ===== Header =====
    auto *header = new QWidget(this);
    header->setFixedHeight(52);
    header->setStyleSheet("border-bottom: 1px solid rgba(255,255,255,0.04);");
    auto *hdrLayout = new QHBoxLayout(header);
    hdrLayout->setContentsMargins(24, 0, 24, 0);

    auto *title = new QLabel(QStringLiteral("\xF0\x9F\x93\x81 \xE5\xAA\x92\xE4\xBD\x93\xE5\xBA\x93"), header);
    title->setStyleSheet("font-size: 13px; font-weight: 500; color: rgba(255,255,255,0.7); letter-spacing: 2px; border: none;");

    auto *addAllBtn = new QPushButton(QStringLiteral("\xE5\x85\xA8\xE9\x83\xA8\xE6\xB7\xBB\xE5\x8A\xA0"), header);
    addAllBtn->setFixedSize(72, 24);
    addAllBtn->setFlat(true);
    addAllBtn->setCursor(Qt::PointingHandCursor);
    addAllBtn->setStyleSheet(
        "QPushButton { font-size: 10px; color: #00d4ff; background: rgba(0,212,255,0.06); "
        "border: 1px solid rgba(0,212,255,0.1); border-radius: 6px; }"
        "QPushButton:hover { color: white; background: rgba(0,212,255,0.15); border-color: rgba(0,212,255,0.25); }"
        "QPushButton:pressed { background: rgba(0,212,255,0.25); }");

    auto *closeBtn = new QPushButton(QStringLiteral("\xE2\x9C\x95"), header);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setFlat(true);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        "QPushButton { font-size: 15px; color: rgba(255,255,255,0.15); background: transparent; border: none; border-radius: 8px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.5); background: rgba(255,255,255,0.06); }");

    hdrLayout->addWidget(title);
    hdrLayout->addStretch();
    hdrLayout->addWidget(addAllBtn);
    hdrLayout->addSpacing(8);
    hdrLayout->addWidget(closeBtn);

    header->installEventFilter(this);
    mainLayout->addWidget(header);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    connect(addAllBtn, &QPushButton::clicked, this, [this]() {
        const auto &lib = m_manager->libraryFiles();
        for (const auto &item : lib) {
            QString path = item.libraryPath.isEmpty() ? item.originalPath : item.libraryPath;
            m_manager->addFile(path, item.fileName);
        }
        if (!lib.isEmpty())
            close();
    });

    // ===== Search bar =====
    auto *searchWidget = new QWidget(this);
    searchWidget->setFixedHeight(48);
    auto *searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(24, 8, 24, 8);

    m_searchInput = new QLineEdit(searchWidget);
    m_searchInput->setPlaceholderText(QStringLiteral("\xE6\x90\x9C\xE7\xB4\xA2\xE5\xAA\x92\xE4\xBD\x93..."));
    m_searchInput->setClearButtonEnabled(true);
    m_searchInput->setStyleSheet(
        "QLineEdit { font-size: 12px; color: rgba(255,255,255,0.6); background: rgba(255,255,255,0.04); "
        "border: 1px solid rgba(255,255,255,0.06); border-radius: 8px; padding: 6px 12px; }"
        "QLineEdit:focus { color: #d4dcec; border-color: rgba(0,212,255,0.2); background: rgba(0,212,255,0.04); }"
        "QLineEdit:hover { border-color: rgba(255,255,255,0.10); }"
        "QLineEdit::placeholder { color: rgba(255,255,255,0.15); }");

    searchLayout->addWidget(m_searchInput);
    mainLayout->addWidget(searchWidget);

    connect(m_searchInput, &QLineEdit::textChanged, this, [this]() { refresh(); });

    // ===== Scrollable list =====
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { width: 4px; background: transparent; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.06); border-radius: 2px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(255,255,255,0.1); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }");

    auto *listContainer = new QWidget(scrollArea);
    listContainer->setStyleSheet("background: transparent;");
    m_listLayout = new QVBoxLayout(listContainer);
    m_listLayout->setContentsMargins(24, 12, 20, 20);
    m_listLayout->setSpacing(6);
    m_listLayout->addStretch();

    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidget(listContainer);
    mainLayout->addWidget(scrollArea, 1);

    // ===== Connections =====
    connect(m_manager, &SessionManager::libraryChanged, this, [this]() { refresh(); });

    refresh();
}

void MediaLibraryDialog::refresh() {
    QLayoutItem *child;
    while ((child = m_listLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    const auto &library = m_manager->libraryFiles();
    QString filter = m_searchInput->text().trimmed();

    // Collect matching items
    QVector<int> matched;
    for (int i = 0; i < library.size(); i++) {
        if (filter.isEmpty() || library[i].fileName.contains(filter, Qt::CaseInsensitive))
            matched.append(i);
    }

    if (library.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("\xE5\xAA\x92\xE4\xBD\x93\xE5\xBA\x93\xE4\xB8\xBA\xE7\xA9\xBA"), this);
        empty->setStyleSheet("font-size: 11px; color: rgba(255,255,255,0.1); border: none;");
        empty->setAlignment(Qt::AlignCenter);
        m_listLayout->addWidget(empty);
        m_listLayout->addStretch();
        return;
    }

    if (matched.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("\xE6\x97\xA0\xE5\x8C\xB9\xE9\x85\x8D\xE7\xBB\x93\xE6\x9E\x9C"), this);
        empty->setStyleSheet("font-size: 11px; color: rgba(255,255,255,0.1); border: none;");
        empty->setAlignment(Qt::AlignCenter);
        m_listLayout->addWidget(empty);
        m_listLayout->addStretch();
        return;
    }

    for (int i : matched)
        m_listLayout->addWidget(createLibraryItemWidget(library[i], i));

    m_listLayout->addStretch();
}

static QString formatFileSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    double kb = bytes / 1024.0;
    if (kb < 1024) return QString("%1 KB").arg(kb, 0, 'f', 0);
    double mb = kb / 1024.0;
    if (mb < 1024) return QString("%1 MB").arg(mb, 0, 'f', 1);
    double gb = mb / 1024.0;
    return QString("%1 GB").arg(gb, 0, 'f', 2);
}

QWidget* MediaLibraryDialog::createLibraryItemWidget(const MediaLibraryItem &item, int index) {
    auto *card = new QWidget(this);
    card->setFixedHeight(64);
    card->setStyleSheet(
        "QWidget#libraryCard { background: #18191d; border: 1px solid transparent; border-radius: 12px; }"
        "QWidget#libraryCard:hover { background: #1e1f24; border-color: rgba(0,212,255,0.08); }");
    card->setObjectName("libraryCard");

    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(14);
    layout->setAlignment(Qt::AlignVCenter);

    // Icon (globe for network refs, film for local files)
    bool isNetRef = item.libraryPath.isEmpty();
    auto *icon = new QLabel(isNetRef ? QStringLiteral("\xF0\x9F\x8C\x90") : QStringLiteral("\xF0\x9F\x8E\xAC"), card);
    icon->setFixedSize(44, 44);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet("font-size: 20px; background: rgba(0,212,255,0.06); border: 1px solid rgba(0,212,255,0.1); border-radius: 8px;");

    // Info column
    auto *textCol = new QVBoxLayout;
    textCol->setSpacing(2);
    textCol->setAlignment(Qt::AlignVCenter);

    auto *nameLabel = new ElidedLabel(item.fileName, card);
    nameLabel->setStyleSheet("font-size: 13px; font-weight: 500; color: #d4dcec; border: none; background: transparent;");

    QString dateStr = QDateTime::fromSecsSinceEpoch(item.dateAdded).toString("yyyy/MM/dd");
    QString sizeStr = isNetRef ? QStringLiteral("\u7f51\u7edc") : formatFileSize(item.fileSize);
    auto *metaLabel = new QLabel(
        QStringLiteral("%1 \xC2\xB7 %2").arg(sizeStr, dateStr), card);
    metaLabel->setStyleSheet("font-size: 11px; color: rgba(255,255,255,0.2); border: none; background: transparent;");
    metaLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    textCol->addWidget(nameLabel);
    textCol->addWidget(metaLabel);

    // Add to playlist button
    auto *addBtn = new QPushButton(QStringLiteral("\xE6\xB7\xBB\xE5\x8A\xA0\xE5\x88\xB0\xE6\x92\xAD\xE6\x94\xBE\xE5\x88\x97\xE8\xA1\xA8"), card);
    addBtn->setFixedHeight(28);
    addBtn->setMaximumWidth(100);
    addBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(
        "QPushButton { font-size: 10px; color: #00d4ff; background: rgba(0,212,255,0.06); "
        "border: 1px solid rgba(0,212,255,0.1); border-radius: 6px; padding: 0 12px; }"
        "QPushButton:hover { color: white; background: rgba(0,212,255,0.14); border-color: rgba(0,212,255,0.25); }"
        "QPushButton:pressed { background: rgba(0,212,255,0.25); }");

    // Delete button
    auto *delBtn = new QPushButton(QStringLiteral("\xC3\x97"), card);
    delBtn->setFixedSize(26, 26);
    delBtn->setFlat(true);
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setStyleSheet(
        "QPushButton { font-size: 14px; color: rgba(255,255,255,0.08); background: transparent; "
        "border: none; border-radius: 13px; }"
        "QPushButton:hover { color: #ff4444; background: rgba(255,68,68,0.12); }");

    layout->addWidget(icon, 0);
    layout->addLayout(textCol, 1);
    layout->addWidget(addBtn, 0);
    layout->addSpacing(4);
    layout->addWidget(delBtn, 0);

    connect(addBtn, &QPushButton::clicked, this, [this, item]() {
        QString path = item.libraryPath.isEmpty() ? item.originalPath : item.libraryPath;
        m_manager->addFile(path, item.fileName);
    });

    connect(delBtn, &QPushButton::clicked, this, [this, index]() {
        m_manager->removeFromLibrary(index);
    });

    return card;
}

bool MediaLibraryDialog::eventFilter(QObject *obj, QEvent *event) {
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
