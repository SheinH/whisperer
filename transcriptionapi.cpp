#include "TranscriptionAPI.h"
#include <QJsonArray>
#include <QDebug>

TranscriptionAPI::TranscriptionAPI(QObject *parent) : QObject(parent) {
    connect(&m_webSocket, &QWebSocket::connected, this, &TranscriptionAPI::onConnected);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &TranscriptionAPI::onTextMessageReceived);
    connect(&m_webSocket, &QWebSocket::binaryMessageReceived, this, &TranscriptionAPI::onBinaryMessageReceived);
    connect(&m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            [=](QAbstractSocket::SocketError error){
                qWarning() << "WebSocket error:" << error;
            });
    connect(&m_webSocket, &QWebSocket::disconnected, this, &TranscriptionAPI::onDisconnected);
}

TranscriptionAPI::~TranscriptionAPI() {
    if (m_webSocket.state() == QAbstractSocket::ConnectedState) {
        m_webSocket.close();
    }
}

void TranscriptionAPI::startConnection() {
    QNetworkRequest request(m_url);
    request.setRawHeader("Authorization", ("Token " + m_apiKey).toUtf8());
    m_webSocket.open(request);
}

void TranscriptionAPI::sendAudioData(const QByteArray &data) {
    if (m_webSocket.state() == QAbstractSocket::ConnectedState) {
        m_webSocket.sendBinaryMessage(data);
    } else {
        qWarning() << "Cannot send data: WebSocket not connected";
    }
}

void TranscriptionAPI::stopConnection() {
    sendCloseStream();
}

void TranscriptionAPI::onConnected() {
    qDebug() << "WebSocket connected";
}

void TranscriptionAPI::onTextMessageReceived(const QString &message) {
    // Here you would typically not expect text messages for audio streaming,
    // but you might want to handle control messages if any.
    qDebug() << "Received text message:" << message;
}

void TranscriptionAPI::onBinaryMessageReceived(const QByteArray &message) {
    // Assuming that transcriptions might come in binary JSON for efficiency
    QJsonDocument doc = QJsonDocument::fromJson(message);
    if (!doc.isNull() && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj["type"].toString() == "Results") {
            QJsonArray alternatives = obj["channel"].toObject()["alternatives"].toArray();
            QString transcript = alternatives[0].toObject()["transcript"].toString();
            if (!transcript.isEmpty()) {
                emit transcriptReceived(transcript);
            }
        }
    }
}

void TranscriptionAPI::onDisconnected() {
    qDebug() << "WebSocket disconnected";
}

void TranscriptionAPI::sendCloseStream() {
    if (m_webSocket.state() == QAbstractSocket::ConnectedState) {
        QJsonObject closeMessage;
        closeMessage["type"] = "CloseStream";
        m_webSocket.sendTextMessage(QJsonDocument(closeMessage).toJson(QJsonDocument::Compact));
        m_webSocket.close(); // This will trigger onDisconnected eventually
    }
}