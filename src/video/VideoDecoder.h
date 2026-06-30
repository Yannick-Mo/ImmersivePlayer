#pragma once

#include "PacketQueue.h"
#include "FrameQueue.h"
#include <atomic>
#include <thread>
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

    bool open(AVCodecParameters* codecParams, AVRational timeBase);
    void close();

    void start(PacketQueue* packetQueue, std::atomic<bool>* eofFlag);
    void stop();
    void pause();
    void resume();

    void seek();

    bool getCurrentFrame(FramePtr& outFrame, int timeoutMs = 0);
    bool peekFrame(FramePtr& out) const;

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    AVPixelFormat getPixelFormat() const { return m_pixFmt; }
    AVRational getTimeBase() const { return m_timeBase; }
    bool isRunning() const { return m_running; }
    bool isPaused() const { return m_paused; }

    void setFrameQueueMaxSize(int size) { m_frameQueue.setMaxSize(size); }

private:
    void decodeLoop();
    void pushFrame(AVFrame* frame);
    void tryPushFrame(AVFrame* frame);

    AVCodecContext* m_codecCtx = nullptr;
    PacketQueue* m_packetQueue = nullptr;
    std::atomic<bool>* m_eofFlag = nullptr;

    FrameQueue m_frameQueue;
    AVRational m_timeBase{0, 0};

    std::thread m_decodeThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_flushDecoder{false};

    std::mutex m_pauseMutex;
    std::condition_variable m_pauseCond;

    std::atomic<int> m_width{0};
    std::atomic<int> m_height{0};
    AVPixelFormat m_pixFmt{AV_PIX_FMT_NONE};

    std::mutex m_closeMutex;
    bool m_closed{false};

    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;
};
