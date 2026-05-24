#include "VideoDecoder.h"
#include <chrono>

VideoDecoder::VideoDecoder()
    : m_formatCtx(nullptr)
    , m_codecCtx(nullptr)
    , m_videoStreamIndex(-1)
    , m_running(false)
    , m_paused(false)
    , m_seekRequest(false)
    , m_seekTarget(0)
    , m_flushDecoder(false)
{
}

VideoDecoder::~VideoDecoder() {
    close();
}

bool VideoDecoder::open(const std::string& url) {
    // 重置取消标志，允许重新打开
    m_cancelled = false;

    // 1. 如果已被取消，直接返回失败
    if (m_cancelled) return false;

    // 2. 清理旧状态（加锁防并发）
    {
        std::lock_guard<std::mutex> lock(m_closeMutex);
        if (m_closed) {
            // 已被 close() 清理过，重置标志后重新初始化
            m_closed = false;
        }
    }

    // 3. 先停止旧的运行状态
    if (m_running) {
        m_running = false;
        m_packetQueue.stop();
        m_frameQueue.stop();
        m_pauseCond.notify_all();
        if (m_readThread.joinable())
            m_readThread.join();
        if (m_decodeThread.joinable())
            m_decodeThread.join();
    }

    // 4. 释放旧的 FFmpeg 资源
    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
    }
    if (m_formatCtx) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
    }

    // 5. 再次检查取消标志（可能在释放资源期间被取消）
    if (m_cancelled) return false;

    // 6. 打开文件/网络流
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

    // 7. 查找视频流
    m_videoStreamIndex = -1;
    for (unsigned i = 0; i < m_formatCtx->nb_streams; ++i) {
        if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStreamIndex = i;
            break;
        }
    }
    if (m_videoStreamIndex == -1) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }

    AVCodecParameters* codecPar = m_formatCtx->streams[m_videoStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }
    if (avcodec_parameters_to_context(m_codecCtx, codecPar) < 0) {
        avcodec_free_context(&m_codecCtx);
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }
    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        avcodec_free_context(&m_codecCtx);
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }

    m_width.store(codecPar->width);
    m_height.store(codecPar->height);
    m_pixFmt = static_cast<AVPixelFormat>(codecPar->format);

    // 8. 初始化队列
    m_packetQueue.start();
    m_packetQueue.clear();
    m_frameQueue.start();
    m_frameQueue.clear();

    // 9. 重置状态（注意：不重置 m_paused，允许用户在加载期间暂停）
    {
        std::lock_guard<std::mutex> lock(m_closeMutex);
        m_closed = false;
    }
    m_running = true;
    m_paused = false;
    m_seekRequest = false;
    m_seekTarget = 0;
    m_flushDecoder = false;
    m_eofReached = false;

    // 10. 启动工作线程前终检取消标志
    if (m_cancelled) {
        m_running = false;
        m_packetQueue.stop();
        m_frameQueue.stop();
        avcodec_free_context(&m_codecCtx);
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }

    m_readThread = std::thread(&VideoDecoder::readLoop, this);
    m_decodeThread = std::thread(&VideoDecoder::decodeLoop, this);

    return true;
}

void VideoDecoder::close() {
    std::lock_guard<std::mutex> lock(m_closeMutex);
    if (m_closed) return;
    m_closed = true;

    // 先设置取消标志，中断任何正在进行的 I/O
    m_cancelled.store(true, std::memory_order_release);

    // 停止运行循环
    m_running.store(false, std::memory_order_release);
    m_paused.store(false, std::memory_order_release);

    // 停止队列，唤醒所有等待线程
    m_packetQueue.stop();
    m_frameQueue.stop();
    m_pauseCond.notify_all();

    // 等待线程结束（必须在持有 closeMutex 的情况下 join，防止竞态）
    if (m_readThread.joinable())
        m_readThread.join();
    if (m_decodeThread.joinable())
        m_decodeThread.join();

    // 释放 FFmpeg 资源
    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
    }
    if (m_formatCtx) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
    }

    m_videoStreamIndex = -1;
    m_width.store(0);
    m_height.store(0);
    m_pixFmt = AV_PIX_FMT_NONE;
}

void VideoDecoder::play() {
    if (m_paused.load(std::memory_order_acquire)) {
        {
            std::lock_guard<std::mutex> lock(m_pauseMutex);
            m_paused.store(false, std::memory_order_release);
        }
        m_pauseCond.notify_all();
    }
}

void VideoDecoder::pause() {
    if (!m_running.load(std::memory_order_acquire)) return;
    if (!m_paused.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(m_pauseMutex);
        m_paused.store(true, std::memory_order_release);
    }
}

void VideoDecoder::seek(int64_t ptsMicroseconds) {
    if (!m_formatCtx || m_videoStreamIndex < 0) return;
    if (!m_running) return;

    // 序列化 seek 操作，防止多个 seek 同时排队
    std::lock_guard<std::mutex> seekLock(m_seekMutex);

    m_seekTarget = ptsMicroseconds;
    m_seekRequest = true;
    m_flushDecoder = true;

    // 清空队列（必须在 seekMutex 持有下，防止与 readLoop 竞争）
    m_packetQueue.clear();
    m_frameQueue.clear();

    // 唤醒等待暂停的线程
    m_pauseCond.notify_all();
}

bool VideoDecoder::getCurrentFrame(FramePtr& outFrame, int timeoutMs) {
    return m_frameQueue.pop(outFrame, timeoutMs);
}

bool VideoDecoder::peekFrame(FramePtr& out) const {
    return m_frameQueue.peek(out);
}

// ----------------------- 读取线程 -----------------------
void VideoDecoder::readLoop() {
    AVPacket pkt{};
    while (m_running.load(std::memory_order_acquire)) {
        // 处理暂停
        if (m_paused.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lock(m_pauseMutex);
            m_pauseCond.wait(lock, [this] { return !m_paused.load(std::memory_order_acquire) || !m_running.load(std::memory_order_acquire) || m_seekRequest.load(std::memory_order_acquire); });
            if (!m_running.load(std::memory_order_acquire)) break;
        }

        // 处理跳转请求
        if (m_seekRequest) {
            std::lock_guard<std::mutex> seekLock(m_seekMutex);

            if (!m_seekRequest || !m_running) {
                continue;
            }

            int64_t target = m_seekTarget;
            AVStream* stream = m_formatCtx->streams[m_videoStreamIndex];
            int64_t seekStream = av_rescale_q(target, AV_TIME_BASE_Q, stream->time_base);

            av_seek_frame(m_formatCtx, m_videoStreamIndex, seekStream, AVSEEK_FLAG_BACKWARD);

            // 清空包队列（seek 后旧包已无效）
            m_packetQueue.clear();
            m_eofReached = false;
            m_seekRequest = false;
            // m_flushDecoder 保持 true，让解码线程刷新
            continue;
        }

        // 读取一个包
        int ret = av_read_frame(m_formatCtx, &pkt);
        if (ret < 0) {
            if (!m_running) break;
            // EOF 或暂时错误，短暂休眠避免忙等
            if (ret == AVERROR_EOF) {
                m_eofReached = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (pkt.stream_index == m_videoStreamIndex) {
            m_packetQueue.push(&pkt);
        }
        av_packet_unref(&pkt);
    }
}

// ----------------------- 解码线程 -----------------------
void VideoDecoder::decodeLoop() {
    AVFrame* frame = av_frame_alloc();
    AVPacket pkt{};
    if (!frame) return;

    while (m_running.load(std::memory_order_acquire)) {
        // 处理暂停
        if (m_paused.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lock(m_pauseMutex);
            m_pauseCond.wait(lock, [this] { return !m_paused.load(std::memory_order_acquire) || !m_running.load(std::memory_order_acquire) || m_seekRequest.load(std::memory_order_acquire) || m_flushDecoder.load(std::memory_order_acquire); });
            if (!m_running.load(std::memory_order_acquire)) break;
        }

        // 处理解码器刷新（seek 后必须刷新内部缓冲区）
        if (m_flushDecoder) {
            avcodec_flush_buffers(m_codecCtx);
            m_frameQueue.clear();
            m_flushDecoder = false;
        }

        // 从包队列取包（10ms 超时，可响应停止信号）
        if (!m_packetQueue.pop(pkt, 10)) {
            // EOF reached: drain remaining frames from decoder
            if (m_eofReached && !m_flushDecoder) {
                if (avcodec_send_packet(m_codecCtx, nullptr) == 0) {
                    while (m_running) {
                        int ret = avcodec_receive_frame(m_codecCtx, frame);
                        if (ret < 0) break;
                        tryPushFrame(frame);
                    }
                }
                m_eofReached = false;
            }
            continue;
        }

        // 发送包给解码器；EAGAIN 时先排空解码器再重试，避免丢包
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

int64_t VideoDecoder::getDuration() const {
    if (!m_formatCtx) return 0;
    return m_formatCtx->duration;
}

AVRational VideoDecoder::getStreamTimeBase() const {
    if (!m_formatCtx || m_videoStreamIndex < 0)
        return { 0, 0 };
    return m_formatCtx->streams[m_videoStreamIndex]->time_base;
}

// ----------------------- 帧输出辅助 -----------------------
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
    m_frameQueue.tryPush(ptr);  // 队列满时静默丢弃，避免死锁
}

int VideoDecoder::interruptCallback(void *ctx) {
    auto *decoder = static_cast<VideoDecoder*>(ctx);
    return (decoder->m_cancelled || decoder->m_seekRequest) ? 1 : 0;
}