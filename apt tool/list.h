#include <windows.h>
#include <iostream>
#include <string>

void listPackages()
{
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA("packages\\*", &ffd);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        std::cout << "No packages directory\n";
        return;
    }

    do
    {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            std::string name = ffd.cFileName;

            // تجاهل . و ..
            if (name == "." || name == "..")
                continue;

            std::cout << name << " | packages\\" << name << std::endl;
        }
    }
    while (FindNextFileA(hFind, &ffd));

    FindClose(hFind);
}
