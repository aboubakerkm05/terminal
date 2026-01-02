#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <windows.h>
#include <wininet.h>
#include <fstream>
#include <string>

#pragma comment(lib, "wininet.lib")

inline bool download(const std::wstring& url, const std::wstring& outPath)
{
    // تحويل المسار من wstring إلى string (MinGW compatible)
    std::string out(outPath.begin(), outPath.end());

    HINTERNET hNet = InternetOpenW(
        L"DarkGet/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG,
        NULL,
        NULL,
        0
    );
    if (!hNet) return false;

    HINTERNET hUrl = InternetOpenUrlW(
        hNet,
        url.c_str(),
        L"User-Agent: DarkGet\r\n",
        -1,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
        0
    );
    if (!hUrl) {
        InternetCloseHandle(hNet);
        return false;
    }

    std::ofstream file(out.c_str(), std::ios::binary);
    if (!file.is_open()) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hNet);
        return false;
    }

    char buffer[4096];
    DWORD bytesRead = 0;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead)
    {
        file.write(buffer, bytesRead);
    }

    file.close();
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hNet);
    return true;
}

#endif
