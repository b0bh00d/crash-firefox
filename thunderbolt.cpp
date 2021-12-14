#include <QMessageBox>

#include "thunderbolt.h"

Thunderbolt::Thunderbolt(DWORD pid, QObject *parent)
    : QThread(parent), m_pid(pid)
{
}

//Thunderbolt::~Thunderbolt()
//{
//}

void Thunderbolt::run()
{
    m_result = Result::Success;

    auto targetProc{OpenProcess(PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION, FALSE, m_pid)};
    if (!targetProc)
        m_result = Result::CouldNotOpen;
    else
    {
        // Create a thread that starts at 0x0 and should immediately crash
        auto hThread(CreateRemoteThread(targetProc, nullptr, 0, nullptr, nullptr, 0, nullptr));
        if (!hThread)
        {
            m_result = Result::CouldNotCreate;
            CloseHandle(targetProc);
        }
        else
        {
            WaitForSingleObject(hThread, INFINITE);
            CloseHandle(hThread);
            CloseHandle(targetProc);
        }
    }
}
