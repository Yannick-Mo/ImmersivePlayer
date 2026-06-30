#pragma once
#include <QString>

enum class SceneType {
    Concert,     // 演唱会 
    TechPlaza,   // 科技广场
    Cinema,      // 电影院
    Normal       // 普通播放
};

inline constexpr const char* SceneTypeName(SceneType t) {
    switch (t) {
        case SceneType::Concert:   return "演唱会";
        case SceneType::TechPlaza: return "科技广场";
        case SceneType::Cinema:    return "电影院";
        case SceneType::Normal:    return "普通播放";
        default:                   return "";
    }
}

struct FileItem {
    QString filePath;
    QString fileName;
};

struct HistoryItem {
    FileItem file;
    SceneType scene = SceneType::Normal;
    qint64 timestamp = 0;  // seconds since epoch
};

struct MediaLibraryItem {
    QString id;             // UUID
    QString originalPath;   // original file path (for dedup)
    QString libraryPath;    // path in app's library directory
    QString fileName;       // display name
    qint64 fileSize = 0;    // bytes
    qint64 dateAdded = 0;   // seconds since epoch
};

struct PlayerState {
    bool isPlaying    = false;
    double position   = 0.0;
    double duration   = 0.0;
    double volume     = 1.0;
    double speed      = 1.0;
    bool isMuted      = false;
    bool isFullscreen = false;
    bool waveformVisible = false;
    bool controlPanelVisible = false;
};

struct MediaInfo {
    QString filePath;
    QString fileName;
    int width  = 0;
    int height = 0;
    double duration = 0.0;
    double fps = 0.0;
    QString codec;
    SceneType scene = SceneType::Normal;
};
