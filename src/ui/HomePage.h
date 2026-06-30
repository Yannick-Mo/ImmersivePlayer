#pragma once
#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QEvent>
#include "common/Types.h"
#include "widgets/SceneCard.h"

// 前向声明，减少编译依赖
class QSplitter;
class SessionManager;
class FilePlaylist;
class HistoryBar;
class HistoryDialog;

// ============================================================================
// HomePage — 应用首页
// ============================================================================
// 启动后显示的第一个页面，主要由两部分组成（通过 QSplitter 左右分割）：
//   - 左侧：文件播放列表（FilePlaylist），支持拖拽添加文件，选中后右侧卡片可用
//   - 右侧：场景卡片网格（SceneCard），点击卡片进入对应场景/普通播放器
//
// 同时还集成了历史记录栏（HistoryBar）和播放会话管理（SessionManager），
// 支持断点续播、播放进度恢复等功能。
// ============================================================================
class HomePage : public QWidget {
    Q_OBJECT
public:
    // 构造函数
    // manager: SessionManager 实例，用于读取/保存播放会话（记录播放进度）
    explicit HomePage(SessionManager *manager, QWidget *parent = nullptr);

signals:
    // 用户点击场景卡片时发出，请求切换到沉浸式场景播放器
    // scene: 场景类型（Concert / LiveHouse 等）
    void openScenePlayer(const QString &filePath, SceneType scene);

    // 用户双击播放列表中的文件时发出，请求切换到普通播放器
    void openNormalPlayer(const QString &filePath);

    // 用户点击"添加文件"按钮时发出，请求打开文件选择对话框
    void addFilesRequested();

protected:
    // 事件过滤器：监听子控件的事件（如场景卡片的点击、播放列表的拖拽等）
    // 用于在 HomePage 层面统一处理子控件的交互逻辑
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    // 初始化 UI 布局：创建 QSplitter、卡片网格、播放列表、历史栏等
    void setupUI();

    // 更新所有场景卡片的状态（如根据是否播放过显示"继续播放"或"开始"）
    void updateCardStates();

    // 更新 QSplitter 的拖拽约束（最小/最大比例），防止用户过度拖拽导致布局异常
    void updateSplitterConstraints();

    // 左右分割器，左侧为播放列表区，右侧为场景卡片区
    QSplitter *m_splitter;

    // 播放会话管理器，负责持久化/恢复播放进度
    SessionManager *m_manager;

    // 左侧文件播放列表，支持拖拽添加视频文件
    FilePlaylist *m_playlist;

    // 底部历史记录栏，显示最近播放的视频
    HistoryBar *m_historyBar;

    // 历史记录弹窗（懒加载，点击历史栏时弹出），展示完整播放历史
    HistoryDialog *m_historyDialog = nullptr;

    // 场景卡片网格布局，用于排列多个 SceneCard
    QGridLayout *m_cardGrid;

    // 当前选中的场景名称标签，显示在网格上方
    QLabel *m_selectedName;

    // 场景卡片数组，存储所有 SceneCard 指针以便批量更新状态
    QVector<SceneCard*> m_cards;
};