#pragma once

#include "video/PacketQueue.h"
#include <atomic>
#include <thread>
#include <string>
#include <mutex>
#include <condition_variable>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

class Demuxer {
public:
    Demuxer();
    ~Demuxer();

    bool open(const std::string& url);
    void close();
    void start();
    void stop();
    void cancel();

    void seek(int64_t ptsMicroseconds);

    PacketQueue* getVideoPacketQueue() { return &m_videoPacketQueue; }
    PacketQueue* getAudioPacketQueue() { return &m_audioPacketQueue; }

    int getVideoStreamIndex() const { return m_videoStreamIndex; }
    int getAudioStreamIndex() const { return m_audioStreamIndex; }
    AVCodecParameters* getVideoCodecParameters() const;
    AVCodecParameters* getAudioCodecParameters() const;
    AVRational getVideoTimeBase() const;
    AVRational getAudioTimeBase() const;
    int64_t getDuration() const;
    bool isRunning() const { return m_running; }
    bool isLiveStream() const;
    bool eofReached() const { return m_eofReached; }

public:
    std::atomic<bool> m_eofReached{false};

private:
    void demuxLoop();
    static int interruptCallback(void* ctx);

    AVFormatContext* m_formatCtx = nullptr;
    int m_videoStreamIndex = -1;
    int m_audioStreamIndex = -1;

    PacketQueue m_videoPacketQueue{50};
    PacketQueue m_audioPacketQueue{200};

    std::thread m_demuxThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cancelled{false};

    std::atomic<bool> m_seekRequest{false};
    std::atomic<int64_t> m_seekTarget{0};
    std::mutex m_seekMutex;
    std::condition_variable m_seekDone;

    std::mutex m_closeMutex;
    bool m_closed{false};
};
