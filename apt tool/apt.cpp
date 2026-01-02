// ------------------ start includes   --------------------------
#include <windows.h>
#include <wininet.h>
#include <iostream>
#include <fstream>
#include <string>
#include "downloader.h"
#include "bootstrap.h"
#include "list.h"
#include "basedir.h"
#include "paths.h"
// ------------------ end includes   --------------------------
// تحقق من وجود pkg عبر packages فقط

bool isInstalled(const std::string& pkg)
{
    std::string p = "packages\\" + pkg;
    DWORD a = GetFileAttributesA(p.c_str());
    return (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
}

// --------------------------------------------------
bool searchRepo(const std::string& pkg, std::string& url)
{
    std::ifstream f("repo.json");
    std::string data(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());

    size_t p = data.find("\"name\": \"" + pkg + "\"");
    if (p == std::string::npos) return false;

    size_t u = data.find("\"url\":", p);
    size_t a = data.find("\"", u + 6) + 1;
    size_t b = data.find("\"", a);
    url = data.substr(a, b - a);
    return true;
}

// --------------------------------------------------
std::string githubSearch(const std::string& pkg)
{
    std::string api =
        "https://api.github.com/search/repositories?q=" + pkg;

    std::wstring w(api.begin(), api.end());

    HINTERNET hNet = InternetOpenW(L"darkget", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    HINTERNET hUrl = InternetOpenUrlW(
        hNet, w.c_str(),
        L"User-Agent: darkget\r\n",
        -1, INTERNET_FLAG_RELOAD, 0);

    if (!hUrl) return "";

    char buf[4096];
    DWORD r;
    std::string json;

    while (InternetReadFile(hUrl, buf, sizeof(buf), &r) && r)
        json.append(buf, r);

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hNet);

    size_t p = json.find("\"full_name\"");
    if (p == std::string::npos) return "";

    size_t a = json.find("\"", p + 12) + 1;
    size_t b = json.find("\"", a);
    return json.substr(a, b - a);
}

// --------------------------------------------------
// فك ZIP عبر Shell.Application (PS 2.0)
bool extractZip(const std::string& zipRel, const std::string& dstRel)
{
    char zip[MAX_PATH], dst[MAX_PATH];
    GetFullPathNameA(zipRel.c_str(), MAX_PATH, zip, NULL);
    GetFullPathNameA(dstRel.c_str(), MAX_PATH, dst, NULL);
    CreateDirectoryA(dst, NULL);

    std::string ps =
        "powershell -NoProfile -Command \""
        "$sh=New-Object -Com Shell.Application;"
        "$z=$sh.NameSpace('" + std::string(zip) + "');"
        "$d=$sh.NameSpace('" + std::string(dst) + "');"
        "if($z -eq $null -or $d -eq $null){exit 1};"
        "$d.CopyHere($z.Items(),16);"
        "while($d.Items().Count -lt $z.Items().Count){Start-Sleep -Milliseconds 200}\"";

    return system(ps.c_str()) == 0;
}

// --------------------------------------------------
int main(int argc, char* argv[])
{
    std::string BASE = getBaseDir();
    // غيّر working directory
    SetCurrentDirectoryA(BASE.c_str());

    bootstrap();

    if (argc < 2) {
        std::cout <<
            "apt install <pkg>\n"
            "apt uninstall <pkg>\n"
            "apt list\n"
            "apt search <pkg>\n";
        return 0;
    }

    std::string cmd = argv[1];
    std::string pkg = (argc >= 3 ? argv[2] : "");

    // -------- list --------
    if (cmd == "list") {
        listPackages(); 
        return 0;
    }

    // -------- uninstall --------
    if (cmd == "uninstall") {
        if (!isInstalled(pkg)) {
            std::cout << pkg << " not installed\n";
            return 0;
        }
        system(("rmdir /s /q packages\\" + pkg).c_str());
        removeFromPath(pkg);
        std::cout << pkg << " removed\n";
        return 0;
    }
    // -------- add to path --------
    if (cmd == "atp" && argc >= 3){
        addToPath(pkg);
        return 0;
    }
    // -------- remove from path --------
    if (cmd == "rfp" && argc >= 3){
        removeFromPath(pkg);
        return 0;
    }


    // -------- search --------
    if (cmd == "search") {
        std::string url;
        if (searchRepo(pkg, url))
            std::cout << "[repo] " << url << "\n";
        else {
            std::string r = githubSearch(pkg);
            if (r.empty()) std::cout << "Not found\n";
            else std::cout << "[github] https://github.com/" << r << "\n";
        }
        return 0;
    }

    // -------- install --------
    if (cmd == "install") {

        if (isInstalled(pkg)) {
            std::cout << pkg << " is already installed.\nInstall again? (y/n): ";
            char c; std::cin >> c;
            if (c != 'y' && c != 'Y') return 0;
            system(("rmdir /s /q packages\\" + pkg).c_str());
        }

        std::string url;
        if (!searchRepo(pkg, url)) {
            std::string r = githubSearch(pkg);
            if (r.empty()) {
                std::cout << "Package not found\n";
                return 0;
            }
            url = "https://github.com/" + r + "/archive/refs/heads/master.zip";
        }

        std::string zipPath = "packages\\" + pkg + ".zip";
        std::wstring wurl(url.begin(), url.end());
        std::wstring wzip(zipPath.begin(), zipPath.end());

        if (!download(wurl, wzip)) {
            std::cout << "Download failed\n";
            return 0;
        }

        system("rmdir /s /q packages\\__tmp >nul 2>&1");
        extractZip(zipPath, "packages\\__tmp");

        system(("for /d %i in (packages\\__tmp\\*) do move \"%i\" packages\\" + pkg + "\"").c_str());
        system("rmdir /s /q packages\\__tmp");
        DeleteFileA(zipPath.c_str());
        addToPath(pkg);

        std::cout << pkg << " installed\n";
    }
}
