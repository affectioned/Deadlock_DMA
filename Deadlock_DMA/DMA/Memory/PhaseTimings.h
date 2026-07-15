#pragma once
#include "pch.h"

#include "DMA/Memory/ScatterRead.h"

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
// For phases dominated by DMA scatter reads, use SCATTER_SCOPE("name", sr)
// instead — it captures bytes-scattered and range-count in addition to
// elapsed microseconds, so the dump answers "how big was the batch?" not
// just "how long did it take?".
//
// For the per-tick CTimers in DeadlockContext, the wrapping is done at lambda
// construction time so the registry lookup happens once per phase, not per
// tick.

// Per-phase microsecond accumulator. `max` surfaces tail stalls that the
// average would otherwise smear over. The bytes/ranges fields are populated
// only by SCATTER_SCOPE — pure-CPU phases via PHASE_SCOPE leave them at 0
// and the dump omits the scatter columns for those rows.
struct PhaseUs {
	int64_t sum = 0;
	int64_t max = 0;
	int     samples = 0;
	int64_t bytes_sum  = 0;
	int64_t bytes_max  = 0;
	int64_t ranges_sum = 0;
	int64_t ranges_max = 0;
	void add(int64_t us) { sum += us; if (us > max) max = us; ++samples; }
	void addScatter(int64_t us, int64_t bytes, int64_t ranges)
	{
		add(us);
		bytes_sum  += bytes;  if (bytes  > bytes_max)  bytes_max  = bytes;
		ranges_sum += ranges; if (ranges > ranges_max) ranges_max = ranges;
	}
	int64_t avg()       const { return samples > 0 ? sum        / samples : 0; }
	int64_t bytesAvg()  const { return samples > 0 ? bytes_sum  / samples : 0; }
	int64_t rangesAvg() const { return samples > 0 ? ranges_sum / samples : 0; }
	void reset()
	{
		sum = 0; max = 0; samples = 0;
		bytes_sum = 0; bytes_max = 0;
		ranges_sum = 0; ranges_max = 0;
	}
};

// RAII timer — adds elapsed microseconds to a PhaseUs on destruction. Works
// across `goto`/early-return because destructors fire on any scope exit.
// Pass an optional name to get a warn log when the phase exceeds 50 ms
// (helps distinguish DMA-bus latency bursts from mutex contention).
struct ScopedUs {
	std::chrono::steady_clock::time_point start;
	PhaseUs& dst;
	const char* name;
	explicit ScopedUs(PhaseUs& d, const char* n = nullptr)
		: start(std::chrono::steady_clock::now()), dst(d), name(n) {}
	~ScopedUs() {
		const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - start).count();
		dst.add(us);
		if (us > 50'000 && name)
			Log::Warn("[PhaseTimings] {} stalled {}ms", name, us / 1000);
	}
};

// Variant of ScopedUs that also snapshots ScatterRead lifetime counters and
// attributes the byte/range delta to the phase. Use SCATTER_SCOPE for any
// phase whose cost is dominated by scatter I/O — the dump will surface both
// wall-time AND bytes/ranges per call.
struct ScopedScatter {
	std::chrono::steady_clock::time_point start;
	PhaseUs& dst;
	const ScatterRead& sr;
	const char* name;
	uint64_t bytesAtEntry;
	uint64_t rangesAtEntry;
	ScopedScatter(PhaseUs& d, const ScatterRead& s, const char* n = nullptr)
		: start(std::chrono::steady_clock::now())
		, dst(d), sr(s), name(n)
		, bytesAtEntry(s.LifetimeBytes()), rangesAtEntry(s.LifetimeRanges()) {}
	~ScopedScatter() {
		const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - start).count();
		const int64_t bytes  = static_cast<int64_t>(sr.LifetimeBytes()  - bytesAtEntry);
		const int64_t ranges = static_cast<int64_t>(sr.LifetimeRanges() - rangesAtEntry);
		dst.addScatter(us, bytes, ranges);
		if (us > 50'000 && name)
			Log::Warn("[PhaseTimings] {} stalled {}ms", name, us / 1000);
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
			if (r.phase->ranges_sum > 0)
			{
				Log::Info("  {:<24} avg={:>6}us max={:>7}us n={:>5} total={:>9}us  "
					"{:>6}B/call {:>4}r  peak={:>6}B/{:>3}r",
					r.name, r.phase->avg(), r.phase->max, r.phase->samples, r.phase->sum,
					r.phase->bytesAvg(), r.phase->rangesAvg(),
					r.phase->bytes_max, r.phase->ranges_max);
			}
			else
			{
				Log::Info("  {:<24} avg={:>6}us max={:>7}us n={:>5} total={:>9}us",
					r.name, r.phase->avg(), r.phase->max, r.phase->samples, r.phase->sum);
			}
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

// Same as PHASE_SCOPE but also captures bytes-scattered and range-count on
// the passed ScatterRead between scope entry and exit. Use for phases whose
// wall-time is bounded by DMA I/O so the dump can distinguish "big batch,
// bus-limited" from "small batch, per-Execute overhead-limited".
#define SCATTER_SCOPE(name, scatter_read)                                                    \
	static PhaseUs& PHASE_TOKEN_PASTE(_phase_, __LINE__) = PhaseTimings::Get(name);          \
	ScopedScatter PHASE_TOKEN_PASTE(_scoped_, __LINE__)(                                     \
		PHASE_TOKEN_PASTE(_phase_, __LINE__), (scatter_read))
