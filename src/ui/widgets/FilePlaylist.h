#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

class SessionManager;
struct FileItem;

class FilePlaylist : public QWidget {
    Q_OBJECT
public:
    explicit FilePlaylist(SessionManager *manager, QWidget *parent = nullptr);

signals:
    void addFilesRequested();
    void networkRequested();

private:
    void refresh();
    QWidget* createFileItemWidget(const FileItem &item, int index, bool selected);

    SessionManager *m_manager;
    QVBoxLayout *m_listLayout;
    QLabel *m_countLabel;
    QWidget *m_listContainer;
};
