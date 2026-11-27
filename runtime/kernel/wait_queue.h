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
#pragma once
#include <cstdint>
#include <vector>

struct WaitQueue
{
    struct Entry
    {
        uint64_t ticket;
        bool waitAll;
    };
    std::vector<Entry> entries;  // in ticket order (tickets only grow)

    void Add(uint64_t ticket, bool waitAll) { entries.push_back({ ticket, waitAll }); }

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
