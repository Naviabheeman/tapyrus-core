// Copyright (c) 2026 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TAPYRUS_SHUTDOWNNOTIFICATION_H
#define TAPYRUS_SHUTDOWNNOTIFICATION_H

#include <atomic>
#ifdef WIN32
#include <condition_variable>
#include <mutex>
#endif

/**
 * Whether the node has been asked to shut down, and a way to wait for that.
 * tapyrusd and tapyrus-qt each own one for the life of the process; the node's
 * NotificationHub passes every shutdown request on to it, and long-running
 * loops check IsRequested().
 *
 * On POSIX a pipe holds one byte while a shutdown is requested, so Request()
 * only sets a flag and writes, and a signal handler may call it.
 */
class ShutdownNotification
{
public:
    ShutdownNotification();
    ~ShutdownNotification();
    ShutdownNotification(const ShutdownNotification&) = delete;
    ShutdownNotification& operator=(const ShutdownNotification&) = delete;

    //! Asks the node to shut down; async-signal-safe on POSIX.
    void Request();
    //! Withdraws the request, when start-up retries after the user chose to.
    void Reset();
    bool IsRequested() const { return m_requested.load(); }
    //! Blocks until a shutdown is requested.
    void Wait();

private:
    std::atomic<bool> m_requested{false};
#ifdef WIN32
    std::mutex m_mutex;
    std::condition_variable m_cv;
#else
    int m_pipe_read{-1};
    int m_pipe_write{-1};
#endif
};

#endif // TAPYRUS_SHUTDOWNNOTIFICATION_H
