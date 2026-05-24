#pragma once

#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>

extern "C" {
#include <libavutil/frame.h>
}

struct FrameDeleter {
    void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};
using FramePtr = std::shared_ptr<AVFrame>;

class FrameQueue {
public:
    FrameQueue(int maxSize = 5);
    ~FrameQueue();

    void push(FramePtr frame);
    bool tryPush(FramePtr frame);  // 非阻塞：队列满时立即返回 false
    bool peek(FramePtr& out) const;
    bool pop(FramePtr& out, int timeoutMs = 0);
    void clear();
    void stop();
    void start();
    bool isStopped() const { return m_stopped; }
    size_t size() const;

    // 动态调整最大容量（高倍速时增大，避免解码线程阻塞）
    void setMaxSize(int size);

private:
    std::queue<FramePtr> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    std::atomic<bool> m_stopped;
    std::atomic<int> m_maxSize;
};