// Copyright (c) 2026 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <shutdownnotification.h>

#include <cstdint>
#include <stdexcept>

#ifndef WIN32
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

// Request() must not take a lock from a signal handler.
static_assert(std::atomic<bool>::is_always_lock_free, "ShutdownNotification needs a lock-free flag");

#ifdef WIN32

ShutdownNotification::ShutdownNotification() = default;
ShutdownNotification::~ShutdownNotification() = default;

void ShutdownNotification::Request()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_requested = true;
    }
    m_cv.notify_all();
}

void ShutdownNotification::Reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_requested = false;
}

void ShutdownNotification::Wait()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return m_requested.load(); });
}

#else

ShutdownNotification::ShutdownNotification()
{
    int fds[2];
    if (pipe(fds) != 0) throw std::runtime_error("Could not create the shutdown notification pipe");
    m_pipe_read = fds[0];
    m_pipe_write = fds[1];
    fcntl(m_pipe_read, F_SETFD, FD_CLOEXEC);
    fcntl(m_pipe_write, F_SETFD, FD_CLOEXEC);
}

ShutdownNotification::~ShutdownNotification()
{
    close(m_pipe_read);
    close(m_pipe_write);
}

void ShutdownNotification::Request()
{
    // Only the first request writes the byte.
    if (m_requested.exchange(true)) return;
    const uint8_t token{0};
    while (write(m_pipe_write, &token, 1) < 0 && errno == EINTR) {}
}

void ShutdownNotification::Reset()
{
    if (!m_requested.exchange(false)) return;
    uint8_t token{0};
    while (read(m_pipe_read, &token, 1) < 0 && errno == EINTR) {}
}

void ShutdownNotification::Wait()
{
    uint8_t token{0};
    while (read(m_pipe_read, &token, 1) < 0 && errno == EINTR) {}
    // Put the byte back: the request stands, for later waiters and for Reset().
    while (write(m_pipe_write, &token, 1) < 0 && errno == EINTR) {}
}

#endif
