#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
    #include <Windows.h>
#endif

namespace Meridian::Paths
{
    inline std::filesystem::path GameRootFromExecutablePath(const std::filesystem::path& a_executablePath)
    {
        if (a_executablePath.empty() || !a_executablePath.has_filename())
        {
            throw std::invalid_argument("executable path must include a filename");
        }
        return a_executablePath.parent_path().lexically_normal();
    }

#ifdef _WIN32
    inline std::filesystem::path ExecutablePath()
    {
        std::vector<wchar_t> buffer(512);
        for (;;)
        {
            const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                throw std::runtime_error("GetModuleFileNameW failed: " + std::to_string(::GetLastError()));
            }
            if (length < buffer.size() - 1)
            {
                return std::filesystem::path(std::wstring_view(buffer.data(), length));
            }
            buffer.resize(buffer.size() * 2);
        }
    }

    inline const std::filesystem::path& GameRoot()
    {
        static const auto root = GameRootFromExecutablePath(ExecutablePath());
        return root;
    }
#endif

    inline std::filesystem::path DataRoot(const std::filesystem::path& a_gameRoot)
    {
        return a_gameRoot / L"Data";
    }

    inline std::filesystem::path MeridianRoot(const std::filesystem::path& a_gameRoot)
    {
        return DataRoot(a_gameRoot) / L"MeridianUI";
    }

    inline std::filesystem::path SksePluginsRoot(const std::filesystem::path& a_gameRoot)
    {
        return DataRoot(a_gameRoot) / L"SKSE" / L"Plugins";
    }
}
