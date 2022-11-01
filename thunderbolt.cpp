#include <QMessageBox>

#include "thunderbolt.h"

void Thunderbolt::run()
{
    m_result = Result::Success;

    auto targetProc{OpenProcess(PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION, FALSE, m_pid)};
    if (!targetProc)
        m_result = Result::CouldNotOpen;
    else
    {
        // Create a thread that starts at 0x0 and should immediately crash
#if defined(CRASH_WITH_0x0)
        auto hThread = CreateRemoteThread(targetProc, nullptr, 0, nullptr, nullptr, 0, nullptr);
#else
        auto rHandle = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleA("NtDll.dll"), "DbgBreakPoint"));
        assert(rHandle);
        auto hThread = CreateRemoteThread(targetProc, nullptr, 0,
            rHandle,
            nullptr, 0, nullptr);
#endif

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
