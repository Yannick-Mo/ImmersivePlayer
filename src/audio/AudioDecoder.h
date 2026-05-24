#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

class AudioDecoder {
public:
    AudioDecoder();
    ~AudioDecoder();

    bool open(const std::string& url);
    void close();
    void start();
    void stop();
    void seek(int64_t ptsMicroseconds);
    void cancel();
    void setInterruptSeek(bool seeking);

    int getSampleRate() const { return m_sampleRate; }
    int getChannels() const { return m_channels; }
    AVSampleFormat getSampleFormat() const { return m_sampleFmt; }
    int64_t getDuration() const;
    bool isRunning() const { return m_running; }

    bool popFrame(AVFrame*& out, int timeoutMs = 0);

    static int interruptCallback(void *ctx);

private:
    void decodeLoop();

    AVFormatContext* m_formatCtx;
    AVCodecContext* m_codecCtx;
    int m_audioStreamIndex;

    int m_sampleRate;
    int m_channels;
    AVSampleFormat m_sampleFmt;

    std::thread m_decodeThread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_cancelled{false};
    std::atomic<bool> m_interruptSeek{false};

    std::queue<AVFrame*> m_frames;
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    std::mutex m_seekCloseMutex;  // serializes seek() / close() to prevent use-after-free

    AudioDecoder(const AudioDecoder&) = delete;
    AudioDecoder& operator=(const AudioDecoder&) = delete;
};