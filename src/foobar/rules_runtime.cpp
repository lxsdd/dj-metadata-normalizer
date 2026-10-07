#include "stdafx.h"
#include "resource.h"
#include "rules_runtime.h"

#include <limits>

namespace djmeta_foobar {
namespace {

std::wstring roaming_rules_path() {
    PWSTR raw = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, &raw);
    if (FAILED(hr) || raw == nullptr) return {};

    std::wstring path(raw);
    CoTaskMemFree(raw);
    path += L"\\DJMetadataNormalizer\\ruleset.json";
    return path;
}

bool read_file_utf8(const std::wstring& path, std::string& out) {
    HANDLE handle = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle, &size) || size.QuadPart < 0 || size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(handle);
        throw std::runtime_error("Shared ruleset exceeds the 16 MiB safety limit.");
    }

    out.resize(static_cast<std::size_t>(size.QuadPart));
    std::size_t total = 0;
    while (total < out.size()) {
        const std::size_t chunk = (std::min)(out.size() - total, static_cast<std::size_t>((std::numeric_limits<DWORD>::max)()));
        const DWORD remaining = static_cast<DWORD>(chunk);
        DWORD read = 0;
        if (!ReadFile(handle, out.data() + total, remaining, &read, nullptr)) {
            CloseHandle(handle);
            throw std::runtime_error("Could not read the shared ruleset.");
        }
        if (read == 0) break;
        total += static_cast<std::size_t>(read);
    }
    CloseHandle(handle);
    out.resize(total);
    return true;
}

std::string load_embedded_rules() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&load_embedded_rules),
            &module)) {
        throw std::runtime_error("Could not resolve the component module for embedded rules.");
    }

    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_DEFAULT_RULESET), RT_RCDATA);
    if (!resource) throw std::runtime_error("Embedded default ruleset resource is missing.");

    HGLOBAL loaded = LoadResource(module, resource);
    if (!loaded) throw std::runtime_error("Embedded default ruleset resource could not be loaded.");

    const DWORD size = SizeofResource(module, resource);
    const void* data = LockResource(loaded);
    if (!data && size != 0) throw std::runtime_error("Embedded default ruleset resource could not be read.");

    return std::string(static_cast<const char*>(data), static_cast<std::size_t>(size));
}

std::string narrow_path_for_display(const std::wstring& path) {
    if (path.empty()) return {};
    const int count = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()),
        nullptr, 0, nullptr, nullptr);
    if (count <= 0) return "<shared ruleset>";
    std::string out(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()),
        out.data(), count, nullptr, nullptr);
    return out;
}

} // namespace

loaded_rules_text load_rules_text() {
    const std::wstring shared_path = roaming_rules_path();
    if (!shared_path.empty()) {
        std::string shared;
        if (read_file_utf8(shared_path, shared)) {
            return {std::move(shared), narrow_path_for_display(shared_path)};
        }
    }
    return {load_embedded_rules(), "embedded default ruleset"};
}

} // namespace djmeta_foobar
