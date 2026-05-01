#include "pch.h"
#include "Input Manager.h"
#include "DMA/Memory/SigScan.h"

//TODO: Restart winlogon.exe when it doesn't exist.
bool c_keys::InitKeyboard(DMA_Connection* Conn)
{
	std::string win = registry.QueryValue(Conn, "HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\CurrentBuild", e_registry_type::sz);
	int Winver = 0;
	if (!win.empty())
		Winver = std::stoi(win);
	else
		return false;
	std::string ubr = registry.QueryValue(Conn, "HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\UBR", e_registry_type::dword);
	int Ubr = 0;
	if (!ubr.empty())
		Ubr = std::stoi(ubr);
	else
		return false;

	DWORD PID = 0x0;
	VMMDLL_PidGetFromName(Conn->GetHandle(), "winlogon.exe", &PID);
	win_logon_pid = PID;

	if (Winver > 22000)
	{
		auto pids = GetPidListFromName(Conn, "csrss.exe");
		for (size_t i = 0; i < pids.size(); i++)
		{
			auto pid = pids[i];

			PVMMDLL_MAP_MODULEENTRY win32k_module_info;
			if (!VMMDLL_Map_GetModuleFromNameW(Conn->GetHandle(), pid, const_cast<LPWSTR>(L"win32ksgd.sys"), &win32k_module_info, VMMDLL_MODULE_FLAG_NORMAL))
			{
				if (!VMMDLL_Map_GetModuleFromNameW(Conn->GetHandle(), pid, const_cast<LPWSTR>(L"win32k.sys"), &win32k_module_info, VMMDLL_MODULE_FLAG_NORMAL))
				{
					Log::Warn("[Input]: Win11 path - win32ksgd.sys/win32k.sys not found in csrss PID {}", pid);
					return false;
				}
			}
			uintptr_t win32k_base = win32k_module_info->vaBase;
			size_t win32k_size = win32k_module_info->cbImageSize;
			VMMDLL_MemFree(win32k_module_info);
			//win32ksgd
			auto g_session_ptr = FindSignature(Conn, "48 8B 05 ? ? ? ? 48 8B 04 C8", win32k_base, win32k_base + win32k_size, pid);
			if (!g_session_ptr)
			{
				//win32k
				g_session_ptr = FindSignature(Conn, "48 8B 05 ? ? ? ? FF C9", win32k_base, win32k_base + win32k_size, pid);
				if (!g_session_ptr)
				{
					Log::Warn("[Input]: Win11 path - g_session_global_slots signature not found in win32k");
					return false;
				}
			}
			uintptr_t ReadAddr = g_session_ptr + 3;
			int _relative = ReadFromPID<int>(Conn, ReadAddr, pid);

			int relative = _relative;
			uintptr_t g_session_global_slots = g_session_ptr + 7 + relative;
			uintptr_t user_session_state = 0;
			for (int i = 0; i < 4; i++)
			{
				uintptr_t Deref1 = ReadFromPID<uintptr_t>(Conn, g_session_global_slots, pid);

				uintptr_t Deref2 = ReadFromPID<uintptr_t>(Conn, Deref1 + 8 * i, pid);

				uintptr_t Deref3 = ReadFromPID<uintptr_t>(Conn, Deref2, pid);

				user_session_state = Deref3;

				if (user_session_state > 0x7FFFFFFFFFFF)
					break;
			}

			PVMMDLL_MAP_MODULEENTRY win32kbase_module_info;
			if (!VMMDLL_Map_GetModuleFromNameW(Conn->GetHandle(), pid, const_cast<LPWSTR>(L"win32kbase.sys"), &win32kbase_module_info, VMMDLL_MODULE_FLAG_NORMAL))
			{
				Log::Warn("[Input]: Win11 path - win32kbase.sys not found in csrss PID {}", pid);
				return false;
			}
			uintptr_t win32kbase_base = win32kbase_module_info->vaBase;
			size_t win32kbase_size = win32kbase_module_info->cbImageSize;
			VMMDLL_MemFree(win32kbase_module_info);

			//Unsure if this sig will work on all versions. (sig is from PostUpdateKeyStateEvent function. seems to exist in both older version and the new version of win32kbase that I have checked)
			uintptr_t ptr = FindSignature(Conn, "48 8D 90 ? ? ? ? E8 ? ? ? ? 0F 57 C0", win32kbase_base, win32kbase_base + win32kbase_size, pid);
			uint32_t session_offset = 0x0;
			if (ptr)
			{
				session_offset = ReadFromPID<uint32_t>(Conn, ptr + 3, pid);

				gafAsyncKeyStateExport = user_session_state + session_offset;
				Log::Info("[Input]: Win11 path - gafAsyncKeyStateExport at 0x{:X}", gafAsyncKeyStateExport);
			}
			else
			{
				Log::Warn("[Input]: Win11 path - gafAsyncKeyStateExport offset signature not found in win32kbase");
				return false;
			}

			if (gafAsyncKeyStateExport > 0x7FFFFFFFFFFF) break;
		}
		if (gafAsyncKeyStateExport > 0x7FFFFFFFFFFF)
		{
			m_bInitialized = true;
			return true;
		}

		return false;
	}
	else
	{
		PVMMDLL_MAP_EAT eat_map = NULL;
		PVMMDLL_MAP_EATENTRY eat_map_entry;
		DWORD PID = 0x0;
		VMMDLL_PidGetFromName(Conn->GetHandle(), "winlogon.exe", &PID);

		bool result = VMMDLL_Map_GetEATU(Conn->GetHandle(), PID | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY, const_cast<LPSTR>("win32kbase.sys"), &eat_map);
		if (!result)
			return false;

		if (eat_map->dwVersion != VMMDLL_MAP_EAT_VERSION)
		{
			VMMDLL_MemFree(eat_map);
			eat_map_entry = NULL;
			return false;
		}

		for (int i = 0; i < eat_map->cMap; i++)
		{
			eat_map_entry = eat_map->pMap + i;
			if (strcmp(eat_map_entry->uszFunction, "gafAsyncKeyState") == 0)
			{
				gafAsyncKeyStateExport = eat_map_entry->vaFunction;

				break;
			}
		}

		VMMDLL_MemFree(eat_map);
		eat_map = NULL;
		if (gafAsyncKeyStateExport < 0x7FFFFFFFFFFF)
		{
			PVMMDLL_MAP_MODULEENTRY module_info;
			auto result = VMMDLL_Map_GetModuleFromNameW(Conn->GetHandle(), PID | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY, const_cast<LPWSTR>(L"win32kbase.sys"), &module_info, VMMDLL_MODULE_FLAG_NORMAL);
			if (!result)
			{
				Log::Warn("[Input]: Win10 path - failed to get win32kbase.sys module info");
				return false;
			}
			uintptr_t win32kbase_va = module_info->vaBase;
			VMMDLL_MemFree(module_info);

			char str[261];
			if (!VMMDLL_PdbLoad(Conn->GetHandle(), PID | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY, win32kbase_va, str))
			{
				Log::Warn("[Input]: Win10 path - failed to load PDB for win32kbase.sys");
				return false;
			}

			uintptr_t gafAsyncKeyState;
			if (!VMMDLL_PdbSymbolAddress(Conn->GetHandle(), str, const_cast<LPSTR>("gafAsyncKeyState"), &gafAsyncKeyState))
			{
				Log::Warn("[Input]: Win10 path - gafAsyncKeyState symbol not found in PDB");
				return false;
			}
			Log::Info("[Input]: Win10 path - gafAsyncKeyState at 0x{:X}", gafAsyncKeyState);
		}
		if (gafAsyncKeyStateExport > 0x7FFFFFFFFFFF)
		{
			m_bInitialized = true;
			return true;
		}

		return false;
	}

}

void c_keys::UpdateKeys(DMA_Connection* Conn)
{
	uint8_t previous_key_state_bitmap[64] = { 0 };
	memcpy(previous_key_state_bitmap, state_bitmap, 64);

	VMMDLL_MemReadEx(Conn->GetHandle(), win_logon_pid | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY, gafAsyncKeyStateExport, reinterpret_cast<PBYTE>(&state_bitmap), 64, NULL, VMMDLL_FLAG_NOCACHE);
	for (int vk = 0; vk < 256; ++vk)
		if ((state_bitmap[(vk * 2 / 8)] & 1 << vk % 4 * 2) && !(previous_key_state_bitmap[(vk * 2 / 8)] & 1 << vk % 4 * 2))
			previous_state_bitmap[vk / 8] |= 1 << vk % 8;
}

bool c_keys::IsKeyDown(DMA_Connection* Conn, uint32_t virtual_key_code)
{
	if (!c_keys::IsInitialized())
		return false;

	if (virtual_key_code >= 256)
		return false;

	if (gafAsyncKeyStateExport < 0x7FFFFFFFFFFF)
		return false;

	if (std::chrono::system_clock::now() - start > std::chrono::milliseconds(100))
	{
		UpdateKeys(Conn);
		start = std::chrono::system_clock::now();
	}

	return state_bitmap[(virtual_key_code * 2 / 8)] & 1 << virtual_key_code % 4 * 2;
}

std::string c_registry::QueryValue(DMA_Connection* Conn, const char* path, e_registry_type type)
{
	if (!Conn->GetHandle())
		return "";

	BYTE buffer[0x128];
	DWORD _type = static_cast<DWORD>(type);
	DWORD size = sizeof(buffer);

	if (!VMMDLL_WinReg_QueryValueExU(Conn->GetHandle(), const_cast<LPSTR>(path), &_type, buffer, &size))
	{
		Log::Warn("[Input]: Registry read failed: {}", path);
		return "";
	}
	if (type == e_registry_type::dword)
	{
		DWORD dwordValue = *reinterpret_cast<DWORD*>(buffer);
		return std::to_string(dwordValue);
	}

	// Bound the wstring by the number of wchar_t units actually returned,
	// then truncate at the first embedded null (registry strings may or may not
	// include the terminator in the reported size).
	size_t wcharCount = size / sizeof(wchar_t);
	std::wstring wstr(reinterpret_cast<wchar_t*>(buffer), wcharCount);
	if (auto pos = wstr.find(L'\0'); pos != std::wstring::npos)
		wstr.resize(pos);

	// Use WideCharToMultiByte for a proper UTF-8 conversion.
	int needed = WideCharToMultiByte(CP_UTF8, 0,
	                                  wstr.data(), (int)wstr.size(),
	                                  nullptr, 0, nullptr, nullptr);
	if (needed <= 0)
		return "";
	std::string result(needed, '\0');
	WideCharToMultiByte(CP_UTF8, 0,
	                    wstr.data(), (int)wstr.size(),
	                    result.data(), needed, nullptr, nullptr);
	return result;
}