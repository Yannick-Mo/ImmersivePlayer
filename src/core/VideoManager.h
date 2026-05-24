#pragma once

#include "video/FrameQueue.h"
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>

class VideoDecoder;
class AudioDecoder;
class AudioRenderer;

class VideoManager {
public:
    VideoManager();
    ~VideoManager();

    VideoManager(const VideoManager&) = delete;
    VideoManager& operator=(const VideoManager&) = delete;
    VideoManager(VideoManager&&) = delete;
    VideoManager& operator=(VideoManager&&) = delete;

    bool open(const std::string& url);
    void close();
    void play();
    void pause();
    void togglePause();
    void cancelOpen();

    void seek(double ratio);
    void seekForward(int64_t us);
    void seekBackward(int64_t us);

    void setSpeed(double speed);
    void applySpeed(double speed);
    double getSpeed() const { return m_speed; }

    void setVolume(double volume);
    double getVolume() const;
    void setMuted(bool muted);
    bool isMuted() const;

    bool isRunning() const;
    bool isPaused() const;
    double getProgress() const;
    int64_t getCurrentPts() const;
    int64_t getDuration() const;
    int getWidth() const;
    int getHeight() const;
    bool isLiveStream() const;

    bool isBuffering() const;

    void setEffect(int effect) { m_effect = effect; }
    int getEffect() const { return m_effect; }

    AudioRenderer* getAudioRenderer() const { return m_audioRenderer.get(); }

    bool getCurrentFrame(FramePtr& outFrame, int timeoutMs = 0);

private:
    void audioFeedLoop();
    bool tryRecoverAudio();
    void startAudioPipeline();
    void stopAudioPipeline();
    void stopAudioFeedThread();

    std::unique_ptr<VideoDecoder> m_decoder;
    std::unique_ptr<AudioDecoder> m_audioDecoder;
    std::unique_ptr<AudioRenderer> m_audioRenderer;
    std::atomic<double> m_speed{1.0};
    std::atomic<int> m_effect{0};

    std::recursive_mutex m_openMutex;
    std::string m_lastUrl;

    std::atomic<int64_t> m_currentPts{0};
    std::atomic<int64_t> m_audioClockOffset{0};
    FramePtr m_lastFrame;
    mutable std::mutex m_frameMutex;

    std::thread m_audioFeedThread;
    std::atomic<bool> m_audioFeeding{false};
    std::atomic<bool> m_audioAvailable{false};

    // If true, the file has an audio stream but no output device was available
    // at init time. Allows deferred audio start when a device appears.
    std::atomic<bool> m_audioStreamExists{false};

    // Seek 节流：防止快速连续拖动导致线程乒乓
    std::mutex m_seekMutex;
    std::atomic<int64_t> m_lastSeekTime{0};  // ms

    // Audio pipeline 状态防竞态
    std::mutex m_audioPipelineMutex;

    // 缓冲检测
    mutable std::atomic<int64_t> m_lastNewFrameTime{0};
    mutable std::atomic<uint64_t> m_frameSequence{0};
};