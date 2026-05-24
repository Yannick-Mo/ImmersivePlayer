#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class PacketQueue {
public:
    // 允许指定最大包数量，默认50
    explicit PacketQueue(int maxSize = 50);
    ~PacketQueue();

    // 入队（内部拷贝，阻塞直到有空间或队列被停止）
    void push(AVPacket* pkt);

    // 出队（timeoutMs > 0: 限时等待; ==0: 非阻塞; <0: 永久阻塞）
    // 返回 true 表示成功取出一个包，调用者负责 av_packet_unref
    bool pop(AVPacket& pkt, int timeoutMs = 0);

    // 清空队列，唤醒所有等待的 push/pop 线程
    void clear();

    // 停止队列，唤醒所有等待线程，后续 push/pop 将失败
    void stop();

    // 重置停止状态，允许队列重新工作
    void start();

    bool isStopped() const { return m_stopped; }
    size_t size() const;

private:
    std::queue<AVPacket> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    std::atomic<bool> m_stopped;
    int m_maxSize;
};