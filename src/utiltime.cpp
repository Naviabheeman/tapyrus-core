// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2019 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <utiltime.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <tinyformat.h>

static std::atomic<int64_t> nMockTime(0); //!< For unit testing

int64_t GetTime()
{
    int64_t mocktime = nMockTime.load(std::memory_order_relaxed);
    if (mocktime) return mocktime;

    time_t now = time(nullptr);
    assert(now > 0);
    return now;
}

void SetMockTime(int64_t nMockTimeIn)
{
    nMockTime.store(nMockTimeIn, std::memory_order_relaxed);
}

int64_t GetMockTime()
{
    return nMockTime.load(std::memory_order_relaxed);
}

template <typename T>
static T GetSystemTime()
{
    const auto now = std::chrono::duration_cast<T>(std::chrono::system_clock::now().time_since_epoch());
    assert(now.count() > 0);
    return now;
}

int64_t GetTimeMillis()
{
    return int64_t{GetSystemTime<std::chrono::milliseconds>().count()};
}

int64_t GetTimeMicros()
{
    return int64_t{GetSystemTime<std::chrono::microseconds>().count()};
}

int64_t GetSystemTimeInSeconds()
{
    return GetTimeMicros()/1000000;
}

void MilliSleep(int64_t n)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(n));
}

std::string FormatISO8601DateTime(int64_t nTime) {
    struct tm ts;
    time_t time_val = nTime;
#ifdef _MSC_VER
    gmtime_s(&ts, &time_val);
#else
    gmtime_r(&time_val, &ts);
#endif
    return strprintf("%04i-%02i-%02iT%02i:%02i:%02iZ", ts.tm_year + 1900, ts.tm_mon + 1, ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec);
}

std::string FormatISO8601Date(int64_t nTime) {
    struct tm ts;
    time_t time_val = nTime;
#ifdef _MSC_VER
    gmtime_s(&ts, &time_val);
#else
    gmtime_r(&time_val, &ts);
#endif
    return strprintf("%04i-%02i-%02i", ts.tm_year + 1900, ts.tm_mon + 1, ts.tm_mday);
}

std::string FormatISO8601Time(int64_t nTime) {
    struct tm ts;
    time_t time_val = nTime;
#ifdef _MSC_VER
    gmtime_s(&ts, &time_val);
#else
    gmtime_r(&time_val, &ts);
#endif
    return strprintf("%02i:%02i:%02iZ", ts.tm_hour, ts.tm_min, ts.tm_sec);
}
