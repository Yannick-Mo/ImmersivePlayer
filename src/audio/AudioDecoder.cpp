#include "AudioDecoder.h"
#include <chrono>

AudioDecoder::AudioDecoder()
    : m_formatCtx(nullptr)
    , m_codecCtx(nullptr)
    , m_audioStreamIndex(-1)
    , m_sampleRate(0)
    , m_channels(0)
    , m_sampleFmt(AV_SAMPLE_FMT_NONE)
    , m_running(false)
{
}

AudioDecoder::~AudioDecoder() {
    close();
}

bool AudioDecoder::open(const std::string& url) {
    close();

    m_cancelled = false;

    m_formatCtx = avformat_alloc_context();
    if (!m_formatCtx) return false;

    m_formatCtx->interrupt_callback.callback = interruptCallback;
    m_formatCtx->interrupt_callback.opaque = this;

    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "timeout", "5000000", 0);
    av_dict_set(&opts, "listen_timeout", "5000000", 0);
    av_dict_set(&opts, "stimeout", "5000000", 0);
    av_dict_set(&opts, "user_agent", "ImmersivePlayer/1.0", 0);

    if (avformat_open_input(&m_formatCtx, url.c_str(), nullptr, &opts) < 0) {
        av_dict_free(&opts);
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }
    av_dict_free(&opts);

    if (m_cancelled) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }

    if (avformat_find_stream_info(m_formatCtx, nullptr) < 0) {
        close();
        return false;
    }

    if (m_cancelled) {
        close();
        return false;
    }

    m_audioStreamIndex = -1;
    for (unsigned i = 0; i < m_formatCtx->nb_streams; ++i) {
        if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            m_audioStreamIndex = i;
            break;
        }
    }
    if (m_audioStreamIndex == -1) {
        close();
        return false;
    }

    AVCodecParameters* codecPar = m_formatCtx->streams[m_audioStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        close();
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) {
        close();
        return false;
    }
    if (avcodec_parameters_to_context(m_codecCtx, codecPar) < 0) {
        close();
        return false;
    }
    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        close();
        return false;
    }

    m_sampleRate = codecPar->sample_rate;
    m_channels = m_codecCtx->ch_layout.nb_channels;
    m_sampleFmt = static_cast<AVSampleFormat>(codecPar->format);

    return true;
}

void AudioDecoder::close() {
    std::lock_guard<std::mutex> lock(m_seekCloseMutex);
    m_cancelled = true;
    stop();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_frames.empty()) {
            av_frame_free(&m_frames.front());
            m_frames.pop();
        }
    }
    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
    }
    if (m_formatCtx) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
    }
    m_audioStreamIndex = -1;
    m_sampleRate = 0;
    m_channels = 0;
    m_sampleFmt = AV_SAMPLE_FMT_NONE;
}

void AudioDecoder::seek(int64_t ptsMicroseconds) {
    std::lock_guard<std::mutex> lock(m_seekCloseMutex);
    if (!m_formatCtx || m_audioStreamIndex < 0) return;
    if (!m_codecCtx) return;

    bool wasRunning = m_running;

    if (wasRunning) {
        m_running = false;
        m_cond.notify_all();
        if (m_decodeThread.joinable())
            m_decodeThread.join();
    }

    avcodec_flush_buffers(m_codecCtx);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_frames.empty()) {
            av_frame_free(&m_frames.front());
            m_frames.pop();
        }
    }

    int64_t seekStream = av_rescale_q(ptsMicroseconds, AV_TIME_BASE_Q,
        m_formatCtx->streams[m_audioStreamIndex]->time_base);
    av_seek_frame(m_formatCtx, m_audioStreamIndex, seekStream, AVSEEK_FLAG_BACKWARD);

    if (wasRunning) {
        m_running = true;
        m_decodeThread = std::thread(&AudioDecoder::decodeLoop, this);
    }
}

void AudioDecoder::start() {
    if (!m_formatCtx) return;
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) return;
    m_decodeThread = std::thread(&AudioDecoder::decodeLoop, this);
}

void AudioDecoder::stop() {
    if (m_running) {
        m_running = false;
        m_cond.notify_all();
        if (m_decodeThread.joinable())
            m_decodeThread.join();
    }
}

void AudioDecoder::cancel() {
    m_cancelled = true;
}

void AudioDecoder::decodeLoop() {
    AVPacket pkt;
    AVFrame* frame = av_frame_alloc();
    if (!frame) return;
    bool drained = false;

    while (m_running) {
        int ret = av_read_frame(m_formatCtx, &pkt);
        if (ret < 0) {
            if (!m_running) break;
            if (ret == AVERROR_EOF) {
                if (!drained) {
                    avcodec_send_packet(m_codecCtx, nullptr);
                    while (m_running) {
                        ret = avcodec_receive_frame(m_codecCtx, frame);
                        if (ret < 0) break;
                        AVFrame* copy = av_frame_alloc();
                        if (copy) {
                            av_frame_ref(copy, frame);
                            {
                                std::unique_lock<std::mutex> lock(m_mutex);
                                m_frames.push(copy);
                            }
                            m_cond.notify_one();
                        }
                    }
                    drained = true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            // 被中断回调打断则不忙等，直接退出循环
            if (m_cancelled) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        drained = false;

        if (pkt.stream_index == m_audioStreamIndex) {
            if (avcodec_send_packet(m_codecCtx, &pkt) == 0) {
                while (m_running) {
                    ret = avcodec_receive_frame(m_codecCtx, frame);
                    if (ret < 0) break;

                    AVFrame* copy = av_frame_alloc();
                    if (copy) {
                        av_frame_ref(copy, frame);
                        {
                            std::unique_lock<std::mutex> lock(m_mutex);
                            m_frames.push(copy);
                        }
                        m_cond.notify_one();

                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_cond.wait_for(lock, std::chrono::milliseconds(50),
                            [this] { return m_frames.size() <= 12 || !m_running; });
                    }
                }
            }
        }
        av_packet_unref(&pkt);
    }

    av_frame_free(&frame);
}

bool AudioDecoder::popFrame(AVFrame*& out, int timeoutMs) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (timeoutMs > 0) {
        m_cond.wait_for(lock, std::chrono::milliseconds(timeoutMs),
            [this] { return !m_frames.empty() || !m_running; });
    } else if (timeoutMs < 0) {
        m_cond.wait(lock, [this] { return !m_frames.empty() || !m_running; });
    }
    if (m_frames.empty()) return false;
    out = m_frames.front();
    m_frames.pop();
    return true;
}

int64_t AudioDecoder::getDuration() const {
    if (!m_formatCtx) return 0;
    return m_formatCtx->duration;
}

int AudioDecoder::interruptCallback(void *ctx) {
    auto *decoder = static_cast<AudioDecoder*>(ctx);
    return (decoder->m_cancelled || decoder->m_interruptSeek) ? 1 : 0;
}

void AudioDecoder::setInterruptSeek(bool seeking) {
    m_interruptSeek = seeking;
}