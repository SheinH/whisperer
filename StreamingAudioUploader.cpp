#include <QThreadPool>
#include "StreamingAudioUploader.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace {
    QMutex g_curlInitMutex;
    bool g_curlInitialized = false;
    // Looks up the Mistral API key: the MISTRAL_API_KEY environment variable wins,
    // otherwise fall back to the "mistralApiKey" entry in the app's QSettings.
    QByteArray loadApiKey() {
        QByteArray key = qgetenv("MISTRAL_API_KEY").trimmed();
        if (key.isEmpty())
            key = QSettings().value("mistralApiKey").toString().trimmed().toUtf8();
        return key;
    }

    void cleanupCurlGlobal() {
        QMutexLocker locker(&g_curlInitMutex);
        if (g_curlInitialized) {
            curl_global_cleanup();
            g_curlInitialized = false;
        }
    }
    inline qint64 getTimeout(int attempt,
                                            qint64 baseMs      = 6000,
                                            double factor      = 1.5)
    {
        // 1) compute exponential
        double expTime = baseMs * std::pow(factor, attempt);

        return static_cast<qint64>(std::round(expTime));
    }
}

StreamingAudioUploader::StreamingAudioUploader(AudioThread *audioThread, QObject *parent) :
        m_audioThread(audioThread), QThread(parent) {
    m_requestTimer.invalidate();
    {
        QMutexLocker locker(&g_curlInitMutex);
        if (!g_curlInitialized) {
            CURLcode code = curl_global_init(CURL_GLOBAL_DEFAULT);
            if (code != CURLE_OK) {
                qCritical() << "curl_global_init failed:" << curl_easy_strerror(code);
            } else {
                g_curlInitialized = true;
                atexit(cleanupCurlGlobal);
            }
        }
    }
    m_curl = curl_easy_init();
    connect(m_audioThread, &AudioThread::dataReady,
            this, &StreamingAudioUploader::onDataReady, Qt::QueuedConnection);
    connect(m_audioThread, &AudioThread::recordingStopped,
            this, &StreamingAudioUploader::onAudioFinished, Qt::QueuedConnection);
    connect(m_audioThread, &AudioThread::errorHappened,
            this, &StreamingAudioUploader::onAudioError, Qt::QueuedConnection);
    setOpts();
    setupRequest();
}

QByteArray extractTextField(const QByteArray &jsonData)
{
    // Parse the bytes into a JSON document
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);

    // Verify it is a valid JSON object
    if (doc.isObject()) {
        // 1. Get the root object
        // 2. Look up "text" (returns QJsonValue::Undefined if missing)
        // 3. Convert to QString (handles JSON un-escaping like \n or \")
        // 4. Convert back to UTF-8
        return doc.object().value("text").toString().toUtf8();
    }

    // Return empty if parsing failed or key doesn't exist
    return QByteArray();
}

void StreamingAudioUploader::run() {
    for(;;) {
        {
            QMutexLocker locker(&m_audioThread->m_bufMutex);
            while (m_audioThread->m_buffers.isEmpty() && !isInterruptionRequested()) {
                m_audioThread->m_bufCond.wait(&m_audioThread->m_bufMutex,500);
            }
        }
        CURLcode res = curl_easy_perform(m_curl);
        qDebug() << res;
        if (res == CURLE_OK) {
            // if (!m_responseBuffer.isEmpty() && m_responseBuffer[0] == ' ')
                    emit(finished(extractTextField(m_responseBuffer)));
            // else
            //         emit(finished(m_responseBuffer));
        }
        else {
            qDebug() << "curl_easy_perform failed:" << curl_easy_strerror(res);
            if (res == CURLE_ABORTED_BY_CALLBACK && m_timeoutFlag) {
                if (timeout_ctr > 3) {
                    QApplication::beep();
                    emit(error("Max timeout reached"));
                } else {
                    timeout_ctr++;
                    cleanupForRetry();
                    continue;
                }
            }
            else if (res == CURLE_HTTP_RETURNED_ERROR){
                QApplication::beep();
                long http_code = 0;
                curl_easy_getinfo (m_curl, CURLINFO_RESPONSE_CODE, &http_code);
                qDebug() << "HTTP error:" << http_code;;

                qDebug() << QString(m_responseBuffer);
                emit(error("HTTP error"));
            }
        }
        cleanupRequest();
        if (isInterruptionRequested()) break;
    }
}
void StreamingAudioUploader::onAudioError(const QString &error) {
}
void StreamingAudioUploader::onDataReady() {
    shouldContinue = true;
}
void StreamingAudioUploader::onAudioFinished() {
    m_requestTimer.start();
}
int StreamingAudioUploader::progressCallback(void *clientp,
                                             curl_off_t /*dltotal*/,
                                             curl_off_t /*dlnow*/,
                                             curl_off_t ultotal,
                                             curl_off_t ulnow) {
    auto *self = static_cast<StreamingAudioUploader *>(clientp);
    if(self->shouldContinue) {
        curl_easy_pause(self->m_curl, CURLPAUSE_CONT);
        self->shouldContinue = false;
    }
    auto &timer = self->m_requestTimer;
    if (timer.isValid() && timer.elapsed() > getTimeout(self->timeout_ctr)) {
        timer.invalidate();
        self->m_timeoutFlag = true;
        return 1;
    }
    return 0;
}


StreamingAudioUploader::~StreamingAudioUploader() {
    if (m_mime)
        curl_mime_free(m_mime);
    curl_easy_cleanup(m_curl);
    curl_slist_free_all(m_headers);
    disconnect(m_audioThread, &AudioThread::dataReady, this, &StreamingAudioUploader::onDataReady);
    disconnect(m_audioThread, &AudioThread::recordingStopped, this, &StreamingAudioUploader::onAudioFinished);
    disconnect(m_audioThread, &AudioThread::errorHappened, this, &StreamingAudioUploader::onAudioError);
}


void StreamingAudioUploader::setupRequest() {
    m_mime = curl_mime_init(m_curl);
    auto addTextPart = [&](const char *name, const char *value) {
        curl_mimepart *part = curl_mime_addpart(m_mime);
        curl_mime_name(part, name);
        curl_mime_data(part, value, CURL_ZERO_TERMINATED);
    };
    addTextPart("model", "voxtral-mini-latest");
    addTextPart("language", "en");
    // addTextPart("response_format", "text");
    curl_mimepart *part = curl_mime_addpart(m_mime);
    curl_mime_name(part, "file");
    curl_mime_filename(part, "audio.ogg");
    curl_mime_type(part, "audio/ogg");
    curl_mime_data_cb(part,
            /* length */ -1,
                      &StreamingAudioUploader::fileReadCallback,
                      &StreamingAudioUploader::seekFunc, nullptr,
                      this);
    curl_easy_setopt(m_curl, CURLOPT_MIMEPOST, m_mime);
}

void StreamingAudioUploader::setOpts() {
    curl_easy_setopt(m_curl, CURLOPT_URL, m_endpoint);
    curl_easy_setopt(m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2);
    curl_easy_setopt(m_curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(m_curl, CURLOPT_UPLOAD_BUFFERSIZE, 131072L);
    curl_easy_setopt(m_curl, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &StreamingAudioUploader::writeCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, &StreamingAudioUploader::progressCallback);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(m_curl, CURLOPT_TRANSFER_ENCODING, 1L);
    const QByteArray apiKey = loadApiKey();
    if (apiKey.isEmpty())
        qCritical() << "No Mistral API key found. Set MISTRAL_API_KEY or the mistralApiKey setting.";
    m_headers = curl_slist_append(m_headers, ("x-api-key: " + apiKey).constData());
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);
    curl_easy_setopt(m_curl, CURLOPT_FAILONERROR, 1L);
}
void StreamingAudioUploader::cleanupRequest() {
    {
        QMutexLocker locker(&m_audioThread->m_bufMutex);
        if (!m_audioThread->m_buffers.isEmpty()) {
            qDebug() << "cleanupRequest: removing first buffer";
            m_audioThread->m_buffers.pop_front();
            newFileFlag = true;
        }
    }
//    curl_mime_free(m_mime);
//    m_mime = nullptr;
    timeout_ctr = 0;
    m_responseBuffer.clear();
    m_requestTimer.invalidate();
    m_timeoutFlag = false;
}
void StreamingAudioUploader::cleanupForRetry() {
    qDebug() << "Retrying!";
    {
        QMutexLocker locker(&m_audioThread->m_bufMutex);
        if (!m_audioThread->m_buffers.isEmpty()) {
            m_audioThread->m_buffers.front().readPos = 0;
        }
    }
//    curl_mime_free(m_mime);
//    m_mime = nullptr;
    m_responseBuffer.clear();
    m_requestTimer.start();
    m_timeoutFlag = false;
}


size_t StreamingAudioUploader::writeCallback(void *contents,
                                             size_t size,
                                             size_t nmemb,
                                             void *userdata) {
    auto *self = static_cast<StreamingAudioUploader *>(userdata);
    size_t total = size * nmemb;
    if (self) {
        self->m_responseBuffer.append(static_cast<const char *>(contents), int(total));
    }
    return total;
}

size_t StreamingAudioUploader::fileReadCallback(char *ptr,
                                                size_t size,
                                                size_t nmemb,
                                                void *userdata) {
    auto *self = static_cast<StreamingAudioUploader *>(userdata);
    if (!self || !self->m_audioThread) {
        return CURL_READFUNC_ABORT;
    }
    if (self->isInterruptionRequested()) {
        return CURL_READFUNC_ABORT;
    }

    size_t total = size * nmemb, bytesRead = 0;
    QMutexLocker locker(&self->m_audioThread->m_bufMutex);
    if (!self->m_audioThread->m_buffers.isEmpty()) {
        auto &firstBuffer = self->m_audioThread->m_buffers.first();
        bytesRead = qMin(total, firstBuffer.size());
        memcpy(ptr, firstBuffer.data.data() + firstBuffer.readPos, bytesRead);
        firstBuffer.readPos += bytesRead;
        if (firstBuffer.finished && firstBuffer.size() == 0) {
            const QByteArray dataCopy = firstBuffer.data;
//            QThreadPool::globalInstance()->start( [dataCopy] {
//                QFile file(QStringLiteral("/Users/meow/development/Qt/whisperer/audio.opus"));
//                if (file.open(QIODevice::WriteOnly)) {
//                    file.write(dataCopy);
//                    file.close();
//                } else {
//                    qWarning() << "Failed to open opus file for writing:" << file.errorString();
//                }
//            });
            qDebug() << "eof";
            return 0;
        }
    }

    if (bytesRead > 0) {
        self->newFileFlag = false;
        return static_cast<size_t>(bytesRead);
    }
    return CURL_READFUNC_PAUSE;
}

int StreamingAudioUploader::seekFunc(void *arg, curl_off_t offset, int origin) {
    auto *self = static_cast<StreamingAudioUploader *>(arg);
    if (!self || !self->m_audioThread) {
        return CURL_SEEKFUNC_FAIL;
    }

    if (self->newFileFlag) {
        return CURL_SEEKFUNC_OK;
    }

    QMutexLocker locker(&self->m_audioThread->m_bufMutex);
    if (self->m_audioThread->m_buffers.isEmpty()) {
        return CURL_SEEKFUNC_FAIL;
    }

    auto &firstBuffer = self->m_audioThread->m_buffers.first();
    size_t bufSize = firstBuffer.size();
    size_t newPos;

    switch (origin) {
        case SEEK_SET:
            newPos = offset;
            break;
        case SEEK_CUR:
            newPos = firstBuffer.readPos + offset;
            break;
        case SEEK_END:
            newPos = bufSize + offset;
            break;
        default:
            return CURL_SEEKFUNC_FAIL;
    }

    if (newPos < 0 || newPos > bufSize) {
        return CURL_SEEKFUNC_FAIL;
    }

    firstBuffer.readPos = newPos;
    return CURL_SEEKFUNC_OK;
}