#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

class PacketQueue;

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

class AudioDecoder {
public:
    AudioDecoder();
    ~AudioDecoder();

    bool open(AVCodecParameters* codecParams, AVRational timeBase);
    void close();

    void start(PacketQueue* packetQueue, std::atomic<bool>* eofFlag);
    void stop();

    void seek();
    void clearFrames();

    bool popFrame(AVFrame*& out, int timeoutMs = 0);

    int getSampleRate() const { return m_sampleRate; }
    int getChannels() const { return m_channels; }
    AVSampleFormat getSampleFormat() const { return m_sampleFmt; }
    bool isRunning() const { return m_running; }

private:
    void decodeLoop();

    AVCodecContext* m_codecCtx = nullptr;
    PacketQueue* m_packetQueue = nullptr;
    std::atomic<bool>* m_eofFlag = nullptr;
    AVRational m_timeBase{0, 0};

    int m_sampleRate = 0;
    int m_channels = 0;
    AVSampleFormat m_sampleFmt = AV_SAMPLE_FMT_NONE;

    std::queue<AVFrame*> m_frames;
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;

    std::thread m_decodeThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_flushDecoder{false};

    mutable std::mutex m_closeMutex;
    bool m_closed{false};

    AudioDecoder(const AudioDecoder&) = delete;
    AudioDecoder& operator=(const AudioDecoder&) = delete;
};
