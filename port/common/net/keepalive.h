// Keeps the multiplayer sessions serviced while the game loop is suspended (port/docs/MULTIPLAYER.md).
//
// On Android SDL blocks the game thread inside its event pump while the app is in the background
// (SDL_HINT_ANDROID_BLOCK_ON_PAUSE). Without service ENet stops acknowledging, and after its peer
// timeout (5 to 10 s, session.cpp) the other players drop the member and it loses its host. While
// suspended, a thread services every session (ServiceAll) instead, so the connections live on;
// the game itself stays paused, as the original was while its window was inactive.
//
// The game thread holds the loop lock except while it waits for platform events (the host's event
// wait hooks); the service thread takes it for each round of service, so the two never touch a
// session at the same time.
#ifndef BLUPI_NET_KEEPALIVE_H
#define BLUPI_NET_KEEPALIVE_H

namespace blupi::net {
// The game thread: hold the lock while running the loop, release it around event waits. Both are
// no-ops when the calling thread does not, or already does, hold it.
void HoldLoop();
void ReleaseLoop();
// Starts (true) or stops (false, joining it) the service thread. Called on the game thread, between
// ReleaseLoop and HoldLoop (from inside the event wait, as SDL delivers the lifecycle events).
void Suspend(bool suspended);
bool Suspended();
// How many rounds of service the thread has run since the program started (tests).
unsigned long SuspendedRounds();
} // namespace blupi::net
#endif
