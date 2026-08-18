#include "pch.h"

#include "MyMakcu.h"

bool MyMakcu::Initialize()
{
	auto devices = makcu::Device::findDevices();
	if (devices.empty())
	{
		Log::Warn("[Makcu] no devices");
	}
	else
	{
		for (const auto& d : devices)
			Log::Info("[Makcu] {} {} v{:04X} p{:04X} conn={}",
				d.port, d.description, d.vid, d.pid, d.isConnected);
	}

	if (!m_Device.connect())
	{
		Log::Warn("[Makcu] connect fail: {}", m_Device.getLastError());
		return false;
	}

	auto DeviceInfo = MyMakcu::m_Device.getDeviceInfo();
	Log::Info("[Makcu] connected {}", DeviceInfo.port);

	return false;
}