#pragma once

#include "PacketQueue.h"
#include "FrameQueue.h"
#include <atomic>
#include <thread>
#include <string>
#include <mutex>
#include <condition_variable>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    bool open(const std::string& url);          // 支持文件/网络流
    void close();
    void cancel() { m_cancelled = true; }

    static int interruptCallback(void *ctx);
    void play();
    void pause();
    void seek(int64_t ptsMicroseconds);         // 跳转到微秒时间点
    bool isRunning() const { return m_running; }
    bool isPaused() const { return m_paused; }

    // 供 OpenGL / 渲染线程调用，获取最新一帧（非阻塞）
    bool getCurrentFrame(FramePtr& outFrame, int timeoutMs = 0);
    bool peekFrame(FramePtr& out) const;

    // 获取视频信息
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    AVPixelFormat getPixelFormat() const { return m_pixFmt; }
    int64_t getDuration() const;
    AVRational getStreamTimeBase() const;

    // 设置帧队列容量（供高倍速播放下调整）
    void setFrameQueueMaxSize(int size) { m_frameQueue.setMaxSize(size); }

private:
    void readLoop();          // 读取包线程
    void decodeLoop();        // 解码线程
    void pushFrame(AVFrame* frame);
    void tryPushFrame(AVFrame* frame);  // 非阻塞，用于 EOF drain 避免死锁

    // FFmpeg 相关
    AVFormatContext* m_formatCtx;
    AVCodecContext*  m_codecCtx;
    int m_videoStreamIndex;

    // 线程与同步
    std::thread m_readThread;
    std::thread m_decodeThread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_paused;
    std::atomic<bool> m_cancelled{false};
    std::mutex m_pauseMutex;
    std::condition_variable m_pauseCond;

    // Seek 控制
    std::mutex m_seekMutex;                    // 序列化 seek 操作
    std::atomic<bool> m_seekRequest;
    std::atomic<int64_t> m_seekTarget;       // 微秒
    std::atomic<bool> m_flushDecoder;        // 通知解码线程刷新解码器

    // 队列
    PacketQueue m_packetQueue;               // 包队列（读取→解码）
    FrameQueue  m_frameQueue;                // 帧队列（解码→渲染）

    // 视频参数
    std::atomic<int> m_width{0};
    std::atomic<int> m_height{0};
    AVPixelFormat m_pixFmt{AV_PIX_FMT_NONE};

    // 关闭互斥锁，防止 close() 并发调用
    std::mutex m_closeMutex;
    bool m_closed{false};

    std::atomic<bool> m_eofReached{false};

    // 禁用拷贝
    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;
};