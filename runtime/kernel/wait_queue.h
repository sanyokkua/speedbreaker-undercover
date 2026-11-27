// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The order the dispatcher hands a semaphore's count or a synchronization
// event's signal out in (kernel/dispatcher.cpp). The NT kernel satisfies the
// threads already waiting inside the release; a thread that starts waiting
// afterwards (most often the releasing thread itself) gets only what is left.
// Our waiters re-check after a notify instead, so without this the releasing
// thread could take its own release back: Half-Blood Prince's render thread
// released a semaphore the main thread waited on and at once waited on it,
// and the main thread waited forever.
//
// Each waiting thread holds a ticket (dispatcher-wide, increasing). A
// wait-any waiter may take a unit when fewer waiters with older tickets than
// its own are queued than there are units. A wait-all waiter is not ordered
// (NT takes nothing for it until all its objects are signalled, so it must
// not hold up the others): it takes when all are signalled, and isn't
// counted ahead of anyone. Header only; dispatcher::Lock() guards it.
//
// Each waiting thread also has its own WaitSignal, and a change to an object
// wakes just the threads queued on it (WakeLocked). One condition variable for
// every wait woke all ~20 guest threads at each release: DH1's main thread
// spent ~45% of a 60 fps frame in NtReleaseMutant's broadcast and in the lock
// the woken threads then fought over (docs/PORTING.md §11).
#pragma once
#include <condition_variable>
#include <cstdint>
#include <vector>

struct WaitSignal
{
    std::condition_variable cv;  // waited on with dispatcher::Lock()
    uint64_t wakes = 0;          // (for tests)

    void Wake()
    {
        wakes++;
        cv.notify_one();
    }
};

struct WaitQueue
{
    struct Entry
    {
        uint64_t ticket;
        bool waitAll;
        WaitSignal* signal;
    };
    std::vector<Entry> entries;  // in ticket order (tickets only grow)

    void Add(uint64_t ticket, bool waitAll, WaitSignal* signal = nullptr) { entries.push_back({ ticket, waitAll, signal }); }

    // The object changed: every thread queued on it re-checks.
    void WakeLocked() const
    {
        for (const Entry& e : entries)
            if (e.signal != nullptr)
                e.signal->Wake();
    }

    void Remove(uint64_t ticket)
    {
        for (size_t i = 0; i < entries.size(); i++)
        {
            if (entries[i].ticket == ticket)
            {
                entries.erase(entries.begin() + i);
                return;
            }
        }
    }

    // Wait-any waiters queued with an older ticket.
    uint32_t AheadOf(uint64_t ticket) const
    {
        uint32_t n = 0;
        for (const Entry& e : entries)
        {
            if (e.ticket >= ticket)
                break;
            n += e.waitAll ? 0 : 1;
        }
        return n;
    }

    // units: what the object can give right now (a semaphore's count, 1 for a
    // signalled synchronization event); < 0: not handed out in order (a
    // notification event, a mutant, a thread, a timer).
    bool MayTake(uint64_t ticket, bool waitAll, int32_t units) const
    {
        if (units < 0 || waitAll)
            return true;
        return AheadOf(ticket) < uint32_t(units);
    }
};
