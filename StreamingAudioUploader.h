#ifndef STREAMING_AUDIO_UPLOADER_H
#define STREAMING_AUDIO_UPLOADER_H

#include <QThread>
#include <QUrl>
#include <QStringList>
#include <QElapsedTimer>
#include <QString>
#include <atomic>
#include <curl/curl.h>
#include <QFile>
#include "AudioThread.h"


class StreamingAudioUploader : public QThread {
Q_OBJECT
    constexpr static const char *const m_endpoint = "https://api.mistral.ai/v1/audio/transcriptions"; // 60 seconds

public:
    explicit StreamingAudioUploader(AudioThread *audioThread, QObject *parent = nullptr);
    ~StreamingAudioUploader() override;
signals:
    // Emitted when upload finishes (success==true) or fails
    void finished(QByteArray response);
    void error(QString errorMessage);

protected:
    void run() override;

private slots:
    void onDataReady();
    void onAudioFinished();
    void onAudioError(const QString &error);

private:
    void setOpts();
    void setupRequest();
    void cleanupRequest();
    void cleanupForRetry();
    static int seekFunc(void *arg, curl_off_t offset, int origin);
    static size_t fileReadCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
    static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userdata);
    static int progressCallback(void *clientp,
                                curl_off_t dltotal,
                                curl_off_t dlnow,
                                curl_off_t ultotal,
                                curl_off_t ulnow);


    std::atomic<bool> shouldContinue = false;
    bool newFileFlag = true;
    bool m_timeoutFlag = false;
    uint8_t timeout_ctr = 0;
    QElapsedTimer m_requestTimer;
    CURL *m_curl;
    AudioThread *m_audioThread;
    curl_mime *m_mime = nullptr;
    curl_slist *m_headers = nullptr;
    QByteArray m_responseBuffer;
};

#endif // STREAMING_AUDIO_UPLOADER_H