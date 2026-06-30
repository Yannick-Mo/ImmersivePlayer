#include "Demuxer.h"
#include <chrono>

Demuxer::Demuxer() {}

Demuxer::~Demuxer() {
    close();
}

bool Demuxer::open(const std::string& url) {
    close();
    m_cancelled = false;

    m_formatCtx = avformat_alloc_context();
    if (!m_formatCtx) return false;

    m_formatCtx->interrupt_callback.callback = interruptCallback;
    m_formatCtx->interrupt_callback.opaque = this;

    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "timeout", "5000000", 0);
    av_dict_set(&opts, "listen_timeout", "5000000", 0);
    av_dict_set(&opts, "stimeout", "5000000", 0);
    av_dict_set(&opts, "user_agent", "ImmersivePlayer/1.0", 0);
    if (avformat_open_input(&m_formatCtx, url.c_str(), nullptr, &opts) < 0) {
        av_dict_free(&opts);
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }
    av_dict_free(&opts);

    if (m_cancelled) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }

    if (avformat_find_stream_info(m_formatCtx, nullptr) < 0) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }
    if (m_cancelled) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }

    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    for (unsigned i = 0; i < m_formatCtx->nb_streams; ++i) {
        auto type = m_formatCtx->streams[i]->codecpar->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO && m_videoStreamIndex < 0)
            m_videoStreamIndex = i;
        else if (type == AVMEDIA_TYPE_AUDIO && m_audioStreamIndex < 0)
            m_audioStreamIndex = i;
    }
    if (m_videoStreamIndex < 0) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }

    m_closed = false;
    return true;
}

void Demuxer::close() {
    std::lock_guard<std::mutex> lock(m_closeMutex);
    if (m_closed) return;
    m_closed = true;

    m_cancelled = true;
    stop();

    if (m_formatCtx) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
    }

    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_eofReached = false;
    m_seekRequest = false;
}

void Demuxer::start() {
    if (m_running) return;
    m_running = true;
    m_eofReached = false;
    m_videoPacketQueue.start();
    m_audioPacketQueue.start();
    m_demuxThread = std::thread(&Demuxer::demuxLoop, this);
}

void Demuxer::stop() {
    if (m_running) {
        m_running = false;
        m_cancelled = true;
        m_videoPacketQueue.stop();
        m_audioPacketQueue.stop();
        m_seekDone.notify_all();
        if (m_demuxThread.joinable())
            m_demuxThread.join();
    }
}

void Demuxer::cancel() {
    m_cancelled = true;
}

void Demuxer::seek(int64_t ptsMicroseconds) {
    if (!m_formatCtx) return;
    if (isLiveStream()) return;

    {
        std::lock_guard<std::mutex> lock(m_seekMutex);
        if (m_seekRequest) return;

        m_videoPacketQueue.clear();
        m_audioPacketQueue.clear();

        m_seekTarget = ptsMicroseconds;
        m_eofReached = false;
        m_seekRequest = true;
    }

    std::unique_lock<std::mutex> lock(m_seekMutex);
    m_seekDone.wait(lock, [this] {
        return !m_seekRequest || !m_running;
    });
}

AVCodecParameters* Demuxer::getVideoCodecParameters() const {
    if (!m_formatCtx || m_videoStreamIndex < 0) return nullptr;
    return m_formatCtx->streams[m_videoStreamIndex]->codecpar;
}

AVCodecParameters* Demuxer::getAudioCodecParameters() const {
    if (!m_formatCtx || m_audioStreamIndex < 0) return nullptr;
    return m_formatCtx->streams[m_audioStreamIndex]->codecpar;
}

AVRational Demuxer::getVideoTimeBase() const {
    if (!m_formatCtx || m_videoStreamIndex < 0)
        return {0, 0};
    return m_formatCtx->streams[m_videoStreamIndex]->time_base;
}

AVRational Demuxer::getAudioTimeBase() const {
    if (!m_formatCtx || m_audioStreamIndex < 0)
        return {0, 0};
    return m_formatCtx->streams[m_audioStreamIndex]->time_base;
}

int64_t Demuxer::getDuration() const {
    if (!m_formatCtx) return 0;
    return m_formatCtx->duration;
}

bool Demuxer::isLiveStream() const {
    return m_formatCtx && m_formatCtx->duration <= 0;
}

void Demuxer::demuxLoop() {
    AVPacket pkt{};

    while (m_running) {
        if (m_cancelled) break;

        if (m_seekRequest) {
            std::lock_guard<std::mutex> lock(m_seekMutex);
            if (!m_seekRequest || !m_running) {
                continue;
            }

            int64_t target = m_seekTarget;
            avformat_seek_file(m_formatCtx, -1, INT64_MIN, target, INT64_MAX, AVSEEK_FLAG_BACKWARD);

            m_videoPacketQueue.clear();
            m_audioPacketQueue.clear();
            m_eofReached = false;

            m_seekRequest = false;
            m_seekDone.notify_all();
            continue;
        }

        int ret = av_read_frame(m_formatCtx, &pkt);
        if (ret < 0) {
            if (!m_running || m_cancelled) break;
            if (ret == AVERROR_EOF) {
                m_eofReached = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            if (m_cancelled) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (pkt.stream_index == m_videoStreamIndex) {
            m_videoPacketQueue.push(&pkt);
        } else if (pkt.stream_index == m_audioStreamIndex) {
            m_audioPacketQueue.push(&pkt);
        }
        av_packet_unref(&pkt);
    }

    av_packet_unref(&pkt);
}

int Demuxer::interruptCallback(void* ctx) {
    auto* d = static_cast<Demuxer*>(ctx);
    return (d->m_cancelled || d->m_seekRequest) ? 1 : 0;
}
