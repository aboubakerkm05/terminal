#ifndef DARKGET_BASEDIR_H
#define DARKGET_BASEDIR_H

#include <windows.h>
#include <string>

inline std::string getBaseDir()
{
    char fullPath[MAX_PATH];
    GetModuleFileNameA(NULL, fullPath, MAX_PATH);

    std::string path(fullPath);

    // احذف اسم exe
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos)
        path = path.substr(0, pos);

    return path;
}

#endif
