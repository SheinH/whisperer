//
// Created by meow on 8/9/24.
//

#ifndef WHISPERER_KEYSTROKES_H
#define WHISPERER_KEYSTROKES_H
#include <QThread>
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
// Types text at the cursor of the focused window. Implemented per platform
// in keystrokes_mac.cpp / keystrokes_win.cpp.
void typeString(const QByteArray &text, bool shouldHitEnter, bool shouldBeAction);


#endif //WHISPERER_KEYSTROKES_H
