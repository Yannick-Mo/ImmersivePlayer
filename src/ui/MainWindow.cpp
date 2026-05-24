#include "ui/MainWindow.h"
#include "ui/HomePage.h"
#include "ui/NormalPlayerWidget.h"
#include "ui/ScenePlayerWidget.h"
#include "ui/SessionManager.h"
#include "ui/widgets/TitleBar.h"
#include "ui/widgets/MediaLibraryDialog.h"
#include <QFileInfo>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
{
    setWindowTitle("IMMERSIVE PLAYER");
    resize(900, 650);
    setMinimumSize(800, 600);
    setStyleSheet("MainWindow { background: #101114; }");
    setAttribute(Qt::WA_Hover);

    m_sessionManager = new SessionManager(this);

    m_titleBar = new TitleBar(this);

    m_stack = new QStackedWidget(this);
    m_stack->setStyleSheet("background: #101114;");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_titleBar);
    layout->addWidget(m_stack, 1);

    m_homePage     = new HomePage(m_sessionManager, this);
    m_normalPlayer = new NormalPlayerWidget(m_sessionManager, this);
    m_scenePlayer  = new ScenePlayerWidget(m_sessionManager, this);

    m_stack->addWidget(m_homePage);      // index 0
    m_stack->addWidget(m_normalPlayer);  // index 1
    m_stack->addWidget(m_scenePlayer);   // index 2

    // TitleBar window controls
    connect(m_titleBar, &TitleBar::minimizeClicked, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeClicked, this, [this]() {
        if (isMaximized()) showNormal();
        else showMaximized();
    });
    connect(m_titleBar, &TitleBar::closeClicked, this, &QWidget::close);

    // TitleBar → media library
    connect(m_titleBar, &TitleBar::mediaLibraryClicked, this, [this]() {
        MediaLibraryDialog dlg(m_sessionManager, this);
        dlg.exec();
    });

    // HomePage → open normal player
    connect(m_homePage, &HomePage::openNormalPlayer, this, [this](const QString &path) {
        m_sessionManager->addHistory(FileItem{path, QFileInfo(path).fileName()}, SceneType::Normal);
        showNormalPlayer(path);
    });

    // HomePage → open scene player
    connect(m_homePage, &HomePage::openScenePlayer, this, [this](const QString &path, SceneType scene) {
        m_sessionManager->addHistory(FileItem{path, QFileInfo(path).fileName()}, scene);
        showScenePlayer(path, scene);
    });

    // Players → back to home
    connect(m_normalPlayer, &NormalPlayerWidget::backToHome, this, &MainWindow::showHomePage);
    connect(m_scenePlayer, &ScenePlayerWidget::backToHome, this, &MainWindow::showHomePage);

    m_stack->setCurrentIndex(0);
}

void MainWindow::showHomePage() {
    exitVideoFullscreen();
    m_normalPlayer->unload();
    m_scenePlayer->unload();
    m_stack->setCurrentIndex(0);
}

void MainWindow::enterVideoFullscreen() {
    if (m_videoFullscreen) return;
    m_videoFullscreen = true;
    m_titleBar->hide();
    showFullScreen();
}

void MainWindow::exitVideoFullscreen() {
    if (!m_videoFullscreen) return;
    m_videoFullscreen = false;
    m_titleBar->show();
    showNormal();
}

void MainWindow::showNormalPlayer(const QString &filePath) {
    m_normalPlayer->loadFile(filePath);
    m_stack->setCurrentIndex(1);
}

void MainWindow::showScenePlayer(const QString &filePath, SceneType sceneType) {
    m_scenePlayer->loadFile(filePath, sceneType);
    m_stack->setCurrentIndex(2);
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG") return false;
    auto *msg = static_cast<MSG*>(message);

    if (msg->message == WM_NCHITTEST && !isFullScreen()) {
        int x = GET_X_LPARAM(msg->lParam);
        int y = GET_Y_LPARAM(msg->lParam);

        POINT pt = { x, y };
        ScreenToClient(msg->hwnd, &pt);

        RECT rc;
        GetClientRect(msg->hwnd, &rc);

        const int border = 4;
        bool top    = pt.y <= border;
        bool bottom = pt.y >= rc.bottom - border;
        bool left   = pt.x <= border;
        bool right  = pt.x >= rc.right - border;

        if (top && left)       { *result = HTTOPLEFT; return true; }
        if (top && right)      { *result = HTTOPRIGHT; return true; }
        if (bottom && left)    { *result = HTBOTTOMLEFT; return true; }
        if (bottom && right)   { *result = HTBOTTOMRIGHT; return true; }
        if (top)               { *result = HTTOP; return true; }
        if (bottom)            { *result = HTBOTTOM; return true; }
        if (left)              { *result = HTLEFT; return true; }
        if (right)             { *result = HTRIGHT; return true; }

    }
#else
    Q_UNUSED(eventType); Q_UNUSED(message); Q_UNUSED(result);
#endif
    return false;
}

void MainWindow::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        if (!isFullScreen() && m_videoFullscreen) {
            m_titleBar->show();
            m_videoFullscreen = false;
        }
        m_titleBar->updateMaximizeButton(isMaximized());
    }
}
