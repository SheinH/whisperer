#include "keystrokes.h"

void KeystrokesThread::run()
{
    for (;;) {

        {
            QMutexLocker locker(&queueMutex);

            if (shutdownRequested)
                break;

            while (pendingStrings.isEmpty() && !shutdownRequested) {
                waitCondition.wait(&queueMutex,500);
            }

            if (shutdownRequested)
                break;

            typeString(pendingStrings.first(),shouldHitEnter,shouldBeAction);
            shouldBeAction = false;
            shouldHitEnter = false;
            pendingStrings.takeFirst();
        }
    }

}
