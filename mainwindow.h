#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include "transcriptionapi.h"
#include "audiorecorder.h"
#include "StreamingAudioUploader.h"
#include "AudioThread.h"
#include "keystrokes.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

    ~MainWindow();

public slots:

    void startKeyPressed();

    void stopKeyPressed();

    void stopKeyPressed2();

    void stopKeyPressed3();


private:
//    AudioRecorder audioRecorder;
//    TranscriptionAPI transcriptionApi;
    AudioThread audioThread;
    StreamingAudioUploader streamingAudioUploader;
    QNetworkAccessManager network;
    Ui::MainWindow *ui;
    bool shouldHitEnter = false;
    bool shouldBeAction = false;
//    ChunkedUploader chunkedUploader;
    QElapsedTimer timer;
    KeystrokesThread keystrokesThread;
};

#endif // MAINWINDOW_H
