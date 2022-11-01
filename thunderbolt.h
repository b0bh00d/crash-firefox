#pragma once

#include <Windows.h>

#include <QThread>
#include <QSharedPointer>

class Thunderbolt : public QThread
{
    Q_OBJECT

public: // aliases and enums
    /*!
      These are the possible results of the force-crash operation.
     */
    enum class Result
    {
        None,
        Success,
        CouldNotOpen,
        CouldNotCreate,
    };

public: // methods
    explicit Thunderbolt(DWORD pid, QObject *parent = nullptr) : QThread(parent), m_pid(pid) {}
    ~Thunderbolt() override = default;

    /*!
      Return the result of the thread operation.
      \returns The Thunderbolt::Result enumeration
     */
    Result  result() const { return m_result; }

    /*!
      Return the process id that force-crashed.
      \returns The process id
     */
    DWORD   pid() const { return m_pid; }

private: // data members
    DWORD   m_pid{0};
    Result  m_result{Result::None};

private: // methods
    void run() override;
};
