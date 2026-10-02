//#include "audiorecorder.h"
//#include <QDebug>
//#include <QTemporaryFile>
//#include <QDir>
//
//AudioRecorder::AudioRecorder(QObject *parent)
//        : QObject(parent), m_process(new QProcess(this)) {
////    connect(m_process, &QProcess::readyReadStandardOutput, this, &AudioRecorder::handleProcessOutput);
//    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
//            this, &AudioRecorder::handleProcessFinished);
//
//}
//
//AudioRecorder::~AudioRecorder() {
//    stopRecording();
//}
//
//void AudioRecorder::startRecording() {
////    QTemporaryFile tempFile(QDir::tempPath() + "/XXXXXX.webm");
////    audioFilePath = tempFile.fileName();
////    audioFilePath = QDir::tempPath() + "audio.webm";
//    static const QStringList arguments = {
//            "-y",
//            "-hide_banner",
//
//            // Input options - placed before -i
//            "-f", "avfoundation",
//            "-probesize", "32",             // Lower probesize for faster startup
//            "-analyzeduration", "10000",    // Lower analyzeduration for faster startup
//            "-fflags", "nobuffer",          // Attempt to reduce input buffering
////            "-loglevel", "quiet",
//
//            // Input source
//            "-i", ":MacBook Pro Microphone",
//
//            // Output options
//            "-ac", "1",
//            "-ar", "16000",
//            "-b:a", "64k",                 // Bitrate (Consider if lower is acceptable, e.g., 64k, 32k for speech)
////            "-f", "flac",
//            "-acodec", "libopus",           // Explicitly specify opus codec (good practice)
//            "-f","webm",
//            "-application", "lowdelay",// Opus low latency mode
//            "-compression_level", "0",      // Opus fastest compression
//            "-flush_packets", "1",          // Flush packets to stdout immediately
//            "-"
//    };
//    m_process->start(QStringLiteral("/opt/homebrew/bin/ffmpeg"), arguments);
//
//    if (!m_process->waitForStarted()) {
//        qDebug() << "Failed to start ffmpeg process";
//    }
//}
//
//void AudioRecorder::stopRecording() {
//    if (m_process->state() == QProcess::Running) {
//        qDebug() << "Writing a Q";
//        m_process->write("q");
//        m_process->closeWriteChannel();
//    }
//}
//
////void AudioRecorder::handleProcessOutput()
////{
////    QByteArray data = m_process->readAllStandardOutput();
////    emit dataReady(data);
////}
//
//void AudioRecorder::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
//    qDebug() << "FFmpeg process finished with exit code:" << exitCode
//             << "and exit status:" << exitStatus;
//    qDebug() << "std error";
//    qDebug() << m_process->readAllStandardError();
//}
//
//QProcess *AudioRecorder::getProcess() {
//    return m_process;
//}
//
//QString AudioRecorder::getAudioFilePath() {
//    return audioFilePath;
//}
//
