#include "AudioDecoder.h"
#include "video/PacketQueue.h"
#include <chrono>

AudioDecoder::AudioDecoder() {}

AudioDecoder::~AudioDecoder() {
    close();
}

bool AudioDecoder::open(AVCodecParameters* codecParams, AVRational timeBase) {
    std::lock_guard<std::mutex> lock(m_closeMutex);
    if (m_closed) m_closed = false;

    if (m_running) {
        m_running = false;
        m_cond.notify_all();
        if (m_decodeThread.joinable())
            m_decodeThread.join();
    }

    {
        std::lock_guard<std::mutex> fLock(m_mutex);
        while (!m_frames.empty()) {
            av_frame_free(&m_frames.front());
            m_frames.pop();
        }
    }

    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
    }

    m_packetQueue = nullptr;
    m_eofFlag = nullptr;
    m_sampleRate = 0;
    m_channels = 0;
    m_sampleFmt = AV_SAMPLE_FMT_NONE;

    if (!codecParams) return false;

    const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
    if (!codec) return false;

    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) return false;

    if (avcodec_parameters_to_context(m_codecCtx, codecParams) < 0) {
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
        return false;
    }

    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
        return false;
    }

    m_sampleRate = codecParams->sample_rate;
    m_channels = m_codecCtx->ch_layout.nb_channels;
    m_sampleFmt = static_cast<AVSampleFormat>(codecParams->format);
    m_timeBase = timeBase;

    return true;
}

void AudioDecoder::close() {
    std::lock_guard<std::mutex> lock(m_closeMutex);
    if (m_closed) return;
    m_closed = true;

    stop();

        {
            std::lock_guard<std::mutex> fLock(m_mutex);
            while (!m_frames.empty()) {
                av_frame_free(&m_frames.front());
                m_frames.pop();
            }
        }

    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
    }

    m_packetQueue = nullptr;
    m_eofFlag = nullptr;
    m_sampleRate = 0;
    m_channels = 0;
    m_sampleFmt = AV_SAMPLE_FMT_NONE;
}

void AudioDecoder::start(PacketQueue* packetQueue, std::atomic<bool>* eofFlag) {
    if (m_running) return;
    m_packetQueue = packetQueue;
    m_eofFlag = eofFlag;
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) return;
    m_decodeThread = std::thread(&AudioDecoder::decodeLoop, this);
}

void AudioDecoder::stop() {
    if (m_running) {
        m_running = false;
        m_cond.notify_all();
        if (m_decodeThread.joinable())
            m_decodeThread.join();
        m_packetQueue = nullptr;
        m_eofFlag = nullptr;
    }
}

void AudioDecoder::seek() {
    m_flushDecoder = true;
}

void AudioDecoder::clearFrames() {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_frames.empty()) {
        av_frame_free(&m_frames.front());
        m_frames.pop();
    }
}

bool AudioDecoder::popFrame(AVFrame*& out, int timeoutMs) {
    std::unique_lock<std::mutex> lock(m_mutex);

    if (timeoutMs > 0) {
        m_cond.wait_for(lock, std::chrono::milliseconds(timeoutMs),
            [this] { return !m_frames.empty() || !m_running; });
    } else if (timeoutMs < 0) {
        m_cond.wait(lock, [this] { return !m_frames.empty() || !m_running; });
    }

    if (m_frames.empty()) return false;

    out = m_frames.front();
    m_frames.pop();
    return true;
}

void AudioDecoder::decodeLoop() {
    AVPacket pkt{};
    AVFrame* frame = av_frame_alloc();
    if (!frame) return;
    bool drained = false;

    while (m_running) {
        if (m_flushDecoder) {
            avcodec_flush_buffers(m_codecCtx);
            clearFrames();
            m_flushDecoder = false;
            drained = false;
        }

        if (!m_packetQueue || !m_packetQueue->pop(pkt, 10)) {
            bool streamEnded = m_eofFlag && m_eofFlag->load();
            if (streamEnded && !drained) {
                avcodec_send_packet(m_codecCtx, nullptr);
                while (m_running) {
                    int ret = avcodec_receive_frame(m_codecCtx, frame);
                    if (ret < 0) break;
                    AVFrame* copy = av_frame_alloc();
                    if (copy) {
                        av_frame_ref(copy, frame);
                        {
                            std::unique_lock<std::mutex> lock(m_mutex);
                            m_frames.push(copy);
                        }
                        m_cond.notify_one();
                    }
                }
                drained = true;
            }
            continue;
        }
        drained = false;

        if (m_flushDecoder) {
            avcodec_flush_buffers(m_codecCtx);
            clearFrames();
            m_flushDecoder = false;
            drained = false;
        }

        if (avcodec_send_packet(m_codecCtx, &pkt) == 0) {
            while (m_running) {
                int ret = avcodec_receive_frame(m_codecCtx, frame);
                if (ret < 0) break;

                AVFrame* copy = av_frame_alloc();
                if (copy) {
                    av_frame_ref(copy, frame);
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_frames.push(copy);
                    }
                    m_cond.notify_one();

                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cond.wait_for(lock, std::chrono::milliseconds(50),
                        [this] { return m_frames.size() <= 12 || !m_running; });
                }
            }
        }
        av_packet_unref(&pkt);
    }

    av_frame_free(&frame);
}
