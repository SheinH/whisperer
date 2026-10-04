#include <windows.h>
#include <QString>
#include <QDebug>
#include <QVector>
#include "keystrokes.h"


// Queue a key down + key up pair. With KEYEVENTF_UNICODE the character is
// delivered as-is, independent of the active keyboard layout.
static void pushKey(QVector<INPUT> &inputs, WORD vk, WORD scan, DWORD flags) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.wScan = scan;
    input.ki.dwFlags = flags;
    inputs.push_back(input);
    input.ki.dwFlags = flags | KEYEVENTF_KEYUP;
    inputs.push_back(input);
}

static void pushChar(QVector<INPUT> &inputs, QChar c) {
    pushKey(inputs, 0, c.unicode(), KEYEVENTF_UNICODE);
}

// Function to type a QString character by character at the cursor position
void typeString(const QByteArray &text, bool shouldHitEnter, bool shouldBeAction) {
    QVector<INPUT> inputs;

    if (shouldBeAction)
        pushChar(inputs, '*');

    const auto str = QString::fromUtf8(text);
    for (auto c : str) {
        pushChar(inputs, c);
    }
    if (shouldBeAction)
        pushChar(inputs, '*');
    if (shouldHitEnter || shouldBeAction)
        pushKey(inputs, VK_RETURN, 0, 0);

    if (inputs.isEmpty())
        return;

    const UINT sent = SendInput(inputs.size(), inputs.data(), sizeof(INPUT));
    if (sent != static_cast<UINT>(inputs.size()))
        qDebug() << "SendInput only sent" << sent << "of" << inputs.size() << "events";
}
