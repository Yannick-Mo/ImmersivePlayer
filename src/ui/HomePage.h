#pragma once
#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QEvent>
#include "common/Types.h"
#include "widgets/SceneCard.h"

class QSplitter;
class SessionManager;
class FilePlaylist;
class HistoryBar;
class HistoryDialog;

class HomePage : public QWidget {
    Q_OBJECT
public:
    explicit HomePage(SessionManager *manager, QWidget *parent = nullptr);

signals:
    void openScenePlayer(const QString &filePath, SceneType scene);
    void openNormalPlayer(const QString &filePath);
    void addFilesRequested();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUI();
    void updateCardStates();
    void updateSplitterConstraints();

    QSplitter *m_splitter;
    SessionManager *m_manager;
    FilePlaylist *m_playlist;
    HistoryBar *m_historyBar;
    HistoryDialog *m_historyDialog = nullptr;
    QGridLayout *m_cardGrid;
    QLabel *m_selectedName;
    QVector<SceneCard*> m_cards;
};
