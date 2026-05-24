#include "PacketQueue.h"

PacketQueue::PacketQueue(int maxSize)
    : m_stopped(false)
    , m_maxSize(maxSize)
{
}

PacketQueue::~PacketQueue()
{
    clear();
}

void PacketQueue::push(AVPacket* pkt)
{
    if (m_stopped) return;

    std::unique_lock<std::mutex> lock(m_mutex);
    // 队列满时等待，直到有空位或队列被停止
    m_cond.wait(lock, [this] {
        return m_queue.size() < static_cast<size_t>(m_maxSize) || m_stopped;
    });

    if (m_stopped) return;

    // 拷贝一份包放入队列（引用计数 +1）
    AVPacket copy{};
    if (av_packet_ref(&copy, pkt) < 0) return;
    m_queue.push(copy);

    // 通知可能正在等待取包的线程
    m_cond.notify_one();
}

bool PacketQueue::pop(AVPacket& pkt, int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (timeoutMs > 0) {
        m_cond.wait_for(lock, std::chrono::milliseconds(timeoutMs),
            [this] { return !m_queue.empty() || m_stopped; });
    } else if (timeoutMs < 0) {
        m_cond.wait(lock, [this] { return !m_queue.empty() || m_stopped; });
    }
    // timeoutMs == 0: 不等待，直接检查队列

    if (m_stopped || m_queue.empty()) {
        return false;
    }

    // 使用移动语义转移包的所有权，避免引用计数错误
    av_packet_move_ref(&pkt, &m_queue.front());
    m_queue.pop();

    // 通知 push 端现在有空位了
    m_cond.notify_one();
    return true;
}

void PacketQueue::clear()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_queue.empty()) {
            av_packet_unref(&m_queue.front());
            m_queue.pop();
        }
    }
    // 必须唤醒所有可能在 push/pop 上等待的线程
    m_cond.notify_all();
}

void PacketQueue::stop()
{
    m_stopped = true;
    m_cond.notify_all();
}

void PacketQueue::start()
{
    m_stopped = false;
}

size_t PacketQueue::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}