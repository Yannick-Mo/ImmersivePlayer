#include "SessionManager.h"
#include <algorithm>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDateTime>
#include <QUuid>
#include <QUrl>

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    m_historyPath = dataDir + "/history.json";
    m_libraryDir = dataDir + "/library/";
    QDir().mkpath(m_libraryDir);
    loadHistory();
    m_recentUrlsPath = dataDir + "/recent_urls.json";
    loadRecentUrls();
    loadLibrary();
}

void SessionManager::addFiles(const QStringList &paths) {
    for (const auto &path : paths) {
        QFileInfo fi(path);
        if (!fi.exists()) continue;
        // Skip duplicates
        bool dup = false;
        for (const auto &f : m_files) {
            if (f.filePath == path) { dup = true; break; }
        }
        if (dup) continue;
        m_files.push_back({path, fi.fileName()});
    }
    emit filesChanged();
}

void SessionManager::addFile(const QString &filePath, const QString &displayName) {
    if (isNetworkUrl(filePath)) {
        for (const auto &f : m_files) {
            if (f.filePath == filePath) return;
        }
        FileItem item;
        item.filePath = filePath;
        item.fileName = displayName;
        m_files.push_back(item);
        emit filesChanged();
        return;
    }
    QFileInfo fi(filePath);
    if (!fi.exists()) return;
    for (const auto &f : m_files) {
        if (f.filePath == filePath) return;
    }
    FileItem item;
    item.filePath = filePath;
    item.fileName = displayName;
    m_files.push_back(item);
    emit filesChanged();
}

void SessionManager::addNetworkFile(const QString &url) {
    for (const auto &f : m_files) {
        if (f.filePath == url) return;
    }
    FileItem item;
    item.filePath = url;
    item.fileName = urlToDisplayName(url);
    m_files.push_back(item);
    emit filesChanged();
    addRecentUrl(url);
}

bool SessionManager::isNetworkUrl(const QString &path) {
    return path.startsWith("http://") || path.startsWith("https://")
        || path.startsWith("rtmp://") || path.startsWith("rtsp://");
}

QString SessionManager::urlToDisplayName(const QString &url) {
    QUrl qurl(url);
    QString name = qurl.fileName();
    if (name.isEmpty())
        name = qurl.host();
    if (name.isEmpty())
        name = url;
    return name;
}

void SessionManager::addRecentUrl(const QString &url) {
    m_recentUrls.removeAll(url);
    m_recentUrls.prepend(url);
    if (m_recentUrls.size() > kMaxRecentUrls)
        m_recentUrls.resize(kMaxRecentUrls);
    saveRecentUrls();
}

void SessionManager::clearRecentUrls() {
    m_recentUrls.clear();
    saveRecentUrls();
}

void SessionManager::loadRecentUrls() {
    QFile f(m_recentUrlsPath);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isArray()) return;
    m_recentUrls.clear();
    for (const auto &val : doc.array())
        m_recentUrls.append(val.toString());
}

void SessionManager::saveRecentUrls() {
    QJsonArray arr;
    for (const auto &url : m_recentUrls)
        arr.append(url);
    QFile f(m_recentUrlsPath);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(arr).toJson());
        f.close();
    }
}

void SessionManager::removeFile(int index) {
    if (index < 0 || index >= m_files.size()) return;
    m_files.removeAt(index);
    if (m_selectedIndex == index) {
        m_selectedIndex = -1;
    } else if (m_selectedIndex > index) {
        m_selectedIndex--;
    }
    emit filesChanged();
    emit selectionChanged(m_selectedIndex);
}

void SessionManager::clearFiles() {
    m_files.clear();
    m_selectedIndex = -1;
    emit filesChanged();
    emit selectionChanged(-1);
}

void SessionManager::selectFile(int index) {
    if (index < -1 || index >= m_files.size()) return;
    m_selectedIndex = index;
    emit selectionChanged(index);
}

const FileItem* SessionManager::selectedFile() const {
    if (m_selectedIndex < 0 || m_selectedIndex >= m_files.size())
        return nullptr;
    return &m_files[m_selectedIndex];
}

void SessionManager::addHistory(const FileItem &file, SceneType scene) {
    // Remove duplicate entry for same file+scene
    m_history.erase(
        std::remove_if(m_history.begin(), m_history.end(),
            [&](const HistoryItem &h) {
                return h.file.filePath == file.filePath && h.scene == scene;
            }),
        m_history.end());

    HistoryItem h;
    h.file = file;
    h.scene = scene;
    h.timestamp = QDateTime::currentSecsSinceEpoch();
    m_history.prepend(h);

    if (m_history.size() > kMaxHistory)
        m_history.resize(kMaxHistory);

    emit historyChanged();
    saveHistory();
}

void SessionManager::removeHistory(int index) {
    if (index < 0 || index >= m_history.size()) return;
    m_history.removeAt(index);
    emit historyChanged();
    saveHistory();
}

void SessionManager::clearHistory() {
    if (m_history.isEmpty()) return;
    m_history.clear();
    emit historyChanged();
    saveHistory();
}

void SessionManager::loadHistory() {
    QFile f(m_historyPath);
    if (!f.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isArray()) return;

    m_history.clear();
    for (const auto &val : doc.array()) {
        QJsonObject obj = val.toObject();
        HistoryItem h;
        h.file.filePath = obj["path"].toString();
        h.file.fileName = obj["name"].toString();
        h.scene = static_cast<SceneType>(obj["scene"].toInt());
        h.timestamp = obj["ts"].toInteger();
        if (!h.file.filePath.isEmpty())
            m_history.push_back(h);
    }
}

void SessionManager::saveHistory() {
    QJsonArray arr;
    for (const auto &h : m_history) {
        QJsonObject obj;
        obj["path"]  = h.file.filePath;
        obj["name"]  = h.file.fileName;
        obj["scene"] = static_cast<int>(h.scene);
        obj["ts"]    = h.timestamp;
        arr.append(obj);
    }
    QFile f(m_historyPath);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(arr).toJson());
        f.close();
    }
}

bool SessionManager::addNetworkToLibrary(const QString &url) {
    for (const auto &lib : m_library) {
        if (lib.originalPath == url)
            return false;
    }

    QString id = QUuid::createUuid().toString();
    id.remove('{').remove('}');

    MediaLibraryItem item;
    item.id = id;
    item.originalPath = url;
    item.fileName = urlToDisplayName(url);
    item.fileSize = 0;
    item.dateAdded = QDateTime::currentSecsSinceEpoch();
    m_library.append(item);

    saveLibrary();
    emit libraryChanged();
    return true;
}

bool SessionManager::addToLibrary(const QString &sourcePath) {
    QFileInfo fi(sourcePath);
    if (!fi.exists()) return false;

    // Dedup by original path
    for (const auto &lib : m_library) {
        if (lib.originalPath == sourcePath)
            return false;
    }

    QString id = QUuid::createUuid().toString();
    id.remove('{').remove('}');
    QString destName = id + "_" + fi.fileName();
    QString destPath = m_libraryDir + destName;

    if (!QFile::copy(sourcePath, destPath))
        return false;

    MediaLibraryItem item;
    item.id = id;
    item.originalPath = sourcePath;
    item.libraryPath = destPath;
    item.fileName = fi.fileName();
    item.fileSize = fi.size();
    item.dateAdded = QDateTime::currentSecsSinceEpoch();
    m_library.append(item);

    saveLibrary();
    emit libraryChanged();
    return true;
}

void SessionManager::removeFromLibrary(int index) {
    if (index < 0 || index >= m_library.size()) return;

    // Delete the file from disk only if it's a local copy
    if (!m_library[index].libraryPath.isEmpty())
        QFile::remove(m_library[index].libraryPath);

    m_library.removeAt(index);
    saveLibrary();
    emit libraryChanged();
}

void SessionManager::loadLibrary() {
    QString path = m_libraryDir + "library.json";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isArray()) return;

    m_library.clear();
    for (const auto &val : doc.array()) {
        QJsonObject obj = val.toObject();
        MediaLibraryItem item;
        item.id = obj["id"].toString();
        item.originalPath = obj["originalPath"].toString();
        item.libraryPath = obj["libraryPath"].toString();
        item.fileName = obj["fileName"].toString();
        item.fileSize = obj["fileSize"].toInteger();
        item.dateAdded = obj["dateAdded"].toInteger();

        // Accept local files (must exist) or URL references (empty libraryPath)
        if (!item.id.isEmpty() && (item.libraryPath.isEmpty() || QFileInfo::exists(item.libraryPath)))
            m_library.append(item);
    }
}

void SessionManager::saveLibrary() {
    QJsonArray arr;
    for (const auto &item : m_library) {
        QJsonObject obj;
        obj["id"] = item.id;
        obj["originalPath"] = item.originalPath;
        obj["libraryPath"] = item.libraryPath;
        obj["fileName"] = item.fileName;
        obj["fileSize"] = item.fileSize;
        obj["dateAdded"] = item.dateAdded;
        arr.append(obj);
    }
    QFile f(m_libraryDir + "library.json");
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(arr).toJson());
        f.close();
    }
}
