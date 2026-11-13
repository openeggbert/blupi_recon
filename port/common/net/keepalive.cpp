#include "net/keepalive.h"
#include "net/session.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace blupi::net {
namespace {
std::mutex loopLock;
thread_local bool holding = false;
std::thread service;
std::atomic<bool> stop{false};
std::atomic<unsigned long> rounds{0};
bool running = false;   // game thread only

void ServiceWhileSuspended()
{
    while (!stop.load()) {
        {
            std::lock_guard<std::mutex> guard(loopLock);
            ServiceAll();
        }
        rounds.fetch_add(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
} // namespace

void HoldLoop()
{
    if (holding) return;
    loopLock.lock();
    holding = true;
}

void ReleaseLoop()
{
    if (!holding) return;
    holding = false;
    loopLock.unlock();
}

void Suspend(bool suspended)
{
#ifdef __EMSCRIPTEN__
    // A browser never blocks the page's loop inside an event wait; a background tab only slows it.
    (void)suspended;
#else
    if (suspended == running) return;
    if (suspended) {
        stop.store(false);
        service = std::thread(ServiceWhileSuspended);
    } else {
        stop.store(true);
        // The game thread must not hold the lock here, or the service round in progress could not end.
        const bool held = holding;
        ReleaseLoop();
        service.join();
        if (held) HoldLoop();
    }
    running = suspended;
#endif
}

bool Suspended() { return running; }
unsigned long SuspendedRounds() { return rounds.load(); }
} // namespace blupi::net
