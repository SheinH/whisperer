#include "mainwindow.h"
#include "macos_integrations.h"
#include <QDebug>
#include <QDebug>
#include <QThreadPool>
#include <QRunnable>

// Constructor
MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent), streamingAudioUploader(&audioThread) {
    keystrokesThread.start();

    // Fire up the audio thread so it's ready when we ask it to record.
    audioThread.start();
    streamingAudioUploader.start();
    audioThread.openInput();

    // Point the uploader at the transcription endpoint.
    // If you need extra headers (e.g. Authorization), do it here:
    // streamingAudioUploader.setHeaders({ "Authorization: Bearer <your_token>" });

    // Tell the uploader which AudioThread to pull from.

    // When upload+transcription finishes, type or handle the result.
    connect(&streamingAudioUploader, &StreamingAudioUploader::finished, this,
            [this](QByteArray result) {
                auto elapsed = timer.nsecsElapsed();
                qDebug() << "Request took" << ((double) elapsed) / 1e6 << "ms";
                timer.restart();
                QMutexLocker lock(&keystrokesThread.queueMutex);
                keystrokesThread.pendingStrings.push_back(result);
                keystrokesThread.waitCondition.notify_all();
            }
    );
}

// Destructor
MainWindow::~MainWindow() {
    if (streamingAudioUploader.isRunning()) {
        keystrokesThread.shutdownRequested = true;
        qDebug() << "MainWindow shutting down: stopping uploader thread...";
        streamingAudioUploader.requestInterruption();
        streamingAudioUploader.quit();
        streamingAudioUploader.wait(3000);
    }
}

// User pressed the “start” hot‑key
void MainWindow::startKeyPressed() {
    qDebug() << "Start key pressed";

    // Clear any leftover flags.
    keystrokesThread.shouldHitEnter = false;
    keystrokesThread.shouldBeAction = false;

    // Begin recording and immediately launch upload thread.
    audioThread.startRecording();

    qDebug() << "Audio recording started, uploader thread launched.";
}

// User pressed the “stop (normal)” hot‑key
void MainWindow::stopKeyPressed() {
    if (audioThread.state() == AudioThread::State::Recording) {
        timer.restart();
        qDebug() << "Stop key pressed (Normal)";
        audioThread.stopRecording();
        qDebug() << "Audio recording stopped; uploader will drain remaining data.";
    }
}

// User pressed the “stop + Enter” hot‑key
void MainWindow::stopKeyPressed2() {
    if (audioThread.state() == AudioThread::State::Recording) {
        timer.restart();
        qDebug() << "Stop key pressed (Enter flag)";
        keystrokesThread.shouldHitEnter = true;
        audioThread.stopRecording();
        qDebug() << "Audio recording stopped; uploader will drain remaining data.";
    }
}

// User pressed the “stop + Action” hot‑key
void MainWindow::stopKeyPressed3() {
    if (audioThread.state() == AudioThread::State::Recording) {
        timer.restart();
        qDebug() << "Stop key pressed (Action flag)";
        keystrokesThread.shouldBeAction = true;
        audioThread.stopRecording();
        qDebug() << "Audio recording stopped; uploader will drain remaining data.";
    }
}