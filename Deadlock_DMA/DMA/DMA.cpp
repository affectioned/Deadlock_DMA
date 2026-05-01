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
