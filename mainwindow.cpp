#include <QTimer>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_ui(new Ui::MainWindow)
{
    m_ui->setupUi(this);

    m_title = tr("Crash Firefox!");
    setWindowTitle(m_title);

    connect(m_ui->combo_Processes, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, &MainWindow::slot_set_control_states);
    connect(m_ui->button_CrashIt, &QPushButton::clicked, this, &MainWindow::slot_crash_it);
    connect(m_ui->button_Cancel, &QPushButton::clicked, qApp, &QApplication::quit);

    slot_set_control_states();

    // keep the window from being resized
    setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);

    // populate the process combo
    QTimer::singleShot(0, this, &MainWindow::slot_enumerate_processes);
}

MainWindow::~MainWindow()
{
    delete m_ui;
}

void MainWindow::slot_set_control_states()
{
    m_ui->button_CrashIt->setEnabled(m_ui->combo_Processes->currentIndex() != 0);
}

void MainWindow::slot_thunderbolt_finished()
{
    Thunderbolt *thunderbolt = qobject_cast<Thunderbolt*>(sender());
    if(thunderbolt)
    {
        switch(thunderbolt->result())
        {
            case Thunderbolt::Result::Success:
                break;
            case Thunderbolt::Result::CouldNotOpen:
                QMessageBox::critical(this, m_title, tr("Could not open target process id %1!").arg(thunderbolt->pid()));
                break;
            case Thunderbolt::Result::CouldNotCreate:
                QMessageBox::critical(this, m_title, tr("Could not create remote thread on process id %1!").arg(thunderbolt->pid()));
                break;
            default:
                break;
        }

        thunderbolt->quit();
        thunderbolt->wait();
        thunderbolt->deleteLater();

        if(--m_thunderbolt_thread_count == 0)
            // rebuild the process list (this may terminate the program)
            QTimer::singleShot(1000, this, &MainWindow::slot_enumerate_processes);
    }
}

void MainWindow::slot_crash_it()
{
    auto current_index{m_ui->combo_Processes->currentIndex()};
    auto current_pid{static_cast<DWORD>(m_ui->combo_Processes->currentData().toInt())};

    auto tb{new Thunderbolt(current_pid, this)};
    connect(tb, &QThread::finished, this, &MainWindow::slot_thunderbolt_finished);
    ++m_thunderbolt_thread_count;
    tb->start();

    if(m_ui->check_IncludeSimilar->isChecked())
    {
        // since the list is sorted, similarly named processes will
        // be in immediate proximity to this one...

        auto str{m_ui->combo_Processes->currentText()};

        QRegularExpression re("(.+)\\s+\\(\\d+\\)$");
        QRegularExpressionMatch match{re.match(str)};
        assert(match.hasMatch());

        auto process_name{match.captured(1)};

        // look backwards...
        auto index{current_index};
        for (;;) {
            if(--index < 0)
                break;

            str = m_ui->combo_Processes->itemText(index);
            if(!str.startsWith(process_name))
                break;

            auto pid{static_cast<DWORD>(m_ui->combo_Processes->itemData(index).toInt())};
            auto tb{new Thunderbolt(pid, this)};
            connect(tb, &QThread::finished, this, &MainWindow::slot_thunderbolt_finished);
            ++m_thunderbolt_thread_count;
            tb->start();
        }

        // look forwards...
        index = current_index;
        for (;;) {
            if(++index >= m_ui->combo_Processes->count())
                break;

            str = m_ui->combo_Processes->itemText(index);
            if(!str.startsWith(process_name))
                break;

            auto pid{static_cast<DWORD>(m_ui->combo_Processes->itemData(index).toInt())};
            auto tb{new Thunderbolt(pid, this)};
            connect(tb, &QThread::finished, this, &MainWindow::slot_thunderbolt_finished);
            ++m_thunderbolt_thread_count;
            tb->start();
        }
    }
}

void MainWindow::slot_enumerate_processes()
{
    m_ui->combo_Processes->clear();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snapshot)
    {
        QMessageBox::critical(this, m_title, tr("Could not enumerate Windows processes!"));
        QTimer::singleShot(0, qApp, &QApplication::quit);
    }
    else
    {
        PROCESSENTRY32 pe{sizeof(pe), 0, 0, 0, 0, 0, 0, 0, 0, {0}};

        QSignalBlocker blocker(m_ui->combo_Processes);

        for(BOOL ok = Process32First(snapshot, &pe); ok; ok = Process32Next(snapshot, &pe))
        {
            QString p{QString::fromWCharArray(pe.szExeFile)};
            QString p_str{QString("%1 (%2)").arg(p).arg(pe.th32ProcessID)};
            m_ui->combo_Processes->addItem(p_str, QVariant::fromValue(pe.th32ProcessID));
        }

        m_ui->combo_Processes->model()->sort(0, Qt::AscendingOrder);

        // find the lowest pid firefox entry, as that is likely
        // the process group leader...

        int ff_index{0};
        DWORD lowest_pid{999999999};

        QRegularExpression re(".+\\s+\\((\\d+)\\)$");

        for(auto i = 0;i < m_ui->combo_Processes->count();++i)
        {
            auto str{m_ui->combo_Processes->itemText(i)};

            QRegularExpressionMatch match{re.match(str)};
            assert(match.hasMatch());

            if(str.toLower().startsWith("firefox"))
            {
                auto pid{static_cast<DWORD>(match.captured(1).toInt())};
                if(pid < lowest_pid)
                {
                    ff_index = i;
                    lowest_pid = pid;
                }
            }
        }

        if (ff_index == 0)
        {
            QMessageBox::critical(this, m_title, tr("Could not identify the Firefox process!"));
            QTimer::singleShot(0, qApp, &QApplication::quit);
        }
        else
        {
            m_ui->combo_Processes->setCurrentIndex(ff_index);
            slot_set_control_states();
        }
    }
}
