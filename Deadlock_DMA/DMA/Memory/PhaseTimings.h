#pragma once
#include "pch.h"

// Phase-timing harness for finding bottlenecks in the DMA loop.
//
// Each "phase" is a named accumulator of elapsed microseconds (sum/avg/max
// over a window). Wrap a unit of work with PHASE_SCOPE("name") and call
// PhaseTimings::DumpAndReset() from a low-frequency timer to log a sorted
// summary and start a fresh window.
//
//   void EntityList::FullPawnRefresh_lk(...) {
//       PHASE_SCOPE("FullPawnRefresh");
//       ...
//   }
//
// For the per-tick CTimers in DeadlockContext, the wrapping is done at lambda
// construction time so the registry lookup happens once per phase, not per
// tick.

// Per-phase microsecond accumulator. `max` surfaces tail stalls that the
// average would otherwise smear over.
struct PhaseUs {
	int64_t sum = 0;
	int64_t max = 0;
	int     samples = 0;
	void add(int64_t us) { sum += us; if (us > max) max = us; ++samples; }
	int64_t avg() const { return samples > 0 ? sum / samples : 0; }
	void reset() { sum = 0; max = 0; samples = 0; }
};

// RAII timer — adds elapsed microseconds to a PhaseUs on destruction. Works
// across `goto`/early-return because destructors fire on any scope exit.
struct ScopedUs {
	std::chrono::steady_clock::time_point start;
	PhaseUs& dst;
	explicit ScopedUs(PhaseUs& d) : start(std::chrono::steady_clock::now()), dst(d) {}
	~ScopedUs() {
		const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - start).count();
		dst.add(us);
	}
};

class PhaseTimings {
public:
	// Look up (or create) the named accumulator. Returned reference is stable
	// for program lifetime — values live on the heap via unique_ptr, so map
	// rehashes don't move them. Call sites can cache the reference.
	static PhaseUs& Get(const std::string& name) {
		std::scoped_lock lk(s_Mutex);
		auto& slot = s_Phases[name];
		if (!slot) slot = std::make_unique<PhaseUs>();
		return *slot;
	}

	// Log every phase that recorded at least one sample (sorted by total
	// descending) then reset the accumulators. Thread-safe.
	static void DumpAndReset() {
		std::scoped_lock lk(s_Mutex);
		struct Row { std::string_view name; PhaseUs* phase; };
		std::vector<Row> rows;
		rows.reserve(s_Phases.size());
		for (auto& [name, p] : s_Phases) {
			if (p->samples == 0) continue;
			rows.push_back({ name, p.get() });
		}
		if (rows.empty()) return;
		std::sort(rows.begin(), rows.end(),
			[](const Row& a, const Row& b) { return a.phase->sum > b.phase->sum; });
		Log::Info("[PhaseTimings] window summary (sorted by total time):");
		for (auto& r : rows) {
			Log::Info("  {:<24} avg={:>6}us max={:>7}us n={:>5} total={:>9}us",
				r.name, r.phase->avg(), r.phase->max, r.phase->samples, r.phase->sum);
			r.phase->reset();
		}
	}

private:
	static inline std::mutex s_Mutex;
	static inline std::unordered_map<std::string, std::unique_ptr<PhaseUs>> s_Phases;
};

#define PHASE_TOKEN_PASTE_INNER(a, b) a##b
#define PHASE_TOKEN_PASTE(a, b) PHASE_TOKEN_PASTE_INNER(a, b)
// One-shot registry lookup per call site (the static reference initializes on
// first hit and is stable forever after). __LINE__ keeps multiple PHASE_SCOPE
// uses in the same function from colliding.
#define PHASE_SCOPE(name)                                                      \
	static PhaseUs& PHASE_TOKEN_PASTE(_phase_, __LINE__) = PhaseTimings::Get(name); \
	ScopedUs PHASE_TOKEN_PASTE(_scoped_, __LINE__)(PHASE_TOKEN_PASTE(_phase_, __LINE__))
