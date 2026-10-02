//#ifndef AUDIORECORDER_H
//#define AUDIORECORDER_H
//
//#include <QObject>
//#include <QProcess>
//
//class AudioRecorder : public QObject
//{
//    Q_OBJECT
//
//public:
//    explicit AudioRecorder(QObject *parent = nullptr);
//    ~AudioRecorder();
//
//    void startRecording();
//    void stopRecording();
//    QProcess* getProcess();
//    QString getAudioFilePath();
//
//signals:
////    void dataReady(const QByteArray &data);
//
//private slots:
////    void handleProcessOutput();
//    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
//
//private:
//    QProcess *m_process;
//    QString audioFilePath;
//};
//
//#endif // AUDIORECORDER_H
