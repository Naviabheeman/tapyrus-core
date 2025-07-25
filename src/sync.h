// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SYNC_H
#define BITCOIN_SYNC_H

#include <threadsafety.h>

#include <condition_variable>
#include <thread>
#include <mutex>


////////////////////////////////////////////////
//                                            //
// THE SIMPLE DEFINITION, EXCLUDING DEBUG CODE //
//                                            //
////////////////////////////////////////////////

/*
Mutex mutex;
    std::recursive_mutex mutex;

LOCK(mutex);
    std::unique_lock<std::recursive_mutex> criticalblock(mutex);

LOCK2(mutex1, mutex2);
    std::unique_lock<std::recursive_mutex> criticalblock1(mutex1);
    std::unique_lock<std::recursive_mutex> criticalblock2(mutex2);

TRY_LOCK(mutex, name);
    std::unique_lock<std::recursive_mutex> name(mutex, std::try_to_lock_t);

ENTER_CRITICAL_SECTION(mutex); // no RAII
    mutex.lock();

LEAVE_CRITICAL_SECTION(mutex); // no RAII
    mutex.unlock();
 */

///////////////////////////////
//                           //
// THE ACTUAL IMPLEMENTATION //
//                           //
///////////////////////////////

/**
 * Template mixin that adds -Wthread-safety locking
 * annotations to a subset of the mutex API.
 */
template <typename PARENT>
class LOCKABLE AnnotatedMixin : public PARENT
{
public:
    void lock() EXCLUSIVE_LOCK_FUNCTION()
    {
        PARENT::lock();
    }

    void unlock() UNLOCK_FUNCTION()
    {
        PARENT::unlock();
    }

    bool try_lock() EXCLUSIVE_TRYLOCK_FUNCTION(true)
    {
        return PARENT::try_lock();
    }

    using UniqueLock = std::unique_lock<PARENT>;

#ifdef __clang__
    //! For negative capabilities in the Clang Thread Safety Analysis.
    //! A negative requirement uses the EXCLUSIVE_LOCKS_REQUIRED attribute, in conjunction
    //! with the ! operator, to indicate that a mutex should not be held.
    const AnnotatedMixin& operator!() const { return *this; }
#endif // __clang__
};

#ifdef DEBUG_LOCKORDER
void EnterCritical(const char* pszName, const char* pszFile, int nLine, void* cs, bool fTry = false);
void LeaveCritical();
void CheckLastCritical(void* cs, std::string& lockname, const char* guardname, const char* file, int line);
std::string LocksHeld();
template <typename MutexType>
void AssertLockHeldInternal(const char* pszName, const char* pszFile, int nLine, MutexType* cs) ASSERT_EXCLUSIVE_LOCK(cs);
template <typename MutexType>
void AssertLockNotHeldInternal(const char* pszName, const char* pszFile, int nLine, MutexType* cs) LOCKS_EXCLUDED(cs);
void DeleteLock(void* cs);
#else
void inline EnterCritical(const char* pszName, const char* pszFile, int nLine, void* cs, bool fTry = false) {}
void inline LeaveCritical() {}
void inline CheckLastCritical(void* cs, std::string& lockname, const char* guardname, const char* file, int line) {}
template <typename MutexType>
void inline AssertLockHeldInternal(const char* pszName, const char* pszFile, int nLine, MutexType* cs) ASSERT_EXCLUSIVE_LOCK(cs) {}
template <typename MutexType>
void inline AssertLockNotHeldInternal(const char* pszName, const char* pszFile, int nLine, MutexType* cs) LOCKS_EXCLUDED(cs) {}
void inline DeleteLock(void* cs) {}
#endif
#define AssertLockHeld(cs) AssertLockHeldInternal(#cs, __FILE__, __LINE__, &cs)
#define AssertLockNotHeld(cs) AssertLockNotHeldInternal(#cs, __FILE__, __LINE__, &cs)

/**
 * Wrapped mutex: supports recursive locking, but no waiting
 * TODO: We should move away from using the recursive lock by default.
 */
using RecursiveMutex = AnnotatedMixin<std::recursive_mutex>;

/** Wrapped mutex: supports waiting but not recursive locking */
using Mutex = AnnotatedMixin<std::mutex>;

/** Just a typedef for std::condition_variable, can be wrapped later if desired */
typedef std::condition_variable CConditionVariable;

/** Just a typedef for std::unique_lock, can be wrapped later if desired */
typedef std::unique_lock<std::mutex> WaitableLock;

#ifdef DEBUG_LOCKCONTENTION
void PrintLockContention(const char* pszName, const char* pszFile, int nLine);
#endif

/** Wrapper around std::unique_lock<Mutex> */
template <typename MutexType>
class SCOPED_LOCKABLE CCriticalBlock
{
private:
    std::unique_lock<MutexType> lock;

    void Enter(const char* pszName, const char* pszFile, int nLine)
    {
        EnterCritical(pszName, pszFile, nLine, (void*)(lock.mutex()));
#ifdef DEBUG_LOCKCONTENTION
        if (!lock.try_lock()) {
            PrintLockContention(pszName, pszFile, nLine);
#endif
            lock.lock();
#ifdef DEBUG_LOCKCONTENTION
        }
#endif
    }

    bool TryEnter(const char* pszName, const char* pszFile, int nLine)
    {
        EnterCritical(pszName, pszFile, nLine, (void*)(lock.mutex()), true);
        if (!lock.try_lock())
            LeaveCritical();
        return lock.owns_lock();
    }

public:
    CCriticalBlock(MutexType& mutexIn, const char* pszName, const char* pszFile, int nLine, bool fTry = false) EXCLUSIVE_LOCK_FUNCTION(mutexIn) : lock(mutexIn, std::defer_lock)
    {
        if (fTry)
            TryEnter(pszName, pszFile, nLine);
        else
            Enter(pszName, pszFile, nLine);
    }

    CCriticalBlock(MutexType* pmutexIn, const char* pszName, const char* pszFile, int nLine, bool fTry = false) EXCLUSIVE_LOCK_FUNCTION(pmutexIn)
    {
        if (!pmutexIn) return;

        lock = std::unique_lock<MutexType>(*pmutexIn, std::defer_lock);
        if (fTry)
            TryEnter(pszName, pszFile, nLine);
        else
            Enter(pszName, pszFile, nLine);
    }

    ~CCriticalBlock() UNLOCK_FUNCTION()
    {
        if (lock.owns_lock())
            LeaveCritical();
    }

    operator bool()
    {
        return lock.owns_lock();
    }
};

#define PASTE(x, y) x ## y
#define PASTE2(x, y) PASTE(x, y)

#define LOCK(cs) CCriticalBlock PASTE2(criticalblock, __COUNTER__)(cs, #cs, __FILE__, __LINE__)
#define LOCK2(cs1, cs2) CCriticalBlock criticalblock1(cs1, #cs1, __FILE__, __LINE__), criticalblock2(cs2, #cs2, __FILE__, __LINE__)
#define TRY_LOCK(cs, name) CCriticalBlock name(cs, #cs, __FILE__, __LINE__, true)
#define WAIT_LOCK(cs, name) CCriticalBlock name(cs, #cs, __FILE__, __LINE__)

#define ENTER_CRITICAL_SECTION(cs)                            \
    {                                                         \
        EnterCritical(#cs, __FILE__, __LINE__, (&cs));        \
        (cs).lock();                                          \
    }

#define LEAVE_CRITICAL_SECTION(cs) \
    {                              \
        (cs).unlock();                                                      \
        LeaveCritical();                                                    \
    }

class CSemaphore
{
private:
    std::condition_variable condition;
    std::mutex mutex;
    int value;

public:
    explicit CSemaphore(int init) : value(init) {}

    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [&]() { return value >= 1; });
        value--;
    }

    bool try_wait()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (value < 1)
            return false;
        value--;
        return true;
    }

    void post()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            value++;
        }
        condition.notify_one();
    }
};

/** RAII-style semaphore lock */
class CSemaphoreGrant
{
private:
    CSemaphore* sem;
    bool fHaveGrant;

public:
    void Acquire()
    {
        if (fHaveGrant)
            return;
        sem->wait();
        fHaveGrant = true;
    }

    void Release()
    {
        if (!fHaveGrant)
            return;
        sem->post();
        fHaveGrant = false;
    }

    bool TryAcquire()
    {
        if (!fHaveGrant && sem->try_wait())
            fHaveGrant = true;
        return fHaveGrant;
    }

    void MoveTo(CSemaphoreGrant& grant)
    {
        grant.Release();
        grant.sem = sem;
        grant.fHaveGrant = fHaveGrant;
        fHaveGrant = false;
    }

    CSemaphoreGrant() : sem(nullptr), fHaveGrant(false) {}

    explicit CSemaphoreGrant(CSemaphore& sema, bool fTry = false) : sem(&sema), fHaveGrant(false)
    {
        if (fTry)
            TryAcquire();
        else
            Acquire();
    }

    ~CSemaphoreGrant()
    {
        Release();
    }

    operator bool() const
    {
        return fHaveGrant;
    }
};

#endif // BITCOIN_SYNC_H
