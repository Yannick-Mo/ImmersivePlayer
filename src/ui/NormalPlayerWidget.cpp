#include "NormalPlayerWidget.h"
#include "ui/MainWindow.h"
#include "ui/SessionManager.h"
#include "ui/widgets/PlaylistPopup.h"
#include "ui/widgets/SpeedPopup.h"
#include "core/VideoManager.h"
#include "render/OpenGLWidget.h"
#include "ui/widgets/ConfirmDialog.h"
#include "ui/widgets/LoadingOverlay.h"
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QFileInfo>
#include <QTimer>
#include <QApplication>
#include <thread>
#include <chrono>

NormalPlayerWidget::NormalPlayerWidget(SessionManager *sessionManager, QWidget *parent)
    : QWidget(parent), m_sessionManager(sessionManager)
{
    m_manager = std::make_shared<VideoManager>();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_videoWidget = new OpenGLWidget(this);
    m_videoWidget->setMinimumSize(640, 360);
    m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_videoWidget, 1);

    m_controlPanel = new SceneControlPanel(this);
    m_controlPanel->hide();

    m_exitBtn = new QPushButton(QStringLiteral("\xe2\x9c\x95"), this);
    m_exitBtn->setFixedSize(32, 32);
    m_exitBtn->setStyleSheet(
        "QPushButton { background: rgba(0,0,0,0.3); border: 1px solid rgba(255,255,255,0.06);"
        "border-radius: 16px; font-size: 14px; color: rgba(255,255,255,0.25); }"
        "QPushButton:hover { background: rgba(255,255,255,0.05); color: rgba(255,255,255,0.5); }");
    connect(m_exitBtn, &QPushButton::clicked, this, &NormalPlayerWidget::backToHome);

    // ── Loading overlay ──
    m_loadingOverlay = new LoadingOverlay(this);

    // ── Live label ──
    m_liveLabel = new QLabel(QStringLiteral("\u25CF LIVE"), this);
    m_liveLabel->setStyleSheet(
        "background: rgba(255,0,0,0.15); color: #ff4444; font-size: 11px; font-weight: 600; "
        "padding: 4px 10px; border-radius: 4px;");
    m_liveLabel->hide();
    m_liveLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    // ── Hide panel and popups on exit ──
    connect(this, &NormalPlayerWidget::backToHome, this, [this]() {
        if (m_controlPanel->isVisible()) {
            m_controlPanel->hide();
            m_panelVisible = false;
        }
        if (m_visPopup) {
            m_visPopup->detachSource();
            m_visPopup->hide();
        }
    });

    // ── Control panel connections ──
    connect(m_controlPanel, &SceneControlPanel::playPauseRequested, this, [this]() {
        // 解码器未运行或加载中时忽略播放/暂停
        if (!m_manager->isRunning()) return;
        if (m_manager->isPaused()) m_manager->play();
        else m_manager->pause();
    });

    connect(m_controlPanel, &SceneControlPanel::seekRequested, this, [this](double ratio) {
        // 解码器未运行时不处理拖拽
        if (!m_manager->isRunning()) return;
        m_manager->seek(ratio);
    });

    connect(m_controlPanel, &SceneControlPanel::speedMenuRequested, this, [this](QPoint anchor) {
        if (!m_speedPopup) {
            m_speedPopup = new SpeedPopup(this);
            connect(m_speedPopup, &SpeedPopup::speedSelected, this, [this](double speed) {
                m_currentSpeed = speed;
                m_manager->applySpeed(speed);
                m_controlPanel->setSpeed(speed);
            });
        }
        m_speedPopup->setCurrentSpeed(m_currentSpeed);
        m_speedPopup->showAt(anchor);
    });

    connect(m_controlPanel, &SceneControlPanel::volumeChanged, this, [this](double vol) {
        m_manager->setVolume(vol);
    });

    connect(m_controlPanel, &SceneControlPanel::muteToggled, this, [this]() {
        bool muted = !m_manager->isMuted();
        m_manager->setMuted(muted);
        m_controlPanel->setMuted(muted);
    });

    connect(m_controlPanel, &SceneControlPanel::prevRequested, this, [this]() {
        int idx = m_sessionManager->selectedIndex();
        if (idx > 0) {
            m_sessionManager->selectFile(idx - 1);
            const FileItem *sel = m_sessionManager->selectedFile();
            if (sel) {
                unload();
                loadFile(sel->filePath);
            }
        }
    });

    connect(m_controlPanel, &SceneControlPanel::nextRequested, this, [this]() {
        int idx = m_sessionManager->selectedIndex();
        if (idx >= 0 && idx < m_sessionManager->files().size() - 1) {
            m_sessionManager->selectFile(idx + 1);
            const FileItem *sel = m_sessionManager->selectedFile();
            if (sel) {
                unload();
                loadFile(sel->filePath);
            }
        }
    });

    connect(m_controlPanel, &SceneControlPanel::fullscreenRequested, this, [this]() {
        auto *mw = qobject_cast<MainWindow*>(window());
        if (mw) {
            if (mw->isFullScreen())
                mw->exitVideoFullscreen();
            else
                mw->enterVideoFullscreen();
        }
    });

    connect(m_controlPanel, &SceneControlPanel::closeRequested, this, [this]() {
        m_controlPanel->hide();
        m_panelVisible = false;
    });

    connect(m_controlPanel, &SceneControlPanel::waveformToggleRequested, this, [this]() {
        if (!m_visPopup) {
            m_visPopup = new AudioVisualizerPopup(this);
            connect(m_visPopup, &AudioVisualizerPopup::volumeChanged, this, [this](double v) {
                m_manager->setVolume(v);
                m_controlPanel->setVolume(v);
            });
            connect(m_visPopup, &AudioVisualizerPopup::muteToggled, this, [this]() {
                bool muted = !m_manager->isMuted();
                m_manager->setMuted(muted);
                m_controlPanel->setMuted(muted);
                m_visPopup->setMuted(muted);
            });
            connect(m_visPopup, &AudioVisualizerPopup::fullscreenRequested, this, [this]() {
                auto *mw = qobject_cast<MainWindow*>(window());
                if (mw) {
                    if (mw->isFullScreen())
                        mw->exitVideoFullscreen();
                    else
                        mw->enterVideoFullscreen();
                }
            });
        }
        m_visPopup->setAudioSource(m_manager->getAudioRenderer());
        m_visPopup->setVolume(m_manager->getVolume());
        m_visPopup->setMuted(m_manager->isMuted());
        m_visPopup->setVisible(!m_visPopup->isVisible());
    });

    // ── Playlist popup ──
    connect(m_controlPanel, &SceneControlPanel::showPlaylistRequested, this, [this]() {
        if (!m_playlistPopup) {
            m_playlistPopup = new PlaylistPopup(m_sessionManager, this);
            connect(m_playlistPopup, &PlaylistPopup::fileSelected, this, [this](const QString &path, int index) {
                m_sessionManager->selectFile(index);
                unload();
                loadFile(path);
            });
        }
        m_playlistPopup->setCurrentHighlight(m_sessionManager->selectedIndex());
        QPoint anchor = m_controlPanel->mapToGlobal(QPoint(m_controlPanel->width() / 2, 0));
        m_playlistPopup->showAt(anchor);
    });

    // ── Position polling timer ──
    auto *posTimer = new QTimer(this);
    connect(posTimer, &QTimer::timeout, this, [this]() {
        if (m_manager && m_manager->isRunning()) {
            bool isLive = m_manager->isLiveStream();
            m_liveLabel->setVisible(isLive);

            int64_t dur = m_manager->getDuration();
            double durSec = dur / 1000000.0;
            double progress = m_manager->getProgress();
            double posSec = progress * durSec;
            m_controlPanel->updatePosition(isLive ? 0.0 : posSec, isLive ? 0.0 : durSec);
            m_controlPanel->updateProgress(isLive ? 0.0 : progress);
            m_controlPanel->setPlayButtonState(!m_manager->isPaused());

            // Buffering detection (debounced 500ms)
            int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            if (m_manager->isBuffering()) {
                if (m_bufferingStartMs == 0) m_bufferingStartMs = now;
            } else {
                m_loadingOverlay->hideImmediately();
                m_bufferingStartMs = 0;
            }
            if (m_bufferingStartMs > 0 && (now - m_bufferingStartMs) >= 500) {
                m_loadingOverlay->showWithDelay(0);
            }
        }
    });
    posTimer->start(250);

    // ── Play timer (cancellable, prevents stale callbacks on rapid file switching) ──
    m_playTimer = new QTimer(this);
    m_playTimer->setSingleShot(true);
    connect(m_playTimer, &QTimer::timeout, this, [this]() {
        if (m_manager->isRunning())
            m_manager->play();
    });

    m_bufferingTimer = new QTimer(this);
    m_bufferingTimer->setSingleShot(true);
    connect(m_bufferingTimer, &QTimer::timeout, this, [this]() {
        if (m_loadingOverlay)
            m_loadingOverlay->showWithDelay(0);
    });

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

NormalPlayerWidget::~NormalPlayerWidget() {}

void NormalPlayerWidget::loadFile(const QString &filePath) {
    m_currentFilePath = filePath;
    m_isNetworkFile = SessionManager::isNetworkUrl(filePath);
    m_liveLabel->hide();
    m_loadingOverlay->hideImmediately();
    m_bufferingTimer->stop();

    m_videoWidget->setVideoDecoder(m_manager);

    if (m_isNetworkFile) {
        // ── 异步打开网络文件：保持 UI 响应 ──
        int token = ++m_openToken;

        m_loadingOverlay->showWithDelay(0);
        QApplication::processEvents();

        // 显示"连接中"提示，5 秒后如果还没完成则维持 overlay
        m_bufferingTimer->start(5000);

        std::thread([this, filePath, token, manager = m_manager]() {
            bool ok = manager->open(filePath.toStdString());

            QMetaObject::invokeMethod(this, [this, ok, filePath, token, manager]() {
                if (token != m_openToken) {
                    if (ok && manager->isRunning()) {
                        manager->close();
                    }
                    return;
                }

                if (ok) {
                    m_loadingOverlay->hideImmediately();
                    m_bufferingTimer->stop();

                    MediaInfo info;
                    info.filePath = filePath;
                    info.fileName = SessionManager::urlToDisplayName(filePath);
                    info.width = manager->getWidth();
                    info.height = manager->getHeight();
                    info.duration = manager->getDuration() / 1000000.0;
                    info.scene = SceneType::Normal;
                    m_controlPanel->updateMediaInfo(info);

                    m_playTimer->start(100);
                } else {
                    m_loadingOverlay->hideImmediately();
                    m_bufferingTimer->stop();

                    QString msg = QStringLiteral("\u65e0\u6cd5\u8fde\u63a5\u5230\u7f51\u7edc\u89c6\u9891\uff0c\u8bf7\u68c0\u67e5\u94fe\u63a5\u662f\u5426\u6b63\u786e\uff1a\n%1").arg(filePath);
                    ConfirmDialog::info(this, QStringLiteral("\u7f51\u7edc\u9519\u8bef"), msg);
                    emit backToHome();
                }
            }, Qt::QueuedConnection);
        }).detach();
    } else {
        // ── 本地文件：同步打开（快速，不阻塞） ──
        if (m_manager->open(filePath.toStdString())) {
            MediaInfo info;
            info.filePath = filePath;
            info.fileName = QFileInfo(filePath).fileName();
            info.width = m_manager->getWidth();
            info.height = m_manager->getHeight();
            info.duration = m_manager->getDuration() / 1000000.0;
            info.scene = SceneType::Normal;
            m_controlPanel->updateMediaInfo(info);
            m_playTimer->start(100);
        } else {
            QString msg = QStringLiteral("\u65e0\u6cd5\u6253\u5f00\u6587\u4ef6\uff0c\u53ef\u80fd\u5df2\u88ab\u79fb\u52a8\u6216\u5220\u9664\uff1a\n%1").arg(filePath);
            ConfirmDialog::info(this, QStringLiteral("\u6587\u4ef6\u672a\u627e\u5230"), msg);
            emit backToHome();
        }
    }
}

void NormalPlayerWidget::unload() {
    // 1. 先递增 token，使任何未完成的异步回调无效化
    int oldToken = m_openToken;
    ++m_openToken;

    // 2. 取消正在进行的 I/O（avformat_open_input 等会被中断）
    m_manager->cancelOpen();

    // 3. 完全关闭解码器、音频等资源
    m_manager->close();

    // 4. 停止所有定时器
    m_playTimer->stop();
    m_bufferingTimer->stop();
    m_loadingOverlay->hideImmediately();

    // 5. 清理可视化弹窗
    if (m_visPopup) {
        m_visPopup->detachSource();
        m_visPopup->hide();
    }

    (void)oldToken;  // 未使用但保留语义清晰
}

void NormalPlayerWidget::togglePanel() {
    m_panelVisible = !m_panelVisible;
    m_controlPanel->setVisible(m_panelVisible);
    if (m_panelVisible) updateControls();
}

void NormalPlayerWidget::updateControls() {
    int idx = m_sessionManager->selectedIndex();
    m_controlPanel->setPrevNextEnabled(idx > 0, idx >= 0 && idx < m_sessionManager->files().size() - 1);
    m_controlPanel->setVolume(m_manager->getVolume());
    m_controlPanel->setMuted(m_manager->isMuted());
    m_controlPanel->setSpeed(m_currentSpeed);

    bool isLive = m_manager->isLiveStream();
    int64_t dur = m_manager->getDuration();
    double durSec = dur / 1000000.0;
    double progress = m_manager->getProgress();
    m_controlPanel->updatePosition(isLive ? 0.0 : progress * durSec, isLive ? 0.0 : durSec);
    m_controlPanel->updateProgress(isLive ? 0.0 : progress);
    m_controlPanel->setPlayButtonState(!m_manager->isPaused());
}

void NormalPlayerWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_exitBtn)
        m_exitBtn->move(width() - 44, 12);
    if (m_liveLabel)
        m_liveLabel->move(width() - m_liveLabel->width() - 16, 12);
    if (m_controlPanel && !m_controlPanel->isUserDragged()) {
        int pw = m_controlPanel->width();
        m_controlPanel->move((width() - pw) / 2, height() - m_controlPanel->height() - 20);
    }
    if (m_loadingOverlay) {
        m_loadingOverlay->resize(size());
        m_loadingOverlay->move(0, 0);
    }
}

void NormalPlayerWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        if (window()->isFullScreen()) {
            auto *mw = qobject_cast<MainWindow*>(window());
            if (mw) mw->exitVideoFullscreen();
        } else if (m_controlPanel->isVisible()) {
            m_controlPanel->hide();
            m_panelVisible = false;
        } else {
            emit backToHome();
        }
    } else if (event->key() == Qt::Key_Space) {
        if (!m_manager->isRunning()) return;
        if (m_manager->isPaused()) m_manager->play();
        else m_manager->pause();
    } else if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Alt) {
        togglePanel();
    }
    QWidget::keyPressEvent(event);
}

void NormalPlayerWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::MiddleButton) {
        togglePanel();
    }
    QWidget::mousePressEvent(event);
}