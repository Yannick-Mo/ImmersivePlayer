#pragma once
#include <QWidget>
#include <QStackedWidget>
#include "common/Types.h"

class HomePage;
class NormalPlayerWidget;
class ScenePlayerWidget;
class SessionManager;
class TitleBar;
class MediaLibraryDialog;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    void showHomePage();
    void showNormalPlayer(const QString &filePath);
    void showScenePlayer(const QString &filePath, SceneType sceneType = SceneType::Concert);

    void enterVideoFullscreen();
    void exitVideoFullscreen();

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void changeEvent(QEvent *event) override;

private:
    QStackedWidget *m_stack;
    TitleBar *m_titleBar;
    HomePage *m_homePage;
    NormalPlayerWidget *m_normalPlayer;
    ScenePlayerWidget *m_scenePlayer;
    SessionManager *m_sessionManager;

    bool m_videoFullscreen = false;
};
