#include "HomePage.h"
#include "ui/SessionManager.h"
#include "ui/widgets/FilePlaylist.h"
#include "ui/widgets/UrlInputDialog.h"
#include "ui/widgets/HistoryBar.h"
#include "ui/widgets/HistoryDialog.h"
#include "common/Constants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QFileDialog>
#include <QPushButton>

HomePage::HomePage(SessionManager *manager, QWidget *parent)
    : QWidget(parent), m_manager(manager)
{
    setStyleSheet(QString("background-color: %1;").arg(Constants::ColorBg.name()));
    setupUI();

    connect(m_manager, &SessionManager::selectionChanged, this, [this](int) {
        updateCardStates();
        const FileItem *sel = m_manager->selectedFile();
        if (sel) {
            m_selectedName->setText(sel->fileName);
            m_selectedName->setStyleSheet("font-size: 10px; color: #00d4ff; font-weight: 500;");
        } else {
            m_selectedName->setText("未选择");
            m_selectedName->setStyleSheet("font-size: 10px; color: rgba(255,255,255,0.12);");
        }
    });

    connect(m_playlist, &FilePlaylist::addFilesRequested, this, [this]() {
        QStringList paths = QFileDialog::getOpenFileNames(this, "选择视频文件", "",
            "视频文件 (*.mp4 *.mkv *.avi *.mov *.flv *.ts);;所有文件 (*)");
        if (!paths.isEmpty()) {
            m_manager->addFiles(paths);
        }
    });

    connect(m_playlist, &FilePlaylist::networkRequested, this, [this]() {
        UrlInputDialog dlg(m_manager->recentUrls(), this);
        if (dlg.exec() == QDialog::Accepted) {
            QString url = dlg.url();
            if (!url.isEmpty()) {
                m_manager->addNetworkFile(url);
            }
        }
    });

    connect(m_historyBar, &HistoryBar::historyItemClicked, this, [this](const HistoryItem &item) {
        if (item.scene == SceneType::Normal) {
            emit openNormalPlayer(item.file.filePath);
        } else {
            emit openScenePlayer(item.file.filePath, item.scene);
        }
    });

    connect(m_historyBar, &HistoryBar::viewAllClicked, this, [this]() {
        if (!m_historyDialog) {
            m_historyDialog = new HistoryDialog(m_manager, this);
            connect(m_historyDialog, &HistoryDialog::historyItemClicked, this, [this](const HistoryItem &item) {
                if (item.scene == SceneType::Normal) {
                    emit openNormalPlayer(item.file.filePath);
                } else {
                    emit openScenePlayer(item.file.filePath, item.scene);
                }
            });
        }
        m_historyDialog->show();
        m_historyDialog->raise();
        m_historyDialog->activateWindow();
    });
}

void HomePage::setupUI() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ===== Top Nav Bar (removed — TitleBar in MainWindow handles this) =====

    // ===== Split Content: Playlist | Scene Cards =====
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(1);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setStyleSheet(
        "QSplitter::handle { background: rgba(255,255,255,0.04); }"
        "QSplitter::handle:hover { background: rgba(0,212,255,0.15); }");
    m_splitter->installEventFilter(this);

    // Left: Playlist
    m_playlist = new FilePlaylist(m_manager, m_splitter);

    // Right: Scene area
    auto *rightWidget = new QWidget(m_splitter);
    rightWidget->setStyleSheet("background: transparent;");
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    // Selection hint
    auto *hintWidget = new QWidget(rightWidget);
    hintWidget->setStyleSheet("background: transparent;");
    hintWidget->setFixedHeight(48);
    auto *hintLayout = new QHBoxLayout(hintWidget);
    hintLayout->setContentsMargins(24, 0, 24, 0);

    auto *selectionHint = new QLabel("← 选择视频，然后选择播放模式", hintWidget);
    selectionHint->setStyleSheet("font-size: 11px; color: rgba(255,255,255,0.18); letter-spacing: 3px;");

    m_selectedName = new QLabel("未选择", hintWidget);
    m_selectedName->setStyleSheet("font-size: 10px; color: rgba(255,255,255,0.12);");

    hintLayout->addWidget(selectionHint);
    hintLayout->addStretch();
    hintLayout->addWidget(m_selectedName);

    rightLayout->addWidget(hintWidget);

    // Scene cards
    auto *cardContainer = new QWidget(rightWidget);
    cardContainer->setStyleSheet("background: transparent;");
    m_cardGrid = new QGridLayout(cardContainer);
    m_cardGrid->setContentsMargins(24, 8, 24, 24);
    m_cardGrid->setSpacing(14);
    m_cardGrid->setAlignment(Qt::AlignCenter);

    auto makeCard = [this](SceneType type, int row, int col) {
        auto *card = new SceneCard(type, this);
        connect(card, &SceneCard::clicked, this, [this](SceneType t) {
            const FileItem *sel = m_manager->selectedFile();
            if (!sel) return;
            if (t == SceneType::Normal) {
                emit openNormalPlayer(sel->filePath);
            } else {
                emit openScenePlayer(sel->filePath, t);
            }
        });
        m_cardGrid->addWidget(card, row, col, Qt::AlignCenter);
        m_cards.push_back(card);
    };
    makeCard(SceneType::Concert,   0, 0);
    makeCard(SceneType::TechPlaza, 0, 1);
    makeCard(SceneType::Cinema,    1, 0);
    makeCard(SceneType::Normal,    1, 1);

    rightLayout->addWidget(cardContainer, 1);

    // ===== History Bar =====
    m_historyBar = new HistoryBar(m_manager, this);

    // ===== Scrollable content =====
    auto *scrollContent = new QWidget(this);
    scrollContent->setStyleSheet("background: transparent;");
    auto *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(0);
    scrollLayout->addWidget(m_splitter, 1);
    scrollLayout->addWidget(m_historyBar, 0);

    auto *outerScroll = new QScrollArea(this);
    outerScroll->setWidgetResizable(true);
    outerScroll->setFrameShape(QFrame::NoFrame);
    outerScroll->setWidget(scrollContent);
    outerScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    outerScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outerScroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { width: 0; background: transparent; }"
        "QScrollBar::handle:vertical { background: transparent; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }");

    mainLayout->addWidget(outerScroll, 1);

    // ===== Status Bar =====
    auto *statusBar = new QWidget(this);
    statusBar->setFixedHeight(24);
    statusBar->setStyleSheet("border-top: 1px solid rgba(255,255,255,0.02); background: rgba(4,4,6,0.95);");
    auto *sbLayout = new QHBoxLayout(statusBar);
    sbLayout->setContentsMargins(28, 0, 28, 0);
    auto *leftInfo = new QLabel("IMMERSIVE PLAYER v1.0.0    基于 FFmpeg · Qt · OpenGL", statusBar);
    leftInfo->setStyleSheet("font-size: 9px; color: rgba(255,255,255,0.06);");
    auto *rightInfo = new QLabel("MP4 MKV AVI MOV FLV TS    Windows 11", statusBar);
    rightInfo->setStyleSheet("font-size: 9px; color: rgba(255,255,255,0.06);");
    sbLayout->addWidget(leftInfo);
    sbLayout->addStretch();
    sbLayout->addWidget(rightInfo);
    mainLayout->addWidget(statusBar);

    updateCardStates();
}

void HomePage::updateCardStates() {
    bool hasSelection = m_manager->selectedFile() != nullptr;
    for (auto *card : m_cards) {
        bool available = (card->sceneType() == SceneType::Concert || card->sceneType() == SceneType::Normal || card->sceneType() == SceneType::Cinema || card->sceneType() == SceneType::TechPlaza);
        card->setEnabled(hasSelection && available);
    }
}

void HomePage::updateSplitterConstraints() {
    int w = m_splitter->width();
    if (w <= 0) return;
    m_playlist->setMinimumWidth(qMax(120, int(w * 0.15)));
    m_playlist->setMaximumWidth(int(w * 0.40));
}

bool HomePage::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_splitter && event->type() == QEvent::Resize) {
        updateSplitterConstraints();
    }
    return QWidget::eventFilter(obj, event);
}
