#ifndef DARKGET_PATHS_H
#define DARKGET_PATHS_H

#include <windows.h>
#include <string>
#include <iostream>
#include <algorithm>

static const char* ENV_KEY =
    "Environment";

/* قراءة PATH (User) */
inline std::string getUserPath()
{
    HKEY hKey;
    char value[32767];
    DWORD size = sizeof(value);

    if (RegOpenKeyExA(
            HKEY_CURRENT_USER,
            ENV_KEY,
            0,
            KEY_READ,
            &hKey) != ERROR_SUCCESS)
        return "";

    RegQueryValueExA(
        hKey,
        "Path",
        NULL,
        NULL,
        (LPBYTE)value,
        &size);

    RegCloseKey(hKey);
    return std::string(value);
}

/* كتابة PATH (User) */
inline bool setUserPath(const std::string& path)
{
    HKEY hKey;

    if (RegOpenKeyExA(
            HKEY_CURRENT_USER,
            ENV_KEY,
            0,
            KEY_SET_VALUE,
            &hKey) != ERROR_SUCCESS)
        return false;

    bool ok =
        RegSetValueExA(
            hKey,
            "Path",
            0,
            REG_EXPAND_SZ,
            (const BYTE*)path.c_str(),
            path.size() + 1) == ERROR_SUCCESS;

    RegCloseKey(hKey);

    // تحديث النظام
    SendMessageTimeoutA(
        HWND_BROADCAST,
        WM_SETTINGCHANGE,
        0,
        (LPARAM)"Environment",
        SMTO_ABORTIFHUNG,
        100,
        NULL);

    return ok;
}

/* ===== atp ===== */
inline void addToPath(const std::string& pkg)
{
    std::string pkgDir = "packages\\" + pkg;

    if (GetFileAttributesA(pkgDir.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        std::cout << "Package not installed\n";
        return;
    }

    char full[MAX_PATH];
    GetFullPathNameA(pkgDir.c_str(), MAX_PATH, full, NULL);
    std::string fullPath(full);

    std::string path = getUserPath();

    if (path.find(fullPath) != std::string::npos)
    {
        std::cout << "Already in PATH\n";
        return;
    }

    if (!path.empty() && path.back() != ';')
        path += ";";

    path += fullPath;

    if (setUserPath(path))
        std::cout << "Added to PATH: " << fullPath << "\n";
    else
        std::cout << "Failed to modify PATH\n";
}

/* ===== rfp ===== */
inline void removeFromPath(const std::string& pkg)
{
    std::string pkgDir = "packages\\" + pkg;

    char full[MAX_PATH];
    GetFullPathNameA(pkgDir.c_str(), MAX_PATH, full, NULL);
    std::string fullPath(full);

    std::string path = getUserPath();

    size_t pos = path.find(fullPath);
    if (pos == std::string::npos)
    {
        std::cout << "Not found in PATH\n";
        return;
    }

    size_t end = pos + fullPath.length();
    if (end < path.size() && path[end] == ';')
        end++;

    path.erase(pos, end - pos);

    if (setUserPath(path))
        std::cout << "Removed from PATH: " << fullPath << "\n";
    else
        std::cout << "Failed to modify PATH\n";
}

#endif
