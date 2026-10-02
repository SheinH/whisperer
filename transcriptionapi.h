#ifndef TRANSCRIPTIONAPI_H
#define TRANSCRIPTIONAPI_H

#include <QObject>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>

class TranscriptionAPI : public QObject {
Q_OBJECT

public:
    explicit TranscriptionAPI(QObject *parent = nullptr);
    ~TranscriptionAPI();

public slots:
    void startConnection();
    void sendAudioData(const QByteArray &data);
    void stopConnection();

signals:
    void transcriptReceived(const QString &transcript);

private slots:
    void onConnected();
    void onTextMessageReceived(const QString &message);
    void onBinaryMessageReceived(const QByteArray &message);
    void onDisconnected();

private:
    QWebSocket m_webSocket;
    const QString m_apiKey = "YOUR_DEEPGRAM_API_KEY"; // Replace with actual key
    const QUrl m_url = QUrl("wss://api.deepgram.com/v1/listen");

    void sendCloseStream();
};

#endif // TRANSCRIPTIONAPI_H