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

    connect(m_ui->combo_Processes, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &MainWindow::slot_process_changed);

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

void MainWindow::slot_process_changed(int index)
{
    auto data = m_ui->combo_Processes->itemData(index).value<process_data_t>();
    m_ui->edit_CLI->setPlainText(data.second);
}

void MainWindow::slot_set_control_states()
{
    m_ui->button_CrashIt->setEnabled(m_ui->combo_Processes->currentIndex() != 0 && m_thunderbolt_thread_count == 0);
    m_ui->button_Cancel->setEnabled(m_thunderbolt_thread_count == 0);
    m_ui->combo_Processes->setEnabled(m_thunderbolt_thread_count == 0);
    m_ui->check_IncludeSimilar->setEnabled(m_thunderbolt_thread_count == 0);
    m_ui->edit_CLI->setEnabled(m_thunderbolt_thread_count == 0);
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
    auto current_data = m_ui->combo_Processes->currentData().value<process_data_t>();
    auto current_pid{current_data.first};

    auto tb{new Thunderbolt(current_pid, this)};
    connect(tb, &QThread::finished, this, &MainWindow::slot_thunderbolt_finished);
    ++m_thunderbolt_thread_count;
    tb->start();

    if(m_ui->check_IncludeSimilar->isChecked())
    {
        // since the list is sorted, similarly named processes will
        // be in immediate proximity to this one...

        auto str{m_ui->combo_Processes->currentText()};

        static QRegularExpression re("(.+)\\s+\\(\\d+\\)$");
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

            auto data = m_ui->combo_Processes->itemData(index).value<process_data_t>();
            auto tb{new Thunderbolt(data.first, this)};
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

            auto data = m_ui->combo_Processes->itemData(index).value<process_data_t>();
            auto tb{new Thunderbolt(data.first, this)};
            connect(tb, &QThread::finished, this, &MainWindow::slot_thunderbolt_finished);
            ++m_thunderbolt_thread_count;
            tb->start();
        }
    }

    slot_set_control_states();
}

BSTR MainWindow::qt_to_BSTR(const QString &str)
{
    BSTR result = SysAllocStringLen(nullptr, static_cast<UINT>(str.length()));
    str.toWCharArray(result);
    return result;
}

int MainWindow::get_process_info(DWORD pid, QString& commandLine, QString& executable)
{
    HRESULT hr{0};

    auto bstr1 = qt_to_BSTR("ROOT\\CIMV2");
    auto bstr2 = qt_to_BSTR("WQL");
    auto bstr3 = qt_to_BSTR("SELECT ProcessId,CommandLine,ExecutablePath FROM Win32_Process");

    //initializate the Windows security
    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

    IWbemLocator* WbemLocator{nullptr};
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<LPVOID *>(&WbemLocator));

    // connect to the WMI
    IWbemServices* WbemServices{nullptr};
    hr = WbemLocator->ConnectServer(bstr1, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &WbemServices);

    // run the WQL query
    IEnumWbemClassObject* EnumWbem{nullptr};
    hr = WbemServices->ExecQuery(bstr2, bstr3, WBEM_FLAG_FORWARD_ONLY, nullptr, &EnumWbem);

    // iterate over the enumerator
    if(EnumWbem)
    {
        IWbemClassObject *result{nullptr};
        ULONG returnedCount{0};

        while((hr = EnumWbem->Next(WBEM_INFINITE, 1, &result, &returnedCount)) == S_OK)
        {
            VARIANT ProcessId;
            hr = result->Get(L"ProcessId", 0, &ProcessId, nullptr, nullptr);

            if(ProcessId.uintVal == pid)
            {
                // access the properties
                VARIANT CommandLine;
                hr = result->Get(L"CommandLine", 0, &CommandLine, nullptr, nullptr);
                commandLine = QString::fromWCharArray(CommandLine.bstrVal);

                VARIANT ExecutablePath;
                hr = result->Get(L"ExecutablePath", 0, &ExecutablePath, nullptr, nullptr);
                executable = QString::fromWCharArray(ExecutablePath.bstrVal);

                if(!commandLine.compare(executable))
                    commandLine.clear();
            }

            result->Release();

            if(ProcessId.uintVal == pid)
                break;
        }

        EnumWbem->Release();
    }

    // release the resources
    SysFreeString(bstr1);
    SysFreeString(bstr2);
    SysFreeString(bstr3);

    WbemServices->Release();
    WbemLocator->Release();

    CoUninitialize();

    return 0;
}

void MainWindow::slot_enumerate_processes()
{
    m_ui->combo_Processes->clear();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(INVALID_HANDLE_VALUE == snapshot)
    {
        QMessageBox::critical(this, m_title, tr("Could not enumerate Windows processes!"));
        QTimer::singleShot(0, qApp, &QApplication::quit);
    }
    else
    {
        PROCESSENTRY32 pe{sizeof(pe), 0, 0, 0, 0, 0, 0, 0, 0, {0}};

        QSignalBlocker blocker(m_ui->combo_Processes);

        auto font{QFont("Courier New")};
        m_ui->combo_Processes->setFont(font);
        m_ui->edit_CLI->setFont(font);

        int index{0};

        for(BOOL ok = Process32First(snapshot, &pe); ok; ok = Process32Next(snapshot, &pe))
        {
            QString cli;
            QString suffix;

            QString p{QString::fromWCharArray(pe.szExeFile)};
            if(p.toLower().startsWith("firefox"))
            {
                QString executable;
                get_process_info(pe.th32ProcessID, cli, executable);
                if(cli[0] == '"')
                {
                    // strip the executable
                    int l = cli.length();
                    int x = 1;
                    while(cli[x] != '"')
                        ++x;
                    ++x;
                    while(cli[x].isSpace())
                        ++x;
                    cli = cli.right(l - x);
                }

                if(!cli.isEmpty())
                {
                    // get the proces type from the end of the cli
                    int l = cli.length() - 1;
                    int x = l;
                    while(!cli[x].isSpace())
                        --x;
                    suffix = QString(".%1").arg(cli.right(l - x));
                }
            }

            QString p_str{QString("%1%2 (%3)").arg(p).arg(suffix).arg(pe.th32ProcessID)};
            auto data = process_data_t(pe.th32ProcessID, cli);
            m_ui->combo_Processes->addItem(p_str, QVariant::fromValue(data));

            m_ui->combo_Processes->setItemData(index, font, Qt::FontRole);

            if(p.toLower().startsWith("firefox") && !suffix.isEmpty())
            {
                if(!suffix.compare(".gpu"))
                    m_ui->combo_Processes->setItemData(index, QBrush(Qt::darkGreen), Qt::TextColorRole);
                else if(!suffix.compare(".tab"))
                    m_ui->combo_Processes->setItemData(index, QBrush(Qt::darkBlue), Qt::TextColorRole);
                else if(!suffix.compare(".utility"))
                    m_ui->combo_Processes->setItemData(index, QBrush(Qt::darkCyan), Qt::TextColorRole);
                else if(!suffix.compare(".socket"))
                    m_ui->combo_Processes->setItemData(index, QBrush(Qt::darkYellow), Qt::TextColorRole);
                else if(!suffix.compare(".rdd"))
                    m_ui->combo_Processes->setItemData(index, QBrush(Qt::darkMagenta), Qt::TextColorRole);
            }

            ++index;
        }

        m_ui->combo_Processes->model()->sort(0, Qt::AscendingOrder);

        int parent_index{-1};
        for(auto i = 0;i < m_ui->combo_Processes->count();++i)
        {
            auto str{m_ui->combo_Processes->itemText(i)};
            if(str.toLower().startsWith("firefox"))
            {
                auto data = m_ui->combo_Processes->itemData(i).value<process_data_t>();
                if(data.second.isEmpty())
                {
                    parent_index = i;
                    break;
                }
            }
        }

        if(parent_index == -1)
        {
            QMessageBox::critical(this, m_title, tr("Could not identify the Firefox process!"));
            QTimer::singleShot(0, qApp, &QApplication::quit);
        }
        else
        {
            m_ui->combo_Processes->setCurrentIndex(parent_index);
            slot_process_changed(parent_index);
        }
    }

    slot_set_control_states();
}
