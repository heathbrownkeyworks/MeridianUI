#pragma once

#include <filesystem>
#include <KnownFolders.h>
#include <shlobj.h>
#include <iostream>
#include "../../Common/RuntimePaths.h"

namespace Meridian::Utils
{
    static inline std::filesystem::path GetTempAppDataPath()
    {
        auto appPath = std::filesystem::temp_directory_path() / Meridian::UI::LibVersion::PROJECT_NAME;
        CreateDirectoryW(appPath.wstring().c_str(), 0);
        return appPath;
    }

    static inline std::filesystem::path GetLocalAppDataPath()
    {
        PWSTR pwStr;
        std::filesystem::path fsPath;

        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &pwStr)))
        {
            fsPath = std::filesystem::path(pwStr) / Meridian::UI::LibVersion::PROJECT_NAME;
        }
        CoTaskMemFree(pwStr);
        return fsPath;
    }

    static inline std::filesystem::path GetPathToMyDocuments()
    {
        PWSTR pwStr;
        std::filesystem::path fsPath;

        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &pwStr)))
        {
            fsPath = std::filesystem::path(pwStr);
        }
        CoTaskMemFree(pwStr);
        return fsPath;
    }

    static inline std::filesystem::path GetPathToSSESaves()
    {
        return GetPathToMyDocuments() / "My Games" / "Skyrim Special Edition" / "Saves";
    }

}
