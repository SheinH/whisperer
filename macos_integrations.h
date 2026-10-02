//
// Created by meow on 8/9/24.
//

#ifndef WHISPERER_MACOS_INTEGRATIONS_H
#define WHISPERER_MACOS_INTEGRATIONS_H
#include <QWaitCondition>
#include <QMutex>
#include <QQueue>

class KeystrokesThread : public QThread {
public:
    QQueue<QByteArray>   pendingStrings;
    QWaitCondition waitCondition;
    QMutex         queueMutex;
    bool           shutdownRequested = false;
    bool shouldHitEnter;
    bool shouldBeAction;
protected:
    void run() override;
};
void typeString(const QString& text, bool shouldHitEnter, bool shouldBeAction);


#endif //WHISPERER_MACOS_INTEGRATIONS_H
