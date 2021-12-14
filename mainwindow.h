#pragma once

// CrashFirefox! is a GUI tool that is based on Benjamin Smedberg's
// https://github.com/bsmedberg/crashfirefox-intentionally project

#include <Windows.h>
#include <TlHelp32.h>

#include <QMainWindow>
#include <QThread>

#include "thunderbolt.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void slot_set_control_states();
    void slot_crash_it();
    void slot_enumerate_processes();

    void slot_thunderbolt_finished();

private: // data members
    Ui::MainWindow *m_ui;

    /*!
      Title string used for all GUI windows.
     */
    QString m_title;

    /*!
      This tracks how many Thunderbolt threads we have launched.
      Note that it is NOT an atomic because it is only accessed
      from the same thread (main).
     */
    int m_thunderbolt_thread_count{0};
};
