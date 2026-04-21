#pragma once

#include "DMA/IGameContext.h"
#include "DMA/DMA Thread.h"

// Concrete IGameContext for Deadlock. Owns all per-frame CTimers.
// Instantiate in main() and assign to g_GameContext before launching the DMA thread.
class DeadlockContext : public IGameContext
{
public:
	bool Initialize(DMA_Connection* conn) override;
	void Tick(DMA_Connection* conn, std::chrono::steady_clock::time_point now) override;

private:
	std::vector<Timer> m_Timers;
};
