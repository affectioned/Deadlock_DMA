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

    Log::Info("DMA connecting");

    try {
        LPCSTR args[] = { "", "-device", "FPGA", "-waitinitialize", "-norefresh" };
        m_VMMHandle = VMMDLL_Initialize(5, args);

        if (!m_VMMHandle)
            throw std::runtime_error("VMMDLL_Initialize failed (Check FPGA connection/drivers)");

        Log::Info("DMA connected");
    }
    catch (const std::exception& e) {
        Log::Error("DMA fail: {}", e.what());
        throw;
    }
}

DMA_Connection::~DMA_Connection()
{

	VMMDLL_Close(m_VMMHandle);

	m_VMMHandle = nullptr;

	Log::Info("DMA disconnected");
}
