#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <QString>
#include <QDebug>
#include <QThread>
#include "macos_integrations.h"


void typeAChar(QChar c, CGEventSourceRef& source) {
    const auto unicodeChar = static_cast<UniChar>(c.unicode());

    // Create a key down event for the character
    CGEventRef keyDown = CGEventCreateKeyboardEvent(source, 0, true);
    if (keyDown) {
        CGEventKeyboardSetUnicodeString(keyDown, 1, &unicodeChar);
        CGEventPost(kCGAnnotatedSessionEventTap, keyDown);
        CFRelease(keyDown);
    } else {
        qDebug() << "Failed to create key down event for character:" << c;
    }

    // Create a key up event for the character
    CGEventRef keyUp = CGEventCreateKeyboardEvent(source, 0, false);
    if (keyUp) {
//        CGEventKeyboardSetUnicodeString(keyUp, 1, &unicodeChar);
        CGEventPost(kCGAnnotatedSessionEventTap, keyUp);
        CFRelease(keyUp);
    } else {
        qDebug() << "Failed to create key up event for character:" << c;
    }
}

// Function to type a QString character by character at the cursor position
void typeString(const QByteArray &text, bool shouldHitEnter, bool shouldBeAction) {
    // Create a keyboard event source
    CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if (!source) {
        qDebug() << "Failed to create event source.";
        return;
    }

    if (shouldBeAction)
        typeAChar('*',source);

    auto str = QString::fromUtf8(text);
    for(auto c : str) {
        typeAChar(c,source);
    }
    if (shouldBeAction)
        typeAChar('*',source);
    if (shouldHitEnter || shouldBeAction) {

        auto enterKeyDown = CGEventCreateKeyboardEvent(source, kVK_Return, true);
        if (enterKeyDown) {
            CGEventPost(kCGAnnotatedSessionEventTap, enterKeyDown);
            CFRelease(enterKeyDown);
        } else {
            qDebug() << "Failed to create key down event for Enter key";
        }

        CGEventRef enterKeyUp = CGEventCreateKeyboardEvent(source, kVK_Return, false);
        if (enterKeyUp) {
            CGEventPost(kCGAnnotatedSessionEventTap, enterKeyUp);
            CFRelease(enterKeyUp);
        } else {
            qDebug() << "Failed to create key up event for Enter key";
        }
    }

    // Release the event source
    CFRelease(source);
}

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