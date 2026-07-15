#include "pch.h"
#include "Bootstrap.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;

namespace
{
	// The MemProcFS "files_and_binaries-win_x64-latest.zip" asset is a stable
	// alias published on every release, so we do not have to parse the
	// versioned filename. All required DLLs sit at the root of that zip.
	constexpr const wchar_t* kGithubApiHost    = L"api.github.com";
	constexpr const wchar_t* kMemProcFSApiPath = L"/repos/ufrisk/MemProcFS/releases/latest";
	constexpr const char*    kAssetNeedle      = "win_x64-latest.zip";

	const std::vector<std::wstring> kRequiredDlls = {
		L"vmm.dll",
		L"leechcore.dll",
		L"leechcore_driver.dll",
		L"FTD3XX.dll",
		L"FTD3XXWU.dll",
	};

	fs::path ExeDir()
	{
		wchar_t buf[MAX_PATH]{};
		GetModuleFileNameW(nullptr, buf, MAX_PATH);
		return fs::path(buf).parent_path();
	}

	bool AnyMissing(const fs::path& dir)
	{
		for (const auto& dll : kRequiredDlls)
		{
			if (!fs::exists(dir / dll)) return true;
		}
		return false;
	}

	std::string Narrow(std::wstring_view w)
	{
		return { w.begin(), w.end() };
	}

	struct Handle
	{
		HINTERNET h = nullptr;
		explicit Handle(HINTERNET x) : h(x) {}
		~Handle() { if (h) WinHttpCloseHandle(h); }
		Handle(const Handle&)            = delete;
		Handle& operator=(const Handle&) = delete;
		operator HINTERNET() const { return h; }
		explicit operator bool() const { return h != nullptr; }
	};

	// Blocking HTTPS GET that follows redirects (needed for the
	// api.github.com → objects.githubusercontent.com hop) and returns the raw
	// response body. Fails on non-200 status.
	bool HttpsGet(const std::wstring& host, const std::wstring& path, std::vector<BYTE>& body)
	{
		Handle session(WinHttpOpen(L"Deadlock_DMA-Bootstrap/1.0",
			WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0));
		if (!session) return false;

		Handle connect(WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
		if (!connect) return false;

		Handle req(WinHttpOpenRequest(connect, L"GET", path.c_str(),
			nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
		if (!req) return false;

		DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
		WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));

		if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) return false;
		if (!WinHttpReceiveResponse(req, nullptr)) return false;

		DWORD status = 0, statusSize = sizeof(status);
		WinHttpQueryHeaders(req,
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
		if (status != 200)
		{
			Log::Error("[Bootstrap] HTTP {} for https://{}{}", status, Narrow(host), Narrow(path));
			return false;
		}

		body.clear();
		DWORD available = 0;
		while (WinHttpQueryDataAvailable(req, &available) && available > 0)
		{
			const size_t off = body.size();
			body.resize(off + available);
			DWORD read = 0;
			if (!WinHttpReadData(req, body.data() + off, available, &read)) return false;
			body.resize(off + read);
		}
		return true;
	}

	// Given the GitHub /releases/latest JSON, finds the browser_download_url
	// of the asset whose "name" field contains `needle`. GitHub's asset
	// objects always spell name before browser_download_url, so a forward
	// scan for the next url key is enough — no JSON parser dependency.
	std::string FindAssetUrl(std::string_view json, std::string_view needle)
	{
		const size_t namePos = json.find(needle);
		if (namePos == std::string_view::npos) return {};

		constexpr std::string_view urlKey = "\"browser_download_url\":\"";
		const size_t urlPos = json.find(urlKey, namePos);
		if (urlPos == std::string_view::npos) return {};

		const size_t start = urlPos + urlKey.size();
		const size_t end   = json.find('"', start);
		if (end == std::string_view::npos) return {};
		return std::string(json.substr(start, end - start));
	}

	bool SplitHttpsUrl(const std::string& url, std::wstring& host, std::wstring& path)
	{
		constexpr std::string_view prefix = "https://";
		if (url.rfind(prefix, 0) != 0) return false;
		const size_t slash = url.find('/', prefix.size());
		if (slash == std::string::npos) return false;
		const std::string h = url.substr(prefix.size(), slash - prefix.size());
		const std::string p = url.substr(slash);
		host.assign(h.begin(), h.end());
		path.assign(p.begin(), p.end());
		return true;
	}

	// Windows 10 1803+ ships bsdtar as %SystemRoot%\System32\tar.exe. It
	// handles .zip via libarchive so no extra dependency is needed.
	bool ExtractZip(const fs::path& zipFile, const fs::path& outDir)
	{
		std::error_code ec;
		fs::create_directories(outDir, ec);

		wchar_t sys[MAX_PATH]{};
		GetSystemDirectoryW(sys, MAX_PATH);

		std::wstring cmd;
		cmd += L"\"";
		cmd += sys;
		cmd += L"\\tar.exe\" -xf \"";
		cmd += zipFile.wstring();
		cmd += L"\" -C \"";
		cmd += outDir.wstring();
		cmd += L"\"";

		STARTUPINFOW si{ sizeof(si) };
		si.dwFlags     = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
		PROCESS_INFORMATION pi{};

		std::wstring mutableCmd = cmd;
		if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE,
			CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;

		WaitForSingleObject(pi.hProcess, INFINITE);
		DWORD exitCode = 1;
		GetExitCodeProcess(pi.hProcess, &exitCode);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return exitCode == 0;
	}

	void InstallDllsFrom(const fs::path& extractedRoot, const fs::path& exeDir)
	{
		std::error_code ec;
		for (auto it = fs::recursive_directory_iterator(extractedRoot, ec);
			it != fs::recursive_directory_iterator(); it.increment(ec))
		{
			if (ec) { ec.clear(); continue; }
			if (!it->is_regular_file()) continue;

			const auto& p = it->path();
			for (const auto& want : kRequiredDlls)
			{
				if (_wcsicmp(p.filename().c_str(), want.c_str()) != 0) continue;

				fs::copy_file(p, exeDir / want, fs::copy_options::overwrite_existing, ec);
				if (ec)
				{
					Log::Warn("[Bootstrap] Copy failed for {}: {}", Narrow(want), ec.message());
					ec.clear();
				}
				else
				{
					Log::Info("[Bootstrap] Installed {}", Narrow(want));
				}
			}
		}
	}
} // namespace

namespace Bootstrap
{
	bool EnsureRuntimeDlls()
	{
		const fs::path exeDir = ExeDir();
		if (!AnyMissing(exeDir)) return true;

		Log::Info("[Bootstrap] MemProcFS runtime DLLs missing; fetching latest release...");

		std::vector<BYTE> apiBody;
		if (!HttpsGet(kGithubApiHost, kMemProcFSApiPath, apiBody))
		{
			Log::Error("[Bootstrap] Failed to query GitHub releases API");
			return false;
		}

		const std::string_view json(reinterpret_cast<const char*>(apiBody.data()), apiBody.size());
		const std::string zipUrl = FindAssetUrl(json, kAssetNeedle);
		if (zipUrl.empty())
		{
			Log::Error("[Bootstrap] Could not find asset '{}' in release manifest", kAssetNeedle);
			return false;
		}
		Log::Info("[Bootstrap] Downloading {}", zipUrl);

		std::wstring zipHost, zipPath;
		if (!SplitHttpsUrl(zipUrl, zipHost, zipPath))
		{
			Log::Error("[Bootstrap] Malformed asset URL: {}", zipUrl);
			return false;
		}

		std::vector<BYTE> zipBytes;
		if (!HttpsGet(zipHost, zipPath, zipBytes))
		{
			Log::Error("[Bootstrap] Failed to download release zip");
			return false;
		}

		wchar_t tempRoot[MAX_PATH]{};
		GetTempPathW(MAX_PATH, tempRoot);
		const fs::path staging = fs::path(tempRoot) / L"deadlock_dma_bootstrap";
		std::error_code ec;
		fs::remove_all(staging, ec);
		fs::create_directories(staging, ec);

		const fs::path zipFile = staging / L"memprocfs.zip";
		{
			std::ofstream out(zipFile, std::ios::binary);
			out.write(reinterpret_cast<const char*>(zipBytes.data()), zipBytes.size());
		}

		const fs::path extractDir = staging / L"extracted";
		if (!ExtractZip(zipFile, extractDir))
		{
			Log::Error("[Bootstrap] tar extraction failed");
			return false;
		}

		InstallDllsFrom(extractDir, exeDir);

		if (AnyMissing(exeDir))
		{
			Log::Error("[Bootstrap] One or more required DLLs still missing after install");
			return false;
		}

		fs::remove_all(staging, ec);
		Log::Info("[Bootstrap] MemProcFS runtime DLLs ready");
		return true;
	}
} // namespace Bootstrap
