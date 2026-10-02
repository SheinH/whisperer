// AudioThread.hh
#pragma once

#include <QThread>
#include <QByteArray>
#include <QMutex>
#include <QLoggingCategory>
#include <atomic>
#include <QtCore/qqueue.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
}

// ──────────────────────────────────────────────────────────────────────────────
// --- Build-time options ──────────────────────────────────────────────────────
#ifndef AUDIO_THREAD_DEBUG            // can be set from cmake or qmake
#define AUDIO_THREAD_DEBUG    0       // 1 ⇒ always log, 0 ⇒ follow QLogging
#endif
// Encoder “presets” (may be overridden locally or by a build system-define)
namespace EncPreset {
    inline constexpr int        kChannels     = 1;
    inline constexpr int        kSampleRate   = 16'000;
    inline constexpr int        kBitRate      = 1024*32;
    inline constexpr AVSampleFormat kSampleFmt = AV_SAMPLE_FMT_S16;
    inline constexpr std::string_view kContainer = "ogg";
//    inline constexpr AVCodecID  kCodecId      = AV_CODEC_ID_AAC;
    inline constexpr std::string_view codecName = "libopus";
} // namespace EncPreset
// ──────────────────────────────────────────────────────────────────────────────

Q_DECLARE_LOGGING_CATEGORY(lcAudioThread)
#include <QByteArray>
#include <cstddef>
#include <QWaitCondition>

struct RecordingBuffer {
    QByteArray   data = QByteArrayLiteral("");             // empty by default
    std::size_t  readPos{0};         // zero by default
    bool         finished{false};    // false by default

    [[nodiscard]]
    std::size_t size() const noexcept {
        // guard just in case readPos > data.size()
        return (data.size() > readPos)
               ? (data.size() - readPos)
               : 0;
    }
};

class AudioThread final : public QThread
{
Q_OBJECT
public:
    enum class State { Idle, Recording };

    explicit AudioThread(QObject *parent = nullptr);
    ~AudioThread() override;

    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void openInput();      // explicit open / close for mic
    Q_INVOKABLE void closeInput();

    [[nodiscard]] State        state() const noexcept { return m_state.load(); }

    // thread-safe access to encoded data
    QMutex              m_bufMutex;
    QWaitCondition     m_bufCond;
    QQueue<RecordingBuffer> m_buffers;

signals:
    void initialized(bool ok);
    void recordingStarted();
    void recordingStopped();
    void errorHappened(const QString &msg);
    void dataReady();

protected:
    void run() override;

private:
    // helpers --------------------------------------------------------------
    bool        setupOutput();
    void        flushAndTearDownOutput();
    static int  writeCallback(void *opaque,const uint8_t *buf,int size);
    int         handleWrite(const uint8_t *buf,int size);
    bool        resampleAndStore(const AVFrame *decoded);
    bool        encodeAndWrite();

    // constant members -----------------------------------------------------

    // thread / state -------------------------------------------------------
    std::atomic<State>  m_state{State::Idle};
    std::atomic_bool    m_abort{false};
    std::atomic_bool    m_startRequested{false};
    std::atomic_bool    m_stopRequested{false};

    // FFmpeg (input) -------------------------------------------------------
    AVFormatContext    *m_inFmtCtx {nullptr};
    AVCodecContext     *m_inDecCtx {nullptr};
    int                 m_inStream {-1};

    // FFmpeg (output) ------------------------------------------------------
    AVFormatContext    *m_outFmtCtx{nullptr};
    AVCodecContext     *m_outEncCtx{nullptr};
    AVIOContext        *m_avioCtx  {nullptr};
    AVBufferRef        *m_avioBuf  {nullptr};
    AVAudioFifo        *m_fifo     {nullptr};
    SwrContext         *m_swr      {nullptr};
    int64_t             m_pts      {0};          // moved to member

    // reusable frame / packet ---------------------------------------------
    AVFrame           *m_decFrame  {nullptr};
    AVPacket          *m_pkt       {nullptr};
    AVPacket          *m_output_pkt       {nullptr};
    AVFrame           *m_output_frame       {nullptr};
};
