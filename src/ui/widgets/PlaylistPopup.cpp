#include "PlaylistPopup.h"
#include "ui/SessionManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QKeyEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QScrollBar>

PlaylistPopup::PlaylistPopup(SessionManager *manager, QWidget *parent)
    : QFrame(parent)
    , m_manager(manager)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Popup | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setFocusPolicy(Qt::StrongFocus);

    setupUI();

    // Re-populate when files change
    connect(m_manager, &SessionManager::filesChanged, this, [this]() {
        populateList(m_searchEdit->text());
    });
    connect(m_manager, &SessionManager::selectionChanged, this, [this](int index) {
        m_currentIndex = index;
        populateList(m_searchEdit->text());
    });
}

void PlaylistPopup::setupUI() {
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *panel = new QWidget(this);
    panel->setObjectName("popupPanel");
    panel->setStyleSheet(
        "#popupPanel {"
        "  background: rgba(10, 10, 22, 0.96);"
        "  border: 1px solid rgba(255, 255, 255, 0.08);"
        "  border-radius: 12px;"
        "}");
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Header ──
    auto *header = new QWidget(panel);
    header->setStyleSheet("background: transparent;");
    auto *hdrLayout = new QHBoxLayout(header);
    hdrLayout->setContentsMargins(16, 12, 16, 8);

    auto *iconLabel = new QLabel(QStringLiteral("\xf0\x9f\x93\x8b"), header);
    iconLabel->setStyleSheet("font-size: 13px; border: none;");

    auto *titleLabel = new QLabel(QStringLiteral("\xe6\x92\xad\xe6\x94\xbe\xe5\x88\x97\xe8\xa1\xa8"), header);
    titleLabel->setStyleSheet("font-size: 13px; color: rgba(255,255,255,0.7); font-weight: 600; border: none;");

    m_countLabel = new QLabel("", header);
    m_countLabel->setStyleSheet("font-size: 11px; color: rgba(255,255,255,0.2); border: none;");

    hdrLayout->addWidget(iconLabel);
    hdrLayout->addSpacing(6);
    hdrLayout->addWidget(titleLabel);
    hdrLayout->addSpacing(6);
    hdrLayout->addWidget(m_countLabel);
    hdrLayout->addStretch();

    layout->addWidget(header);

    // ── Search bar ──
    m_searchEdit = new QLineEdit(panel);
    m_searchEdit->setPlaceholderText(QStringLiteral("\xe6\x90\x9c\xe7\xb4\xa2\xe8\xa7\x86\xe9\xa2\x91..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(
        "QLineEdit {"
        "  background: rgba(255,255,255,0.04);"
        "  border: 1px solid rgba(255,255,255,0.06);"
        "  border-radius: 8px;"
        "  padding: 6px 12px;"
        "  margin: 4px 12px 8px 12px;"
        "  font-size: 12px;"
        "  color: rgba(255,255,255,0.5);"
        "}"
        "QLineEdit:focus {"
        "  border-color: rgba(0,212,255,0.3);"
        "  color: rgba(255,255,255,0.8);"
        "}"
        "QLineEdit::placeholder { color: rgba(255,255,255,0.15); }");

    layout->addWidget(m_searchEdit);

    // ── List ──
    m_listWidget = new QListWidget(panel);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listWidget->verticalScrollBar()->setStyleSheet(
        "QScrollBar:vertical { width: 6px; background: transparent; margin: 0; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.07); border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(255,255,255,0.14); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }");
    m_listWidget->setStyleSheet(
        "QListWidget {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 0px 8px;"
        "  font-size: 12px;"
        "  color: rgba(255,255,255,0.5);"
        "  outline: none;"
        "}"
        "QListWidget::item {"
        "  padding: 7px 12px;"
        "  border-radius: 6px;"
        "  margin: 1px 4px;"
        "}"
        "QListWidget::item:hover {"
        "  background: rgba(255,255,255,0.04);"
        "  color: rgba(255,255,255,0.7);"
        "}"
        "QListWidget::item:selected {"
        "  background: rgba(0,212,255,0.08);"
        "  color: rgba(255,255,255,0.9);"
        "}");

    m_listWidget->setSpacing(1);
    layout->addWidget(m_listWidget, 1);

    outerLayout->addWidget(panel);

    // ── Search filtering ──
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        populateList(text);
    });

    // ── Item click ──
    connect(m_listWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        int index = item->data(Qt::UserRole).toInt();
        if (index >= 0 && index < m_manager->files().size()) {
            const auto &files = m_manager->files();
            emit fileSelected(files[index].filePath, index);
        }
        close();
    });

    // Initial population
    populateList();
}

void PlaylistPopup::populateList(const QString &filter) {
    m_listWidget->clear();
    const auto &files = m_manager->files();
    int count = 0;

    for (int i = 0; i < files.size(); ++i) {
        if (!filter.isEmpty()) {
            if (!files[i].fileName.contains(filter, Qt::CaseInsensitive))
                continue;
        }

        auto *item = new QListWidgetItem();
        item->setData(Qt::UserRole, i);

        // Display text: file name, with "⏵" indicator for current
        QString displayText;
        if (i == m_currentIndex) {
            displayText = QStringLiteral("\xe2\x96\xb6 ") + files[i].fileName;
            item->setForeground(QColor(0, 212, 255));
        } else {
            displayText = QStringLiteral("  ") + files[i].fileName;
            item->setForeground(QColor(180, 180, 190));
        }
        item->setText(displayText);
        m_listWidget->addItem(item);
        ++count;
    }

    m_countLabel->setText(QString::number(count) + " / " + QString::number(files.size()));

    // Auto-select current item and scroll to it
    if (m_currentIndex >= 0 && filter.isEmpty()) {
        auto items = m_listWidget->findItems(QStringLiteral("\xe2\x96\xb6"), Qt::MatchStartsWith);
        if (!items.isEmpty()) {
            m_listWidget->setCurrentItem(items.first());
            m_listWidget->scrollToItem(items.first(), QAbstractItemView::PositionAtCenter);
        }
    }
}

void PlaylistPopup::showAt(const QPoint &globalPos, int preferredWidth) {
    setFixedWidth(preferredWidth);
    setMaximumHeight(400);

    // Populate and resize height to fit content
    populateList(m_searchEdit->text());
    int itemCount = m_listWidget->count();
    int listHeight = qMin(itemCount * 36 + 20, 320);
    listHeight = qMax(listHeight, 60);
    m_listWidget->setMinimumHeight(listHeight);

    adjustSize();

    // Position: show above the anchor point if possible
    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen) screen = QGuiApplication::primaryScreen();
    QRect screenGeom = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    int popupHeight = sizeHint().height();
    int x = globalPos.x() - width() / 2;
    int y = globalPos.y() - popupHeight - 12;

    // If above doesn't fit, show below
    if (y < screenGeom.top()) {
        y = globalPos.y() + 12;
    }
    // Keep within horizontal bounds
    if (x + width() > screenGeom.right())
        x = screenGeom.right() - width();
    if (x < screenGeom.left())
        x = screenGeom.left();

    move(x, y);
    show();
    raise();
    setFocus();

    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

void PlaylistPopup::setCurrentHighlight(int index) {
    m_currentIndex = index;
    populateList(m_searchEdit->text());
}

void PlaylistPopup::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        close();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        auto *current = m_listWidget->currentItem();
        if (current) {
            int index = current->data(Qt::UserRole).toInt();
            if (index >= 0 && index < m_manager->files().size()) {
                const auto &files = m_manager->files();
                emit fileSelected(files[index].filePath, index);
            }
            close();
        }
    } else if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
        // Forward navigation keys to list widget
        QApplication::sendEvent(m_listWidget, event);
    } else {
        QFrame::keyPressEvent(event);
    }
}
