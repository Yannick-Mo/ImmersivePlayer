#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <memory>
#include "common/Types.h"
#include "ui/SceneControlPanel.h"
#include "ui/widgets/SpeedPopup.h"
#include "ui/AudioVisualizerPopup.h"

class VideoManager;
class OpenGLWidget;
class Scene;
class SessionManager;
class PlaylistPopup;
class LoadingOverlay;

class ScenePlayerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ScenePlayerWidget(SessionManager *sessionManager, QWidget *parent = nullptr);
    ~ScenePlayerWidget();

    void loadFile(const QString &filePath, SceneType sceneType = SceneType::Concert);
    void unload();
    std::shared_ptr<VideoManager> videoManager() const { return m_manager; }

signals:
    void backToHome();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void createScene(SceneType type);
    void togglePanel();
    void updateControls();
    double m_currentSpeed = 1.0;

    SessionManager *m_sessionManager;
    std::shared_ptr<VideoManager> m_manager;
    OpenGLWidget *m_sceneWidget;
    SceneControlPanel *m_controlPanel;

    QTimer *m_playTimer;
    PlaylistPopup *m_playlistPopup = nullptr;
    SpeedPopup *m_speedPopup = nullptr;
    AudioVisualizerPopup *m_visPopup = nullptr;
    QPushButton *m_exitBtn;

    bool m_panelVisible = false;
    bool m_hasPrevious = false;
    bool m_hasNext = false;
    QString m_currentFilePath;
    SceneType m_currentSceneType = SceneType::Concert;

    QLabel *m_liveLabel = nullptr;
    LoadingOverlay *m_loadingOverlay = nullptr;
    int64_t m_bufferingStartMs = 0;
    QTimer *m_bufferingTimer = nullptr;
    bool m_isNetworkFile = false;
    int m_openToken = 0;
};
