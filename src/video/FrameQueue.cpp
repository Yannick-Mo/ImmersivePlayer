#include "FrameQueue.h"

FrameQueue::FrameQueue(int maxSize) : m_stopped(false), m_maxSize(maxSize) {}
FrameQueue::~FrameQueue() { clear(); }

void FrameQueue::push(FramePtr frame) {
    if (m_stopped) return;
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cond.wait(lock, [this] {
        return m_queue.size() < static_cast<size_t>(m_maxSize.load()) || m_stopped;
    });
    if (m_stopped) return;
    m_queue.push(frame);
    m_cond.notify_one();
}

bool FrameQueue::tryPush(FramePtr frame) {
    if (m_stopped) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.size() >= static_cast<size_t>(m_maxSize.load()))
        return false;
    m_queue.push(frame);
    m_cond.notify_one();
    return true;
}

bool FrameQueue::peek(FramePtr& out) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.empty()) return false;
    out = m_queue.front();
    return true;
}

bool FrameQueue::pop(FramePtr& out, int timeoutMs) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (timeoutMs > 0) {
        m_cond.wait_for(lock, std::chrono::milliseconds(timeoutMs),
            [this] { return !m_queue.empty() || m_stopped; });
    } else if (timeoutMs < 0) {
        m_cond.wait(lock, [this] { return !m_queue.empty() || m_stopped; });
    }
    if (m_stopped || m_queue.empty()) return false;
    out = m_queue.front();
    m_queue.pop();
    m_cond.notify_one();
    return true;
}

void FrameQueue::clear() {
    {    
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_queue.empty()) m_queue.pop();
    }
    m_cond.notify_all();
}

void FrameQueue::stop() {
    m_stopped = true;
    m_cond.notify_all();
}

void FrameQueue::start() {
    m_stopped = false;
}

size_t FrameQueue::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

void FrameQueue::setMaxSize(int size) {
    if (size < 1) size = 1;
    if (size > 60) size = 60;  // 设置合理上限
    m_maxSize = size;
    m_cond.notify_all();  // 唤醒可能因容量不足而等待的 push 线程
}