#pragma once
#include <QFrame>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include "common/Types.h"

class SessionManager;

class PlaylistPopup : public QFrame {
    Q_OBJECT
public:
    explicit PlaylistPopup(SessionManager *manager, QWidget *parent = nullptr);

    void showAt(const QPoint &globalPos, int preferredWidth = 380);
    void setCurrentHighlight(int index);

signals:
    void fileSelected(const QString &filePath, int index);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupUI();
    void populateList(const QString &filter = QString());

    SessionManager *m_manager;
    QLineEdit *m_searchEdit;
    QListWidget *m_listWidget;
    QLabel *m_countLabel;
    int m_currentIndex = -1;
};
