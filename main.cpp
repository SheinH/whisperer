#include "mainwindow.h"
#include "audiorecorder.h"

#include <QApplication>
#include <QHotkey>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // Gives QSettings the com.sheinhtike.whisperer domain (matches Info.plist).
    QCoreApplication::setOrganizationDomain("sheinhtike.com");
    QCoreApplication::setApplicationName("whisperer");
    MainWindow w;
//    AudioRecorder rec;
    QKeySequence startKeySeq( Qt::ALT | Qt::Key_Z);
    QKeySequence stopKeySeq( Qt::ALT | Qt::Key_X);
    QKeySequence stopKeySeq2( Qt::ALT | Qt::Key_C);
    QKeySequence stopKeySeq3( Qt::ALT | Qt::Key_A);
    QKeySequence ks4( Qt::ALT | Qt::Key_F14);
    QKeySequence ks5( Qt::ALT | Qt::Key_F13);
    QHotkey hotkey(startKeySeq, true, &a); //The hotkey will be automatically registered
    QHotkey hotkey2(stopKeySeq, true, &a); //The hotkey will be automatically registered
    QHotkey hotkey3(stopKeySeq2, true, &a); //The hotkey will be automatically registered
    QHotkey hotkey4(stopKeySeq3, true, &a); //The hotkey will be automatically registered
    QHotkey hotkey5(ks4, true, &a); //The hotkey will be automatically registered
    QHotkey hotkey6(ks5, true, &a); //The hotkey will be automatically registered
    QObject::connect(&hotkey, &QHotkey::activated,  &w,&MainWindow::startKeyPressed);
    QObject::connect(&hotkey2, &QHotkey::activated,  &w,&MainWindow::stopKeyPressed);
    QObject::connect(&hotkey3, &QHotkey::activated,  &w,&MainWindow::stopKeyPressed2);
    QObject::connect(&hotkey4, &QHotkey::activated,  &w,&MainWindow::stopKeyPressed3);
    QObject::connect(&hotkey5, &QHotkey::activated,  &w,&MainWindow::startKeyPressed);
    QObject::connect(&hotkey5, &QHotkey::released,  &w,&MainWindow::stopKeyPressed2);
    QObject::connect(&hotkey6, &QHotkey::activated,  &w,&MainWindow::startKeyPressed);
    QObject::connect(&hotkey6, &QHotkey::released,  &w,&MainWindow::stopKeyPressed);
//    w.show();


    return a.exec();
}
