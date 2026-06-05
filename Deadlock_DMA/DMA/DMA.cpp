#include "pch.h"
#include "DMA.h"

DMA_Connection* DMA_Connection::GetInstance()
{
	if (m_Instance == nullptr)
		m_Instance = new DMA_Connection();

	return m_Instance;
}

VMM_HANDLE DMA_Connection::GetHandle() const
{
	return m_VMMHandle;
}

bool DMA_Connection::EndConnection()
{
	delete m_Instance;
	m_Instance = nullptr;
	return true;
}

DMA_Connection::DMA_Connection()
{

    Log::Info("Connecting to DMA...");

    try {
        LPCSTR args[] = { "", "-device", "FPGA", "-waitinitialize" };
        m_VMMHandle = VMMDLL_Initialize(4, args);

        if (!m_VMMHandle)
            throw std::runtime_error("VMMDLL_Initialize failed (Check FPGA connection/drivers)");

        Log::Info("Connected to DMA!");

        // Extend the TLB (page-table) cache TTL so VMMDLL doesn't re-walk
        // Deadlock's full page tables every ~10 s and stall scatter reads for
        // ~200 ms. Game entity pages are stable for the lifetime of a match.
        ULONG64 tickMs = 0, tlbTicks = 0;
        if (VMMDLL_ConfigGet(m_VMMHandle, VMMDLL_OPT_CONFIG_TICK_PERIOD, &tickMs) &&
            VMMDLL_ConfigGet(m_VMMHandle, VMMDLL_OPT_CONFIG_TLBCACHE_TICKS, &tlbTicks))
        {
            Log::Info("VMMDLL: tick={}ms tlbcache={} ticks ({}s TTL)",
                tickMs, tlbTicks, tickMs * tlbTicks / 1000);
            VMMDLL_ConfigSet(m_VMMHandle, VMMDLL_OPT_CONFIG_TLBCACHE_TICKS, 6000);
            Log::Info("VMMDLL: TLB cache extended to {}s", tickMs * 6000 / 1000);
        }
    }
    catch (const std::exception& e) {
        Log::Error("--- CRITICAL ERROR ---");
        Log::Error("{}", e.what());
        throw;
    }
}

DMA_Connection::~DMA_Connection()
{

	VMMDLL_Close(m_VMMHandle);

	m_VMMHandle = nullptr;

	Log::Info("Disconnected from DMA!");
}
