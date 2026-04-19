#include "pch.h"

#include "MyMakcu.h"

bool MyMakcu::Initialize()
{
	std::println("[Makcu] Initializing Makcu Device...");

	auto devices = makcu::Device::findDevices();
	if (devices.empty())
	{
		std::println("[Makcu] No Makcu devices found on any COM port");
	}
	else
	{
		for (const auto& d : devices)
			std::println("[Makcu] Found device: {} — {} (VID:{:04X} PID:{:04X}) connected={}",
				d.port, d.description, d.vid, d.pid, d.isConnected);
	}

	if (!m_Device.connect())
	{
		std::println("[Makcu] Failed to connect: {}", m_Device.getLastError());
		return false;
	}

	auto DeviceInfo = MyMakcu::m_Device.getDeviceInfo();
	std::println("[Makcu] Connected to Makcu Device on port: {}", DeviceInfo.port);

	return false;
}