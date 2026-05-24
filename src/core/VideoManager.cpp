#include "VideoManager.h"
#include "video/VideoDecoder.h"
#include "audio/AudioDecoder.h"
#include "audio/AudioRenderer.h"
#include <QDebug>
#include <mutex>
#include <chrono>

extern "C" {
#include <libavutil/avutil.h>
}

VideoManager::VideoManager()
    : m_decoder(std::make_unique<VideoDecoder>())
    , m_audioDecoder(std::make_unique<AudioDecoder>())
    , m_audioRenderer(std::make_unique<AudioRenderer>())
{
}

VideoManager::~VideoManager() {
    close();
}

// ── Open / Close ────────────────────────────────────────────────────────────

bool VideoManager::open(const std::string& url) {
    std::lock_guard<std::recursive_mutex> lock(m_openMutex);

    m_lastUrl = url;

    // 先清理旧状态
    close();

    if (!m_decoder->open(url))
        return false;

    m_audioStreamExists = m_audioDecoder->open(url);
    m_audioAvailable = false;

    if (m_audioStreamExists) {
        m_audioAvailable = m_audioRenderer->init(
            m_audioDecoder->getSampleRate(),
            m_audioDecoder->getChannels(),
            m_speed.load()
        );
        if (!m_audioAvailable) {
            qWarning() << "VideoManager: audio stream present but no output device — video-only mode";
        }
    }

    return true;
}

void VideoManager::close() {
    std::lock_guard<std::recursive_mutex> lock(m_openMutex);

    stopAudioFeedThread();
    m_audioDecoder->close();
    m_audioRenderer->stop();
    m_decoder->close();
    {
        std::lock_guard<std::mutex> frameLock(m_frameMutex);
        m_currentPts = 0;
        m_audioClockOffset = 0;
        m_lastFrame.reset();
    }
    m_audioAvailable = false;
    m_audioStreamExists = false;
    m_lastNewFrameTime = 0;
}

void VideoManager::cancelOpen() {
    std::lock_guard<std::recursive_mutex> lock(m_openMutex);
    m_decoder->cancel();
    if (m_audioDecoder) m_audioDecoder->cancel();
}

// ── Playback control ────────────────────────────────────────────────────────

void VideoManager::play() {
    // 直播流暂停后需重新打开（避免累积延迟）；open() 已启动解码器
    {
        std::string reopenUrl;
        bool needReopen = false;
        {
            std::lock_guard<std::recursive_mutex> lock(m_openMutex);
            if (m_decoder->isPaused() && isLiveStream() && !m_lastUrl.empty()) {
                needReopen = true;
                reopenUrl = m_lastUrl;
            }
        }
        if (needReopen) {
            close();
            if (!open(reopenUrl)) {
                qWarning() << "VideoManager: live stream reopen failed, entering stopped state";
                return;
            }
            // open() 已将解码器置为播放状态，直接进入音频管线
        }
    }

    if (!m_decoder->isRunning()) return;

    m_decoder->play();

    // 音频管线保护：避免在未初始化时启动
    if (m_audioAvailable && m_audioDecoder->isRunning()) {
        if (m_audioFeeding)
            return;
        std::lock_guard<std::mutex> lock(m_audioPipelineMutex);
        if (!m_audioFeeding)
            startAudioPipeline();
    } else if (m_audioStreamExists) {
        if (tryRecoverAudio())
            return;
    }
}

void VideoManager::pause() {
    if (!m_decoder->isRunning()) {
        // 解码器未运行（例如正在加载中），忽略暂停
        return;
    }
    m_decoder->pause();

    // 音频管线保护：确保在锁内操作
    {
        std::lock_guard<std::mutex> lock(m_audioPipelineMutex);
        if (m_audioFeeding)
            stopAudioPipeline();
    }
}

void VideoManager::togglePause() {
    if (m_decoder->isPaused())
        play();
    else
        pause();
}

// ── Seek ────────────────────────────────────────────────────────────────────

void VideoManager::seek(double ratio) {
    if (!m_decoder->isRunning()) return;

    int64_t dur = getDuration();
    if (dur <= 0) return;
    int64_t target = static_cast<int64_t>(ratio * dur);

    // 节流：如果两次 seek 间隔小于 50ms，直接跳过此操作
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    int64_t last = m_lastSeekTime.load();
    if (last > 0 && (now - last) < 50) {
        return;  // 真正跳过 seek 操作
    }
    m_lastSeekTime = now;

    bool wasPaused = m_decoder->isPaused();

    {
        std::lock_guard<std::mutex> frameLock(m_frameMutex);
        m_currentPts = target;
        m_lastFrame.reset();
        m_lastNewFrameTime = 0;
    }

    if (m_audioAvailable) {
        std::lock_guard<std::mutex> lock(m_audioPipelineMutex);
        if (!wasPaused) {
            stopAudioPipeline();
        }

        m_audioDecoder->setInterruptSeek(true);
        {
            struct Guard { AudioDecoder* d; ~Guard() { if (d) d->setInterruptSeek(false); } } guard{m_audioDecoder.get()};
            m_audioDecoder->seek(target);
        }

        if (!wasPaused) {
            startAudioPipeline();
        }
    }

    m_decoder->seek(target);
}

void VideoManager::seekForward(int64_t us) {
    int64_t dur = getDuration();
    if (dur <= 0) return;
    int64_t target = m_currentPts + us;
    if (target > dur) target = dur;
    seek(static_cast<double>(target) / dur);
}

void VideoManager::seekBackward(int64_t us) {
    int64_t dur = getDuration();
    if (dur <= 0) return;
    int64_t target = m_currentPts - us;
    if (target < 0) target = 0;
    seek(static_cast<double>(target) / dur);
}

// ── Speed ───────────────────────────────────────────────────────────────────

void VideoManager::setSpeed(double speed) {
    if (speed < 0.5) speed = 0.5;
    if (speed > 4.0) speed = 4.0;
    m_speed = speed;
}

void VideoManager::applySpeed(double speed) {
    m_speed = speed;

    // 根据播放速度调整帧队列容量，高倍速时增大缓冲
    if (speed >= 1.5) {
        m_decoder->setFrameQueueMaxSize(static_cast<int>(speed * 8));  // 2x→16, 4x→32
    } else {
        m_decoder->setFrameQueueMaxSize(5);
    }

    if (!m_audioAvailable || !m_audioFeeding) return;

    std::lock_guard<std::mutex> lock(m_audioPipelineMutex);

    if (!m_audioFeeding) return;

    int64_t savedPosition = m_audioRenderer->getCustomClock() + m_audioClockOffset;

    stopAudioPipeline();

    m_audioDecoder->seek(savedPosition);

    m_audioClockOffset = savedPosition;
    m_audioDecoder->start();
    m_audioRenderer->init(
        m_audioDecoder->getSampleRate(),
        m_audioDecoder->getChannels(),
        speed
    );
    m_audioRenderer->start();
    m_audioFeeding = true;
    m_audioFeedThread = std::thread(&VideoManager::audioFeedLoop, this);
}

// ── Volume ──────────────────────────────────────────────────────────────────

void VideoManager::setVolume(double volume) {
    if (m_audioRenderer)
        m_audioRenderer->setVolume(volume);
}

double VideoManager::getVolume() const {
    return m_audioRenderer ? m_audioRenderer->volume() : 1.0;
}

void VideoManager::setMuted(bool muted) {
    if (m_audioRenderer)
        m_audioRenderer->setMuted(muted);
}

bool VideoManager::isMuted() const {
    return m_audioRenderer ? m_audioRenderer->isMuted() : false;
}

// ── State queries ───────────────────────────────────────────────────────────

bool VideoManager::isRunning() const {
    return m_decoder && m_decoder->isRunning();
}

bool VideoManager::isPaused() const {
    return m_decoder && m_decoder->isPaused();
}

int64_t VideoManager::getCurrentPts() const {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    return m_currentPts;
}

double VideoManager::getProgress() const {
    int64_t dur = getDuration();
    if (dur <= 0) return 0.0;
    int64_t pts = m_currentPts.load();
    if (pts <= 0) return 0.0;
    return static_cast<double>(pts) / dur;
}

int64_t VideoManager::getDuration() const {
    return m_decoder ? m_decoder->getDuration() : 0;
}

int VideoManager::getWidth() const {
    return m_decoder ? m_decoder->getWidth() : 0;
}

int VideoManager::getHeight() const {
    return m_decoder ? m_decoder->getHeight() : 0;
}

bool VideoManager::isLiveStream() const {
    if (!m_decoder || !m_decoder->isRunning()) return false;
    return getDuration() <= 0;
}

// ── Audio feed ──────────────────────────────────────────────────────────────

void VideoManager::audioFeedLoop() {
    try {
        while (m_audioFeeding && m_audioDecoder && m_audioDecoder->isRunning()) {
            AVFrame* frame = nullptr;
            if (m_audioDecoder->popFrame(frame, 100)) {
                if (frame) {
                    try {
                        m_audioRenderer->write(frame);
                    } catch (...) {
                        av_frame_free(&frame);
                        throw;
                    }
                    av_frame_free(&frame);
                }
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "Audio feed thread exception:" << e.what();
    } catch (...) {
        qWarning() << "Audio feed thread unknown exception";
    }
    m_audioFeeding = false;
}

// ── Audio pipeline helpers ──────────────────────────────────────────────────

void VideoManager::startAudioPipeline() {
    m_audioClockOffset = m_currentPts.load();
    m_audioDecoder->start();
    m_audioRenderer->init(
        m_audioDecoder->getSampleRate(),
        m_audioDecoder->getChannels(),
        m_speed.load()
    );
    m_audioRenderer->start();
    m_audioFeeding = true;
    m_audioFeedThread = std::thread(&VideoManager::audioFeedLoop, this);
}

void VideoManager::stopAudioFeedThread() {
    if (m_audioFeeding) {
        m_audioFeeding = false;
        if (m_audioFeedThread.joinable())
            m_audioFeedThread.join();
    }
}

void VideoManager::stopAudioPipeline() {
    stopAudioFeedThread();
    m_audioDecoder->stop();
    m_audioRenderer->stop();
}

// ── Deferred audio start ────────────────────────────────────────────────────

bool VideoManager::tryRecoverAudio() {
    if (!m_audioStreamExists) return false;
    if (!AudioRenderer::hasOutputDevice()) return false;

    qDebug() << "VideoManager: attempting deferred audio start";

    stopAudioFeedThread();
    m_audioDecoder->stop();
    m_audioRenderer->stop();

    if (!m_audioRenderer->reinitialize()) {
        qWarning() << "VideoManager: audio recovery failed";
        return false;
    }

    m_audioDecoder->start();
    if (m_currentPts > 0)
        m_audioDecoder->seek(m_currentPts);

    m_audioRenderer->start();
    m_audioClockOffset = m_currentPts.load();
    m_audioAvailable = true;
    m_audioFeeding = true;
    m_audioFeedThread = std::thread(&VideoManager::audioFeedLoop, this);

    qDebug() << "VideoManager: audio started on newly available device";
    return true;
}

// ── Frame delivery ──────────────────────────────────────────────────────────

bool VideoManager::getCurrentFrame(FramePtr& outFrame, int timeoutMs) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    if (!m_decoder) return false;
    if (!m_decoder->isRunning()) {
        if (m_lastFrame) {
            outFrame = m_lastFrame;
            return true;
        }
        return false;
    }

    int64_t effectiveClock = -1;
    if (m_audioAvailable && m_audioFeeding) {
        int64_t customClock = m_audioRenderer->getCustomClock();
        if (customClock >= 0) {
            effectiveClock = customClock + m_audioClockOffset;
        }
    }

    if (effectiveClock >= 0) {
        // Post-seek guard: if the audio clock is far ahead of the visible
        // video PTS, the decoder is still catching up from the keyframe
        // before the seek target. Fall back to PTS-based throttling until
        // the video catches up.
        int64_t lastPts = m_currentPts.load(std::memory_order_relaxed);
        if (effectiveClock > lastPts + 300000) {
            effectiveClock = -1;
        }
    }

    if (effectiveClock >= 0) {
        FramePtr peeked;
        if (m_decoder->peekFrame(peeked) && peeked && peeked->pts != AV_NOPTS_VALUE) {
            int64_t framePts = av_rescale_q(
                peeked->pts,
                m_decoder->getStreamTimeBase(),
                AVRational{1, 1000000}
            );

            int64_t diff = framePts - effectiveClock;
            const int64_t SYNC_THRESHOLD = 50000;

            if (diff > SYNC_THRESHOLD) {
                if (m_lastFrame) {
                    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                    m_lastNewFrameTime = now;
                    outFrame = m_lastFrame;
                    return true;
                }
            } else if (diff < -SYNC_THRESHOLD) {
                int skipped = 0;
                while (m_decoder->getCurrentFrame(outFrame, 0)) {
                    if (!outFrame || !outFrame->data[0]) continue;
                    ++skipped;
                    int64_t pts = av_rescale_q(
                        outFrame->pts,
                        m_decoder->getStreamTimeBase(),
                        AVRational{1, 1000000}
                    );
                    if (pts >= effectiveClock - 10000) break;
                }
                if (skipped > 0 && outFrame && outFrame->data[0]) {
                    m_currentPts = av_rescale_q(
                        outFrame->pts,
                        m_decoder->getStreamTimeBase(),
                        AVRational{1, 1000000}
                    );
                    m_lastFrame = outFrame;
                    return true;
                }
            }
        }
    }

    bool got = m_decoder->getCurrentFrame(outFrame, timeoutMs);

    if (got && outFrame && outFrame->data[0]) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        m_lastNewFrameTime = now;
        m_frameSequence++;

        if (effectiveClock < 0) {
            if (m_lastFrame && m_lastFrame->pts != AV_NOPTS_VALUE && outFrame && outFrame->pts != AV_NOPTS_VALUE) {
                int64_t lastPts = av_rescale_q(
                    m_lastFrame->pts,
                    m_decoder->getStreamTimeBase(),
                    AVRational{1, 1000000}
                );
                int64_t currentPts = av_rescale_q(
                    outFrame->pts,
                    m_decoder->getStreamTimeBase(),
                    AVRational{1, 1000000}
                );
                int64_t ptsDiff = currentPts - lastPts;
                int64_t targetInterval = static_cast<int64_t>(1000000.0 / 30.0 / m_speed.load());
                if (ptsDiff < targetInterval) {
                    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                    m_lastNewFrameTime = now;
                    outFrame = m_lastFrame;
                    return true;
                }
            }
        }

        AVFrame* raw = outFrame.get();
        if (raw->pts != AV_NOPTS_VALUE) {
            m_currentPts = av_rescale_q(
                raw->pts,
                m_decoder->getStreamTimeBase(),
                AVRational{1, 1000000}
            );
        }
        m_lastFrame = outFrame;
        return true;
    }

    if (m_lastFrame) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        m_lastNewFrameTime = now;
        outFrame = m_lastFrame;
        return true;
    }

    return got;
}

bool VideoManager::isBuffering() const {
    if (!m_decoder || !m_decoder->isRunning()) return false;
    auto last = m_lastNewFrameTime.load();
    if (last == 0) return true;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return (now - last) > 500;
}