#pragma once
#include "DMA/DMA.h"

#include <string>
#include <string_view>
#include <vector>

// Resolves Source 2 schema field offsets from the live CSchemaSystem instead
// of from constants baked per game build.
//
// Modelled on dezlock-dump's runtime schema walker, with one difference that
// shapes the whole implementation: that dumper is injected, so it calls
// CSchemaSystem::FindTypeScopeForModule and CSchemaSystemTypeScope::
// FindDeclaredClass through their vtables. We only have reads, so we walk the
// same structures by hand — the type-scope vector, the class-binding
// CUtlTSHash, the field array.
//
// What that buys: the ~20 game-class offsets in Offsets.h stop being build
// specific. What it costs: a handful of *engine*-level offsets (the field
// array stride, the class-info layout, the TSHash node shape) are still
// hardcoded, and the two that have historically moved — where the bindings
// hash sits inside a type scope, and where the bucket array sits inside the
// hash — are probed for rather than assumed.
namespace SchemaWalker
{
	// Walks schemasystem.dll -> CSchemaSystem -> the client.dll type scope and
	// caches the field table of every class named in WantedClasses. One-time
	// cost at process attach. Returns false if any stage failed, in which case
	// Find returns 0 for everything and callers keep their baked offsets.
	bool Resolve(DMA_Connection* Conn, DWORD Pid, const std::vector<std::string>& WantedClasses);

	// Absolute offset of a field within its declaring class, or 0 if unknown.
	// Source 2 single inheritance places every base at +0, so the offset a
	// class declares for its own field is already the absolute offset in any
	// class derived from it — no flattening pass needed.
	std::ptrdiff_t Find(std::string_view ClassName, std::string_view FieldName);

	bool   Ready();
	size_t ClassCount();
	size_t FieldCount();

	// One-line summary of the last walk, for the Offsets tab.
	std::string Status();
}
