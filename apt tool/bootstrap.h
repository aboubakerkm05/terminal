#ifndef BOOTSTRAP_H
#define BOOTSTRAP_H

#include <windows.h>
#include <fstream>

inline void bootstrap()
{
    CreateDirectoryA("packages", NULL);
    if (GetFileAttributesA("repo.json") == INVALID_FILE_ATTRIBUTES) {
        MessageBoxA(NULL, "repo.json not found", "darkget", MB_ICONERROR);
        ExitProcess(1);
    }
}
#endif
