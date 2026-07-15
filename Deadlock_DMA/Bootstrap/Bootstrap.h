#pragma once

namespace Bootstrap
{
    // Ensures the MemProcFS runtime DLLs exist next to the executable.
    // On first launch (or after the user deletes them) this downloads the
    // latest Windows release zip from github.com/ufrisk/MemProcFS and
    // extracts vmm.dll / leechcore.dll / leechcore_driver.dll / FTD3XX*.dll
    // into the exe directory.
    //
    // MUST run before any call into vmm.dll — the imports are delay-loaded
    // so the process can reach main() without the DLL, but the first VMMDLL
    // call will fault if the DLL is still missing when it fires.
    //
    // Returns true if every required DLL is present at the end.
    bool EnsureRuntimeDlls();
}
