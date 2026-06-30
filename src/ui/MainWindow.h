#pragma once
#include <QWidget>
#include <QStackedWidget>
#include "common/Types.h"

// 前向声明
class HomePage;
class NormalPlayerWidget;
class ScenePlayerWidget;
class SessionManager;
class TitleBar;
class MediaLibraryDialog;

// ============================================================================
// MainWindow — 应用主窗口
// ============================================================================
// 作为整个应用的顶层容器，使用 QStackedWidget 管理三个主要页面：
//   1. HomePage           — 首页（媒体库、文件浏览）
//   2. NormalPlayerWidget — 普通视频播放器（2D / 平面播放）
//   3. ScenePlayerWidget  — 沉浸式场景播放器（如演唱会 VR 场景）
//
// 通过 showHomePage() / showNormalPlayer() / showScenePlayer() 切换页面。
// 同时负责全屏切换、Windows 原生消息处理（如屏幕 DPI 变化）等窗口级功能。
// ============================================================================
class MainWindow : public QWidget {
    Q_OBJECT
public:
    // 构造函数，parent 为 Qt 窗口层级中的父窗口
    explicit MainWindow(QWidget *parent = nullptr);

    // -------------------------------------------------------------------------
    // 页面切换
    // -------------------------------------------------------------------------

    // 切换到首页，显示媒体库和文件浏览界面
    void showHomePage();

    // 切换到普通播放器页面，播放指定路径的视频文件
    void showNormalPlayer(const QString &filePath);

    // 切换到沉浸式场景播放器页面
    // sceneType: 场景类型，如 Concert（演唱会）、LiveHouse 等
    void showScenePlayer(const QString &filePath, SceneType sceneType = SceneType::Concert);

    // -------------------------------------------------------------------------
    // 全屏控制
    // -------------------------------------------------------------------------

    // 进入视频全屏模式（隐藏标题栏，最大化窗口）
    void enterVideoFullscreen();

    // 退出视频全屏模式（恢复标题栏和窗口大小）
    void exitVideoFullscreen();

protected:
    // 处理 Windows 原生消息（如 WM_DPICHANGED 等），用于 DPI 缩放适配
    // eventType: "windows_generic_MSG"
    // message:  MSG 结构体指针
    // result:   消息处理结果传出
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

    // 窗口状态变化事件（最大化、最小化、全屏等），用于同步全屏状态
    void changeEvent(QEvent *event) override;

private:
    // 页面栈容器，用于在首页、普通播放器、场景播放器之间切换
    QStackedWidget *m_stack;

    // 自定义标题栏，支持全屏时隐藏
    TitleBar *m_titleBar;

    // 首页（媒体库），由 QStackedWidget 管理
    HomePage *m_homePage;

    // 普通播放器页面，由 QStackedWidget 管理
    NormalPlayerWidget *m_normalPlayer;

    // 沉浸式场景播放器页面，由 QStackedWidget 管理
    ScenePlayerWidget *m_scenePlayer;

    // 播放会话管理器，负责记录/恢复播放进度
    SessionManager *m_sessionManager;

    // 视频全屏状态标志，由 enterVideoFullscreen() / exitVideoFullscreen() 控制
    bool m_videoFullscreen = false;
};