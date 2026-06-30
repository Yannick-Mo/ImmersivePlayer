#pragma once

#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>

extern "C" {
#include <libavutil/frame.h>
}

// 当 shared_ptr 引用计数归零时，自动调用 av_frame_free() 而非默认的 delete，
// 确保 FFmpeg 分配的内存由 FFmpeg 自己的 API 正确释放，避免跨库 delete 导致的未定义行为。
struct FrameDeleter {
    void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};

// FramePtr 是带 FrameDeleter 的 shared_ptr，自动管理 AVFrame 生命周期
using FramePtr = std::shared_ptr<AVFrame>;

// ============================================================================
// FrameQueue — 线程安全的视频帧缓冲队列
// ============================================================================
// 采用生产者-消费者模式，在解码线程（生产者）与渲染线程（消费者）之间传递 AVFrame。
// 使用 std::shared_ptr 共享帧数据，支持多消费者同时持有同一帧而不发生 use-after-free。
// 提供阻塞/非阻塞两种入队方式，以及带超时的出队操作。
// ============================================================================
class FrameQueue {
public:

    explicit FrameQueue(int maxSize = 5);

    ~FrameQueue();

    // -------------------------------------------------------------------------
    // 入队操作（生产者侧）
    // -------------------------------------------------------------------------

    // 阻塞入队：若队列已满，阻塞当前线程直至有空位或队列被 stop()
    // 适用于解码线程——必须确保每一帧都入队，宁可等待也不能丢帧
    void push(FramePtr frame);

    // 非阻塞入队：队列满时立即返回 false，不阻塞
    // 适用于高倍速等场景，避免因队列满导致解码线程卡死，允许主动丢弃帧
    // 返回值：true 成功入队，false 队列已满
    bool tryPush(FramePtr frame);

    // -------------------------------------------------------------------------
    // 出队操作（消费者侧）
    // -------------------------------------------------------------------------

    // 查看队首帧，不移除队列。队列为空时立即返回 false
    // 调用方拿到的 shared_ptr 与队列中共享同一块 AVFrame 内存，
    // 后续即使 pop() 移除队列中的引用，调用方手中的帧依然有效
    bool peek(FramePtr& out) const;

    // 阻塞出队：取出并移除队首帧
    // timeoutMs = 0：无限等待，直至有帧可取或队列被 stop()
    // timeoutMs > 0：等待指定毫秒数，超时后返回 false
    // 返回值：true 成功取出帧，false 超时或队列已停止
    bool pop(FramePtr& out, int timeoutMs = 0);

    // -------------------------------------------------------------------------
    // 队列控制
    // -------------------------------------------------------------------------

    // 清空队列中所有帧，通常配合 stop() 在 Seek 或关闭时使用
    void clear();

    // 停止队列：标记停止状态，唤醒所有在 push()/pop() 上阻塞的线程
    // 停止后 push() 会直接丢弃帧，pop() 会立即返回 false
    void stop();

    // 启动/恢复队列：清除停止标记，通常在 Seek 完成后调用
    void start();

    // 查询队列是否处于停止状态
    bool isStopped() const { return m_stopped; }

    // 获取当前队列中帧的数量（线程安全）
    size_t size() const;

    // 动态调整最大容量（高倍速时增大，避免解码线程阻塞）
    void setMaxSize(int size);

private:
    // 帧队列，存储 shared_ptr<AVFrame>，引用计数自动管理帧生命周期
    std::queue<FramePtr> m_queue;

    // 互斥锁，保护 m_queue 的读写，mutable 允许在 const 成员函数中加锁
    mutable std::mutex m_mutex;

    // 条件变量，用于 push() 等待队列非满、pop() 等待队列非空
    std::condition_variable m_cond;

    // 停止标志（原子变量），stop() 后所有阻塞操作立即返回
    std::atomic<bool> m_stopped;

    // 队列最大容量（原子变量），支持运行时动态调整
    std::atomic<int> m_maxSize;
};