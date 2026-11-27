// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// kernel/wait_queue.h: a semaphore's count or a synchronization event's
// signal goes to the threads already waiting, oldest first, before a thread
// that starts waiting later (the NT kernel satisfies the waiters inside the
// release). Half-Blood Prince's render thread released a semaphore the main
// thread waited on and at once waited on it itself: taking its own release
// back left the main thread waiting forever. A wait-all waiter doesn't hold
// the others up. A change wakes just the threads queued on that object (each
// has its own WaitSignal). Header only.
#include <kernel/wait_queue.h>

#include <cstdio>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main()
{
    {
        printf("HBP: the releasing thread waits after the main thread\n");
        WaitQueue q;
        q.Add(1, false);  // main thread, waiting
        q.Add(2, false);  // render thread, waits right after its release of 1
        CHECK(q.MayTake(1, false, 1), "the main thread gets the release");
        CHECK(!q.MayTake(2, false, 1), "the releasing thread took its own release back");
        q.Remove(1);      // main took it (count back to 0)
        CHECK(!q.MayTake(2, false, 0), "nothing left");
        CHECK(q.MayTake(2, false, 1), "the next release is the render thread's");
    }
    {
        printf("two units, three waiters\n");
        WaitQueue q;
        q.Add(5, false);
        q.Add(6, false);
        q.Add(9, false);
        CHECK(q.MayTake(5, false, 2) && q.MayTake(6, false, 2), "the two oldest");
        CHECK(!q.MayTake(9, false, 2), "the newest waits");
    }
    {
        printf("wait-all waiters are not ahead of anyone\n");
        WaitQueue q;
        q.Add(1, true);   // waits for the semaphore and an unset event
        q.Add(2, false);
        CHECK(q.MayTake(2, false, 1), "held up by a wait-all that can't finish");
        CHECK(q.MayTake(1, true, 1), "a wait-all takes when all are signalled");
    }
    {
        printf("unordered objects and an empty queue\n");
        WaitQueue q;
        CHECK(q.MayTake(3, false, 1), "no one waiting: a poll takes it");
        q.Add(1, false);
        q.Add(2, false);
        CHECK(q.MayTake(2, false, -1), "notification events, mutants: everyone re-checks");
        q.Remove(7);
        CHECK(q.entries.size() == 2, "removing a ticket that isn't queued");
        q.Remove(1);
        CHECK(q.AheadOf(2) == 0, "ahead %u", q.AheadOf(2));
    }
    {
        printf("a change wakes only the threads queued on that object\n");
        // DH1 at 60 fps: one condition variable for every wait woke all ~20
        // guest threads at each NtReleaseMutant.
        WaitSignal a, b, c;
        WaitQueue mutant, event;
        mutant.Add(1, false, &a);
        event.Add(2, false, &b);
        event.Add(3, true, &c);   // a wait-all on the event and the mutant
        mutant.Add(3, true, &c);
        mutant.WakeLocked();
        CHECK(a.wakes == 1 && c.wakes == 1, "the mutant's waiters: %llu %llu", (unsigned long long)a.wakes, (unsigned long long)c.wakes);
        CHECK(b.wakes == 0, "the event's waiter was woken");
        event.WakeLocked();
        CHECK(b.wakes == 1 && c.wakes == 2 && a.wakes == 1, "the event's waiters");
        mutant.Remove(1);
        mutant.WakeLocked();
        CHECK(a.wakes == 1, "a thread that left the queue was woken");
        WaitQueue none;
        none.Add(4, false);  // no signal (an older caller): nothing to wake
        none.WakeLocked();
    }
    printf(g_failures ? "%d FAILED\n" : "ok\n", g_failures);
    return g_failures ? 1 : 0;
}
