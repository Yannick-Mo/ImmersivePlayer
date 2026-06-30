#include "VideoManager.h"
#include "Demuxer.h"
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
    : m_demuxer(std::make_unique<Demuxer>())
    , m_decoder(std::make_unique<VideoDecoder>())
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
    close();

    if (!m_demuxer->open(url))
        return false;

    m_decoder->open(
        m_demuxer->getVideoCodecParameters(),
        m_demuxer->getVideoTimeBase()
    );

    m_audioStreamExists = (m_demuxer->getAudioStreamIndex() >= 0);
    m_audioAvailable = false;

    if (m_audioStreamExists) {
        if (m_audioDecoder->open(
                m_demuxer->getAudioCodecParameters(),
                m_demuxer->getAudioTimeBase()))
        {
            m_audioAvailable = m_audioRenderer->init(
                m_audioDecoder->getSampleRate(),
                m_audioDecoder->getChannels(),
                m_speed.load()
            );
            if (!m_audioAvailable) {
                qWarning() << "VideoManager: audio stream present but no output device — video-only mode";
            }
        }
    }

    m_decoder->start(m_demuxer->getVideoPacketQueue(), &m_demuxer->m_eofReached);

    if (m_audioAvailable) {
        m_audioDecoder->start(m_demuxer->getAudioPacketQueue(), &m_demuxer->m_eofReached);
    }

    m_demuxer->start();

    return true;
}

void VideoManager::close() {
    std::lock_guard<std::recursive_mutex> lock(m_openMutex);

    stopAudioFeedThread();
    m_demuxer->stop();
    m_audioDecoder->close();
    m_audioRenderer->stop();
    m_decoder->close();
    m_demuxer->close();
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
    m_demuxer->cancel();
}

// ── Playback control ────────────────────────────────────────────────────────

void VideoManager::play() {
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
        }
    }

    if (!m_decoder->isRunning()) return;

    m_decoder->resume();

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
    if (!m_decoder->isRunning()) return;
    m_decoder->pause();

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

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    int64_t last = m_lastSeekTime.load();
    if (last > 0 && (now - last) < 50) return;
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

        m_demuxer->seek(target);

        m_decoder->seek();
        m_audioDecoder->seek();

        if (!wasPaused) {
            startAudioPipeline();
        }
    } else {
        m_demuxer->seek(target);
        m_decoder->seek();
    }
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

    if (speed >= 1.5) {
        m_decoder->setFrameQueueMaxSize(static_cast<int>(speed * 8));
    } else {
        m_decoder->setFrameQueueMaxSize(5);
    }

    if (!m_audioAvailable || !m_audioFeeding) return;

    std::lock_guard<std::mutex> lock(m_audioPipelineMutex);
    if (!m_audioFeeding) return;

    int64_t savedPosition = m_audioRenderer->getCustomClock() + m_audioClockOffset;

    stopAudioFeedThread();
    m_audioDecoder->stop();
    m_audioDecoder->clearFrames();
    m_audioRenderer->stop();

    m_audioClockOffset = savedPosition;

    m_demuxer->getAudioPacketQueue()->clear();

    m_audioDecoder->start(m_demuxer->getAudioPacketQueue(), &m_demuxer->m_eofReached);
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
    return m_demuxer ? m_demuxer->getDuration() : 0;
}

int VideoManager::getWidth() const {
    return m_decoder ? m_decoder->getWidth() : 0;
}

int VideoManager::getHeight() const {
    return m_decoder ? m_decoder->getHeight() : 0;
}

bool VideoManager::isLiveStream() const {
    if (!m_demuxer || !m_demuxer->isRunning()) return false;
    return m_demuxer->getDuration() <= 0;
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
    m_audioDecoder->start(m_demuxer->getAudioPacketQueue(), &m_demuxer->m_eofReached);
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

    m_demuxer->getAudioPacketQueue()->clear();

    m_audioDecoder->start(m_demuxer->getAudioPacketQueue(), &m_demuxer->m_eofReached);

    if (!m_audioRenderer->reinitialize()) {
        qWarning() << "VideoManager: audio recovery failed";
        return false;
    }

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
                m_decoder->getTimeBase(),
                AVRational{1, 1000000}
            );

            int64_t diff = framePts - effectiveClock;
            const int64_t SYNC_THRESHOLD = 20000;

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
                        m_decoder->getTimeBase(),
                        AVRational{1, 1000000}
                    );
                    if (pts >= effectiveClock - 10000) break;
                }
                if (skipped > 0 && outFrame && outFrame->data[0]) {
                    m_currentPts = av_rescale_q(
                        outFrame->pts,
                        m_decoder->getTimeBase(),
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
                    m_decoder->getTimeBase(),
                    AVRational{1, 1000000}
                );
                int64_t currentPts = av_rescale_q(
                    outFrame->pts,
                    m_decoder->getTimeBase(),
                    AVRational{1, 1000000}
                );
                int64_t ptsDiff = currentPts - lastPts;
                int64_t targetInterval = static_cast<int64_t>(1000000.0 / 50.0 / m_speed.load());
                if (ptsDiff < targetInterval) {
                    auto t = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                    m_lastNewFrameTime = t;
                    outFrame = m_lastFrame;
                    return true;
                }
            }
        }

        AVFrame* raw = outFrame.get();
        if (raw->pts != AV_NOPTS_VALUE) {
            m_currentPts = av_rescale_q(
                raw->pts,
                m_decoder->getTimeBase(),
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
