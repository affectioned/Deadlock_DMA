#pragma once
#include <chrono>

class DMA_Connection;

// Implement this interface in the game layer and assign g_GameContext before
// launching DMA_Thread_Main. The DMA thread calls Initialize once, then Tick
// every millisecond, then Shutdown on exit.
struct IGameContext
{
    // Called once after the DMA connection is established and keyboard is
    // initialized. Return false to abort — sets bRunning = false.
    virtual bool Initialize(DMA_Connection* conn) = 0;

    // Called every tick (nominally 1 ms). Tick your CTimers here.
    virtual void Tick(DMA_Connection* conn, std::chrono::steady_clock::time_point now) = 0;

    virtual ~IGameContext() = default;
};

// Defined in DMA Thread.cpp. Set by main() before launching DMA_Thread_Main.
extern IGameContext* g_GameContext;
