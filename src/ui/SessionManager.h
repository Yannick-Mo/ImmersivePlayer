#pragma once
#include <QObject>
#include <QStringList>
#include <QVector>
#include "common/Types.h"

class SessionManager : public QObject {
    Q_OBJECT
public:
    explicit SessionManager(QObject *parent = nullptr);

    // File list
    void addFiles(const QStringList &paths);
    void addFile(const QString &filePath, const QString &displayName);
    void addNetworkFile(const QString &url);
    static bool isNetworkUrl(const QString &path);
    static QString urlToDisplayName(const QString &url);
    void removeFile(int index);
    void clearFiles();
    const QVector<FileItem>& files() const { return m_files; }

    // Selection
    void selectFile(int index);
    int selectedIndex() const { return m_selectedIndex; }
    const FileItem* selectedFile() const;

    // History
    void addHistory(const FileItem &file, SceneType scene);
    void removeHistory(int index);
    void clearHistory();
    const QVector<HistoryItem>& history() const { return m_history; }
    void loadHistory();
    void saveHistory();

    // Recent URLs
    QStringList recentUrls() const { return m_recentUrls; }
    void addRecentUrl(const QString &url);
    void clearRecentUrls();

    // Media Library
    bool addToLibrary(const QString &sourcePath);
    bool addNetworkToLibrary(const QString &url);
    void removeFromLibrary(int index);
    const QVector<MediaLibraryItem>& libraryFiles() const { return m_library; }
    QString libraryDir() const { return m_libraryDir; }

signals:
    void filesChanged();
    void selectionChanged(int index);
    void historyChanged();
    void libraryChanged();

private:
    void loadLibrary();
    void saveLibrary();

    QVector<FileItem> m_files;
    int m_selectedIndex = -1;
    QVector<HistoryItem> m_history;
    QVector<MediaLibraryItem> m_library;
    QString m_historyPath;
    QString m_libraryDir;
    QStringList m_recentUrls;
    QString m_recentUrlsPath;
    void loadRecentUrls();
    void saveRecentUrls();
    static constexpr int kMaxRecentUrls = 10;
    static constexpr int kMaxHistory = 20;
};
