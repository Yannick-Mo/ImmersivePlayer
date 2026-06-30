#include "VideoDecoder.h"

VideoDecoder::VideoDecoder() {}

VideoDecoder::~VideoDecoder() {
    close();
}

bool VideoDecoder::open(AVCodecParameters* codecParams, AVRational timeBase) {
    std::lock_guard<std::mutex> lock(m_closeMutex);
    if (m_closed) m_closed = false;

    if (m_running) {
        m_running = false;
        m_frameQueue.stop();
        m_pauseCond.notify_all();
        if (m_decodeThread.joinable())
            m_decodeThread.join();
    }

    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
    }

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

    m_width.store(codecParams->width);
    m_height.store(codecParams->height);
    m_pixFmt = static_cast<AVPixelFormat>(codecParams->format);
    m_timeBase = timeBase;

    return true;
}

void VideoDecoder::close() {
    std::lock_guard<std::mutex> lock(m_closeMutex);
    if (m_closed) return;
    m_closed = true;

    m_running = false;
    m_paused = false;
    m_frameQueue.stop();
    m_pauseCond.notify_all();

    if (m_decodeThread.joinable())
        m_decodeThread.join();

    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
    }

    m_packetQueue = nullptr;
    m_width.store(0);
    m_height.store(0);
    m_pixFmt = AV_PIX_FMT_NONE;
}

void VideoDecoder::start(PacketQueue* packetQueue, std::atomic<bool>* eofFlag) {
    if (m_running) return;
    m_packetQueue = packetQueue;
    m_eofFlag = eofFlag;
    m_packetQueue->start();
    m_frameQueue.start();
    m_frameQueue.clear();
    m_running = true;
    m_paused = false;
    m_flushDecoder = false;
    m_decodeThread = std::thread(&VideoDecoder::decodeLoop, this);
}

void VideoDecoder::stop() {
    if (m_running) {
        m_running = false;
        m_frameQueue.stop();
        m_pauseCond.notify_all();
        if (m_decodeThread.joinable())
            m_decodeThread.join();
        m_packetQueue = nullptr;
    }
}

void VideoDecoder::pause() {
    if (!m_running) return;
    if (!m_paused) {
        std::lock_guard<std::mutex> lock(m_pauseMutex);
        m_paused = true;
    }
}

void VideoDecoder::resume() {
    if (m_paused) {
        {
            std::lock_guard<std::mutex> lock(m_pauseMutex);
            m_paused = false;
        }
        m_pauseCond.notify_all();
    }
}

void VideoDecoder::seek() {
    m_flushDecoder = true;
    m_pauseCond.notify_all();
}

bool VideoDecoder::getCurrentFrame(FramePtr& outFrame, int timeoutMs) {
    return m_frameQueue.pop(outFrame, timeoutMs);
}

bool VideoDecoder::peekFrame(FramePtr& out) const {
    return m_frameQueue.peek(out);
}

void VideoDecoder::decodeLoop() {
    AVFrame* frame = av_frame_alloc();
    AVPacket pkt{};
    if (!frame) return;

    while (m_running) {
        if (m_paused) {
            std::unique_lock<std::mutex> lock(m_pauseMutex);
            m_pauseCond.wait(lock, [this] {
                return !m_paused || !m_running || m_flushDecoder;
            });
            if (!m_running) break;
        }

        if (m_flushDecoder) {
            avcodec_flush_buffers(m_codecCtx);
            m_frameQueue.clear();
            m_flushDecoder = false;
        }

        if (!m_packetQueue || !m_packetQueue->pop(pkt, 10)) {
            bool streamEnded = m_eofFlag && m_eofFlag->load();
            if (streamEnded && !m_flushDecoder) {
                if (avcodec_send_packet(m_codecCtx, nullptr) == 0) {
                    while (m_running) {
                        int ret = avcodec_receive_frame(m_codecCtx, frame);
                        if (ret < 0) break;
                        tryPushFrame(frame);
                    }
                }
            }
            continue;
        }

        if (m_flushDecoder) {
            avcodec_flush_buffers(m_codecCtx);
            m_frameQueue.clear();
            m_flushDecoder = false;
        }

        int ret = avcodec_send_packet(m_codecCtx, &pkt);
        if (ret == AVERROR(EAGAIN)) {
            while (m_running && !m_flushDecoder) {
                ret = avcodec_receive_frame(m_codecCtx, frame);
                if (ret == AVERROR(EAGAIN) || ret < 0) break;
                pushFrame(frame);
            }
            ret = avcodec_send_packet(m_codecCtx, &pkt);
        }
        if (ret == 0) {
            while (m_running && !m_flushDecoder) {
                ret = avcodec_receive_frame(m_codecCtx, frame);
                if (ret == AVERROR(EAGAIN)) break;
                if (ret < 0) break;
                pushFrame(frame);
            }
        }
        av_packet_unref(&pkt);
    }

    av_frame_free(&frame);
}

void VideoDecoder::pushFrame(AVFrame* frame) {
    if (m_frameQueue.isStopped()) return;
    AVFrame* copy = av_frame_alloc();
    if (!copy) return;
    av_frame_ref(copy, frame);
    FramePtr ptr(copy, FrameDeleter());
    m_frameQueue.push(ptr);
}

void VideoDecoder::tryPushFrame(AVFrame* frame) {
    if (m_frameQueue.isStopped()) return;
    AVFrame* copy = av_frame_alloc();
    if (!copy) return;
    av_frame_ref(copy, frame);
    FramePtr ptr(copy, FrameDeleter());
    m_frameQueue.tryPush(ptr);
}
