// AudioThread.cc
#include "AudioThread.h"
#include <QDebug>

Q_LOGGING_CATEGORY(lcAudioThread, "AudioThread", QtWarningMsg)

// ──────────────────────────────────────────────────────────────────────────────
// --- construction / destruction ──────────────────────────────────────────────
AudioThread::AudioThread(QObject *parent) : QThread(parent) {
    avdevice_register_all();
    m_decFrame = av_frame_alloc();
    m_pkt = av_packet_alloc();
    m_output_pkt = av_packet_alloc();
    m_output_frame = av_frame_alloc();
}

AudioThread::~AudioThread() {
    m_abort = true;
    wait();                 // stop thread

    closeInput();
    flushAndTearDownOutput();

    av_frame_free(&m_decFrame);
    av_packet_free(&m_pkt);
    if (m_fifo) av_audio_fifo_free(m_fifo);
    if (m_swr) swr_free(&m_swr);
}

// ──────────────────────────────────────────────────────────────────────────────
// --- public API (called from UI thread) ---
void AudioThread::openInput() {
    if (m_inFmtCtx) return;     // already open

    constexpr auto *kFmt = "avfoundation";
    constexpr auto *kDev = ":MacBook Pro Microphone";

    if (auto *ifmt = av_find_input_format(kFmt); ifmt) {
        AVDictionary *opts = nullptr;
        av_dict_set(&opts, "thread_queue_size", "512", 0);
        av_dict_set(&opts, "fflags", "nobuffer", 0);

        if (avformat_open_input(&m_inFmtCtx, kDev, ifmt, &opts) >= 0 &&
            avformat_find_stream_info(m_inFmtCtx, nullptr) >= 0) {
            m_inStream = av_find_best_stream(m_inFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
            if (m_inStream >= 0) {
                const AVStream *st = m_inFmtCtx->streams[m_inStream];
                const AVCodec *dec = avcodec_find_decoder(st->codecpar->codec_id);
                m_inDecCtx = avcodec_alloc_context3(dec);
                avcodec_parameters_to_context(m_inDecCtx, st->codecpar);
                if (avcodec_open2(m_inDecCtx, dec, nullptr) >= 0) {
                    emit initialized(true);
                    return;
                }
            }
        }
        av_dict_free(&opts);
    }
    emit initialized(false);
}

void AudioThread::closeInput() {
    if (!m_inFmtCtx) return;
    avcodec_free_context(&m_inDecCtx);
    avformat_close_input(&m_inFmtCtx);
    m_inStream = -1;
}

void AudioThread::startRecording() { m_startRequested = true; }

void AudioThread::stopRecording() { m_stopRequested = true; }


// ──────────────────────────────────────────────────────────────────────────────
// --- thread entry point ---
void AudioThread::run() {
    while (!m_abort) {
        if (m_startRequested.exchange(false) && m_state == State::Idle) {
            if (setupOutput()) {
                AVDictionary *options = nullptr;
                av_dict_set_int(&options, "page_duration", 20000, 0);
                QMutexLocker lock(&m_bufMutex);
                m_buffers.emplaceBack();
                m_bufCond.notify_all();
                lock.unlock();
                avformat_write_header(m_outFmtCtx, &options);
                av_dict_free(&options);
                m_state = State::Recording;
                emit recordingStarted();
            }
        }
        if (m_stopRequested.exchange(false) && m_state == State::Recording) {
            flushAndTearDownOutput();
            m_state = State::Idle;
            emit recordingStopped();
        }

        // nothing to do?
        if (!m_inFmtCtx) {
            msleep(5);
            continue;
        }

        // read one packet --------------------------------------------------
        if (av_read_frame(m_inFmtCtx, m_pkt) < 0) {
            msleep(8);
            continue;
        }
        if (m_pkt->stream_index != m_inStream || m_state == State::Idle) {
            av_packet_unref(m_pkt);
            continue;
        }
        // decode -----------------------------------------------------------
        if (avcodec_send_packet(m_inDecCtx, m_pkt) >= 0) {
            while (avcodec_receive_frame(m_inDecCtx, m_decFrame) == 0) {
                if (!resampleAndStore(m_decFrame))
                        emit errorHappened(QStringLiteral(u"Audio FIFO write failed"));
                while (av_audio_fifo_size(m_fifo) >= m_outEncCtx->frame_size) {
                    encodeAndWrite();
                }
                av_frame_unref(m_decFrame);
            }
        }
        av_packet_unref(m_pkt);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// --- output side (encode) ---
bool AudioThread::setupOutput() {
    constexpr int kAvioBufSize = 4096;

    m_pts = 0;
    m_avioBuf = av_buffer_alloc(kAvioBufSize);
    m_avioCtx = avio_alloc_context(m_avioBuf->data, kAvioBufSize, 1, this, nullptr, &AudioThread::writeCallback,
                                   nullptr);

    AVDictionary *opts = nullptr;
//    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
    if (avformat_alloc_output_context2(&m_outFmtCtx, nullptr,
                                       EncPreset::kContainer.data(), nullptr) < 0)
        return false;
    m_outFmtCtx->pb = m_avioCtx;
    m_outFmtCtx->flags |= AVFMT_FLAG_FLUSH_PACKETS  | AVFMT_FLAG_NOBUFFER;


//    const AVCodec *enc = avcodec_find_encoder_by_name(EncPreset::codecName.data());
    if (!m_outEncCtx) {
        const AVCodec *enc = avcodec_find_encoder_by_name(EncPreset::codecName.data());
        m_outEncCtx = avcodec_alloc_context3(enc);
        av_channel_layout_default(&m_outEncCtx->ch_layout, EncPreset::kChannels);
        m_outEncCtx->sample_rate = EncPreset::kSampleRate;
        m_outEncCtx->sample_fmt = EncPreset::kSampleFmt;
        m_outEncCtx->time_base = AVRational{1, EncPreset::kSampleRate};

// if your format needs a global header:
        if (m_outFmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
            m_outEncCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

// 1) Tell libavcodec to use Q-scale (so it picks global_quality)
//        m_outEncCtx->flags |= AV_CODEC_FLAG_QSCALE;

// 2) Set the *best* VBR quantizer (0 = highest internal quality)
//        m_outEncCtx->global_quality = FF_QP2LAMBDA * 10;
m_outEncCtx->bit_rate = EncPreset::kBitRate;

// 3) Build your option dict: VBR mode + speed/quality knob
        AVDictionary *encopts = NULL;
//        av_dict_set(&encopts, "aac_at_mode", "vbr", 0);
//        av_dict_set(&encopts, "aac_at_quality", "2", 0);
        av_dict_set(&encopts, "vbr", "on", 0);
        av_dict_set(&encopts, "application", "voip", 0);
        av_dict_set_int(&encopts, "frame_duration", 20, 0);
        av_dict_set_int(&encopts, "packet_loss", 0, 0);
        if (avcodec_open2(m_outEncCtx, enc, &encopts) < 0)
            return false;
        av_dict_free(&encopts);
    }

    AVStream *st = avformat_new_stream(m_outFmtCtx, nullptr);
    avcodec_parameters_from_context(st->codecpar, m_outEncCtx);
    st->time_base = m_outEncCtx->time_base;

    // resampler -----------------------------------------------------------
    auto ret = swr_alloc_set_opts2(&m_swr,
                                   &m_outEncCtx->ch_layout, m_outEncCtx->sample_fmt, m_outEncCtx->sample_rate,
                                   &m_inDecCtx->ch_layout, m_inDecCtx->sample_fmt, m_inDecCtx->sample_rate,
                                   0, nullptr);
    if (ret > 0 || swr_init(m_swr) < 0)
        return false;

    // fifo ----------------------------------------------------------------
    m_fifo = av_audio_fifo_alloc(m_outEncCtx->sample_fmt, m_outEncCtx->ch_layout.nb_channels, 1);
    return m_fifo;
}

void AudioThread::flushAndTearDownOutput() {
    if (!m_outFmtCtx) return;

    // drain remaining samples
    while (av_audio_fifo_size(m_fifo) > 0)
        encodeAndWrite();
    avcodec_send_frame(m_outEncCtx, nullptr);
    encodeAndWrite();
    qCDebug(lcAudioThread) << "AudioThread::flushAndTearDownOutput()1";
    av_interleaved_write_frame(m_outFmtCtx, nullptr);
//    if(m_outEncCtx->codec->capabilities & AV_CODEC_CAP_ENCODER_FLUSH) {
//        avcodec_flush_buffers(m_outEncCtx);
//    }
//    else{
        avcodec_free_context(&m_outEncCtx);
    qCDebug(lcAudioThread) << "AudioThread::flushAndTearDownOutput()2";
//    }

    avio_flush(m_outFmtCtx->pb);
    av_write_trailer(m_outFmtCtx);
    qCDebug(lcAudioThread) << "AudioThread::flushAndTearDownOutput()3";
    {
        QMutexLocker lock(&m_bufMutex);
        m_buffers.last().finished = true;
    }

    avio_context_free(&m_avioCtx);
    avformat_free_context(m_outFmtCtx);
    m_outFmtCtx = nullptr;

    av_buffer_unref(&m_avioBuf);
    av_audio_fifo_reset(m_fifo);
    swr_free(&m_swr);
}

bool AudioThread::resampleAndStore(const AVFrame *decoded) {
    const int inSamples = decoded->nb_samples;
    const int maxOut = av_rescale_rnd(swr_get_delay(m_swr, m_inDecCtx->sample_rate) + inSamples,
                                      m_outEncCtx->sample_rate, m_inDecCtx->sample_rate, AV_ROUND_UP);
    uint8_t **conv = nullptr;
    if (av_samples_alloc_array_and_samples(&conv, nullptr,
                                           m_outEncCtx->ch_layout.nb_channels, maxOut, m_outEncCtx->sample_fmt, 0) < 0)
        return false;

    const int outSamples = swr_convert(m_swr, conv, maxOut,
                                       (const uint8_t **) decoded->extended_data, inSamples);
    if (outSamples < 0) {
        av_freep(&conv[0]);
        av_freep(&conv);
        return false;
    }

    if (av_audio_fifo_realloc(m_fifo, av_audio_fifo_size(m_fifo) + outSamples) < 0) {
        av_freep(&conv[0]);
        av_freep(&conv);
        return false;
    }

    const bool ok = av_audio_fifo_write(m_fifo, (void **) conv, outSamples) == outSamples;
    av_freep(&conv[0]);
    av_freep(&conv);
    return ok;
}

bool AudioThread::encodeAndWrite() {
    // prepare frame -------------------------------------------------------
    if(av_audio_fifo_size(m_fifo) != 0) {
        int frame_size = FFMIN(av_audio_fifo_size(m_fifo),
                               m_outEncCtx->frame_size);
        qCDebug(lcAudioThread)  << "FRAME SIZE: "<< m_outEncCtx->frame_size;
        m_output_frame->nb_samples = frame_size;
        av_channel_layout_copy(&m_output_frame->ch_layout, &m_outEncCtx->ch_layout);
        m_output_frame->format = m_outEncCtx->sample_fmt;
        m_output_frame->sample_rate = m_outEncCtx->sample_rate;
        av_frame_get_buffer(m_output_frame, 0);

        av_audio_fifo_read(m_fifo, (void **) m_output_frame->data, frame_size);
        m_output_frame->pts = m_pts;
        m_pts += m_output_frame->nb_samples;

        // encode --------------------------------------------------------------
        if (avcodec_send_frame(m_outEncCtx, m_output_frame) < 0) {
            av_frame_unref(m_output_frame);
            return false;
        }
        av_frame_unref(m_output_frame);
    }

    while (avcodec_receive_packet(m_outEncCtx, m_output_pkt) == 0) {
        qCDebug(lcAudioThread) << "AudioThread::encodeAndWrite(): writing out";
        m_output_pkt->stream_index = 0;
        av_packet_rescale_ts(m_output_pkt, m_outEncCtx->time_base,
                             m_outFmtCtx->streams[0]->time_base);
        av_interleaved_write_frame(m_outFmtCtx, m_output_pkt);
    }
    av_interleaved_write_frame(m_outFmtCtx, nullptr);
    avio_flush(m_outFmtCtx->pb);
    return true;
}


// ──────────────────────────────────────────────────────────────────────────────
// --- AVIO write callback (cross-thread safe) ---
int AudioThread::writeCallback(void *opaque, const uint8_t *buf,
                               int size) { return static_cast<AudioThread *>(opaque)->handleWrite(buf, size); }

int AudioThread::handleWrite(const uint8_t *buf, int size) {
    QMutexLocker lock(&m_bufMutex);
//    m_buffer.insert(m_buffer.end(), buf, buf + size);
    auto &dest = m_buffers.last().data;
    dest.append(reinterpret_cast<const char *>(buf), size);
    lock.unlock();
    emit dataReady();
    return size;
}