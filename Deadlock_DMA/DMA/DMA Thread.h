#pragma once

#include "IGameContext.h"

template <typename T, typename F>
class CTimer
{
public:
	CTimer(T _Interval, F _Function) : m_Interval(_Interval), m_Function(_Function) {}

	void Tick(std::chrono::steady_clock::time_point CurrentTime)
	{
		if (std::chrono::duration_cast<T>(CurrentTime - m_LastExecutionTime) >= m_Interval)
		{
			m_Function();
			m_LastExecutionTime = CurrentTime;
		}
	}
private:
	T m_Interval{};
	F m_Function{};
	std::chrono::steady_clock::time_point m_LastExecutionTime{};
};

// Type-erased timer for storing mixed-interval timers in a container.
// All intervals are expressed in milliseconds.
using Timer = CTimer<std::chrono::milliseconds, std::function<void()>>;

void DMA_Thread_Main();
