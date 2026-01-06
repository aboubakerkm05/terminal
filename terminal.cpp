#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <conio.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#define KALI_BLUE (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY | FOREGROUND_INTENSITY)
using namespace std;
bool isRoot = false;
#ifndef CONSOLE_FONT_INFOEX
typedef struct _CONSOLE_FONT_INFOEX {
    ULONG cbSize;
    DWORD nFont;
    COORD dwFontSize;
    UINT FontFamily;
    UINT FontWeight;
    WCHAR FaceName[LF_FACESIZE];
} CONSOLE_FONT_INFOEX;

extern "C" BOOL WINAPI SetCurrentConsoleFontEx(
    HANDLE, BOOL, CONSOLE_FONT_INFOEX*
);
#endif

HANDLE hOut;
SHORT inputStartX = 0;
string getComputerNameStr();
string getDisplayPath(bool isRoot);
bool actpath = false;

bool autocompleteEnabled = true;
void fixWrappedCursor();
void executeCommand(const string& input);
void setTitle() {
    SetConsoleTitleA("Terminal");
}
void setColor(WORD c) {
    SetConsoleTextAttribute(hOut, c);
}

void resetColor() {
    setColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void setBetterFont() {
    CONSOLE_FONT_INFOEX cfi{};
    cfi.cbSize = sizeof(cfi);
    cfi.dwFontSize.Y = 18;
    wcscpy(cfi.FaceName, L"Consolas");
    SetCurrentConsoleFontEx(hOut, FALSE, &cfi);
}
std::string stripExt(const std::string& name) {
    size_t p = name.find_last_of('.');
    if (p == std::string::npos) return name;
    return name.substr(0, p);
}
bool endsWith(const std::string& s, const std::string& e) {
    return s.size() >= e.size() &&
           s.compare(s.size() - e.size(), e.size(), e) == 0;
}


const string PROMPT = "kali@user:~$ ";
bool isRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(
        &NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0,0,0,0,0,0,
        &adminGroup))
    {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}
void runAsAdmin(const string& cmd) {
    ShellExecuteA(
        NULL,
        "runas",
        "cmd.exe",
        ("/c " + cmd).c_str(),
        NULL,
        SW_SHOW
    );
}

string getHomeDir() {
    char buf[MAX_PATH];
    GetEnvironmentVariableA("USERPROFILE", buf, MAX_PATH);
    return buf;
}

SHORT promptStartY;

void drawPrompt()
{
    CONSOLE_SCREEN_BUFFER_INFO cs;
    GetConsoleScreenBufferInfo(hOut, &cs);

    SHORT startX = cs.dwCursorPosition.X;
    SHORT startY = cs.dwCursorPosition.Y;

    string pc = getComputerNameStr();
    string path = getDisplayPath(isRoot);

    setColor(KALI_BLUE);
    cout << "(";
    resetColor();

    if (isRoot) {
        setColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
        cout << "root@"<<pc;
    } else {
        setColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        cout << "kali@"<<pc;
    }

    resetColor();

    setColor(KALI_BLUE);
    cout << ")-[";
    resetColor();
    if (isRoot){
    cout << path;
    
    }
    else{
        if (actpath){
            cout << path;
        }
        else{
            cout << "~";
        }
    }
    resetColor();

    setColor(KALI_BLUE);
    cout << "]";
    resetColor();

    GetConsoleScreenBufferInfo(hOut, &cs);
    SHORT cornerX = cs.dwCursorPosition.X;

    cout << "\n";

    COORD promptPos{startX, (SHORT)(startY + 1)};
    SetConsoleCursorPosition(hOut, promptPos);

    setColor(KALI_BLUE);
    cout << "-";
    resetColor();

    if (isRoot) {
        setColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
        cout << "# ";
    } else {
        setColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        cout << "$ ";
    }

    resetColor();

    GetConsoleScreenBufferInfo(hOut, &cs);
    inputStartX = cs.dwCursorPosition.X;
    promptStartY = cs.dwCursorPosition.Y;
}


string buffer, ghost;
size_t cursorPos = 0;


bool fileExists(const string& p) {
    return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES;
}

vector<string> splitPATH() {
    vector<string> r;
    char* p = getenv("PATH");
    if (!p) return r;
    string s(p);
    size_t pos;
    while ((pos = s.find(';')) != string::npos) {
        r.push_back(s.substr(0, pos));
        s.erase(0, pos + 1);
    }
    r.push_back(s);
    return r;
}

bool running = true;

vector<string> baseCmds = {"clear","pwd","ping ","whoami","ifconfig","cls","dir","ls","bash","cd","sudo su","apt","apt install ","apt search ","apt list","apt uninstall"};

vector<string> currentDirFiles() {
    vector<string> r;
    WIN32_FIND_DATAA fd;
    string display;
    HANDLE h = FindFirstFileA("*", &fd);
    if (h == INVALID_HANDLE_VALUE) return r;
    do {
        string n = fd.cFileName;
        if (string(fd.cFileName) != "." && string(fd.cFileName) != ".."){
            if (endsWith(fd.cFileName, ".exe") || endsWith(fd.cFileName, ".bat")){
                display = stripExt(fd.cFileName);
                r.push_back(display);
            }
            else{
                r.push_back(n);
            }
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return r;
}

void updateGhost() {
    ghost.clear();
    if (!autocompleteEnabled || buffer.empty()) return;

    vector<string> all = baseCmds;
    auto files = currentDirFiles();
    all.insert(all.end(), files.begin(), files.end());

    for (auto& c : all) {
        if (c.rfind(buffer, 0) == 0 && c != buffer) {
            ghost = c.substr(buffer.size());
            return;
        }
    }
}
string getComputerNameStr() {
    char name[MAX_PATH];
    DWORD size = MAX_PATH;
    if (GetComputerNameA(name, &size))
        return name;
    return "kali";
}
string getDisplayPath(bool rootMode)
{
    char cwd[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, cwd);

    string cwdStr = cwd;
    string home = getHomeDir();

    if (!rootMode)
    {
        if (_stricmp(cwdStr.c_str(), home.c_str()) == 0)
            return "~";

        if (cwdStr.size() < home.size())
            return "~";

        if (_stricmp(cwdStr.substr(0, home.size()).c_str(), home.c_str()) == 0)
        {
            if (cwdStr.size() == home.size())
                return "~";

            string rest = cwdStr.substr(home.size() + 1);
            size_t pos = rest.find_last_of("\\/");
            return pos == string::npos ? rest : rest.substr(pos + 1);
        }

        size_t pos = cwdStr.find_last_of("\\/");
        return pos == string::npos ? cwdStr : cwdStr.substr(pos + 1);
    }

    replace(cwdStr.begin(), cwdStr.end(), '\\', '/');
    return cwdStr;
}


void redrawLine()
{
    CONSOLE_SCREEN_BUFFER_INFO cs;
    GetConsoleScreenBufferInfo(hOut, &cs);

    SHORT width = cs.dwSize.X;

    int totalLen = (int)buffer.size() + (int)ghost.size();
    int linesUsed = (inputStartX + totalLen) / width + 1;

    DWORD written;

    for (int i = 0; i < linesUsed; ++i)
    {
        COORD pos;
        DWORD count;

        if (i == 0)
        {

            pos = { inputStartX, (SHORT)(promptStartY) };
            count = width - inputStartX;
        }
        else
        {

            pos = { 0, (SHORT)(promptStartY + i) };
            count = width;
        }

        FillConsoleOutputCharacterA(
            hOut,
            ' ',
            count,
            pos,
            &written
        );
    }

    COORD start{ inputStartX, promptStartY };
    SetConsoleCursorPosition(hOut, start);

    cout << buffer;

    if (!ghost.empty())
    {
        setColor(FOREGROUND_INTENSITY);
        cout << ghost;
        resetColor();
    }

    int cursorIndex = inputStartX + (int)cursorPos;

    COORD cur;
    cur.X = cursorIndex % width;
    cur.Y = promptStartY + (cursorIndex / width);

    SetConsoleCursorPosition(hOut, cur);
}




void listDir() {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("*", &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        string n = fd.cFileName;
        if (n == "." || n == "..") continue;

        bool isDir = fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
        bool isExe = !isDir && n.size() > 4 &&
                     _stricmp(n.c_str() + n.size() - 4, ".exe") == 0;
        bool isBAT = !isDir && n.size() > 4 &&
                     _stricmp(n.c_str() + n.size() - 4, ".bat") == 0;

        if (isDir)
            setColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        else if (isExe || isBAT) {
            setColor(FOREGROUND_BLUE | FOREGROUND_INTENSITY);
            n = n.substr(0, n.size() - 4);
        }

        cout << n << "\n";
        resetColor();

    } while (FindNextFileA(h, &fd));
    cout<<"\n";
    FindClose(h);
}
string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    if (start == string::npos) return "";
    return s.substr(start, end - start + 1);
}
void loadGhostCommands() {
    HKEY hKey;
    if (RegOpenKeyExA(
            HKEY_CURRENT_USER,
            "Software\\TerminalGhost\\Commands",
            0,
            KEY_READ,
            &hKey) != ERROR_SUCCESS)
        return;

    char name[256];
    DWORD nameSize;
    DWORD index = 0;

    while (true) {
        nameSize = sizeof(name);
        if (RegEnumValueA(
                hKey,
                index,
                name,
                &nameSize,
                NULL,
                NULL,
                NULL,
                NULL) != ERROR_SUCCESS)
            break;

        if (find(baseCmds.begin(), baseCmds.end(), name) == baseCmds.end())
            baseCmds.push_back(name);

        index++;
    }

    RegCloseKey(hKey);
}

void relaunchAsAdmin(const string& extraArgs = "") {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    char cwd[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, cwd);

    string params = "--admin";
    if (!extraArgs.empty()) {
        params += " ";
        params += extraArgs;
    }

    SHELLEXECUTEINFOA sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask  = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = "runas";
    sei.lpFile = exePath;          
    sei.lpParameters = params.c_str();
    sei.lpDirectory  = cwd;        
    sei.nShow = SW_SHOW;

    if (ShellExecuteExA(&sei)) {
        ExitProcess(0); 
    }
}


void handleArgs(int argc, char* argv[]) {
    bool wantAdmin = false;
    bool hasCommand = false;
    string command;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];

        if (arg == "--admin") {
            wantAdmin = true;
        }
        else if (arg == "-c" && i + 1 < argc) {
            hasCommand = true;
            command = argv[i + 1];
            i++;
        }
    }


    if (wantAdmin && !isRunningAsAdmin()) {
        string extra;
        if (hasCommand) {
            extra = "-c \"" + command + "\"";
        }
        relaunchAsAdmin(extra);
        return;
    }



    if (wantAdmin && isRunningAsAdmin()) {
        isRoot = true;
    }

    if (hasCommand) {
        executeCommand(command);
        running = false; 
    }
}
DWORD originalConsoleMode;
void saveConsoleMode() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hIn, &originalConsoleMode);
}
void setShellConsoleMode() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = originalConsoleMode;

    mode |= ENABLE_EXTENDED_FLAGS;
    mode |= ENABLE_QUICK_EDIT_MODE;
    mode |= ENABLE_MOUSE_INPUT;
    mode &= ~ENABLE_PROCESSED_INPUT; 

    SetConsoleMode(hIn, mode);
}
void setInteractiveConsoleMode() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = originalConsoleMode;

    mode |= ENABLE_PROCESSED_INPUT;   
    mode |= ENABLE_LINE_INPUT;
    mode |= ENABLE_ECHO_INPUT;

    SetConsoleMode(hIn, mode);
}

void ghostAdd(const string& cmd) {
    if (cmd.empty()) return;

    if (find(baseCmds.begin(), baseCmds.end(), cmd) != baseCmds.end()) {
        cout << "[ghost] already exists: " << cmd << "\n";
        return;
    }

    baseCmds.push_back(cmd);

    HKEY hKey;
    RegCreateKeyExA(
        HKEY_CURRENT_USER,
        "Software\\TerminalGhost\\Commands",
        0, NULL, 0,
        KEY_WRITE,
        NULL,
        &hKey,
        NULL
    );

    DWORD v = 1;
    RegSetValueExA(
        hKey,
        cmd.c_str(),
        0,
        REG_DWORD,
        (BYTE*)&v,
        sizeof(v)
    );

    RegCloseKey(hKey);

    cout << "[ghost] added permanently: " << cmd << "\n";
}
PROCESS_INFORMATION child{};
bool childAlive = false;

void ghostRemove(const string& cmd) {
    auto it = find(baseCmds.begin(), baseCmds.end(), cmd);
    if (it == baseCmds.end()) {
        cout << "[ghost] not found: " << cmd << "\n";
        return;
    }

    baseCmds.erase(it);

    HKEY hKey;
    if (RegOpenKeyExA(
            HKEY_CURRENT_USER,
            "Software\\TerminalGhost\\Commands",
            0,
            KEY_WRITE,
            &hKey) == ERROR_SUCCESS) {

        RegDeleteValueA(hKey, cmd.c_str());
        RegCloseKey(hKey);
    }

    cout << "[ghost] removed permanently: " << cmd << "\n";
}
#include <windows.h>
#include <string>

bool runProcessInteractive(const std::string& command)
{
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    char cmdline[4096];
    strcpy(cmdline, command.c_str());

    BOOL ok = CreateProcessA(
        nullptr,
        cmdline,
        nullptr,
        nullptr,
        TRUE,   
        0,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    if (!ok)
        return false;

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}
DWORD g_childPID = 0;
void executeCommand(const string& input) {
    string cmdline = trim(input);
    if (cmdline.empty()) return;

    if (cmdline == "sudo su") {
        if (!isRunningAsAdmin()) {
            relaunchAsAdmin();
        } else {
            isRoot = true;
        }
        return;
    }

    if (cmdline == "ifconfig") { executeCommand("ipconfig"); return; }
    if (cmdline == "clear" || cmdline == "cls") { system("cls"); return; }
    if (cmdline == "ls" || cmdline == "dir") { listDir(); return; }

    if (cmdline == "pwd") {
        char b[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, b);
        cout << b << "\n";
        return;
    }
    

    if (cmdline.rfind("//ghost add ", 0) == 0) {
        string cmd = cmdline.substr(11); 
        ghostAdd(trim(cmd));
        return;
    }


    if (cmdline.rfind("//ghost remove ", 0) == 0) {
        string cmd = cmdline.substr(14); 
        ghostRemove(trim(cmd));
        return;
    }
    

    if (cmdline == "exit") {
        running = false;
        return;
    }

    if (cmdline == "bash") {
        autocompleteEnabled = !autocompleteEnabled;
        cout << "[autocomplete "
             << (autocompleteEnabled ? "ON" : "OFF") << "]\n";
        return;
    }

    if (cmdline.rfind("cd ", 0) == 0) {
        actpath = true;
        SetCurrentDirectoryA(cmdline.substr(3).c_str());
        return;
    }
    std::string cmd = trim(input);
    if (cmd.empty()) return;

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD oldMode = 0;
    GetConsoleMode(hIn, &oldMode);


    std::string base = cmd.substr(0, cmd.find(' '));
    std::string args = (cmd.find(' ') != std::string::npos)
        ? cmd.substr(cmd.find(' ') + 1)
        : "";

    char found[MAX_PATH]{};
    bool isExe = false;
    bool isPy  = false;
    bool isBat = false;

    if (SearchPathA(NULL, (base + ".exe").c_str(), NULL, MAX_PATH, found, NULL))
        isExe = true;
    else if (SearchPathA(NULL, (base + ".py").c_str(), NULL, MAX_PATH, found, NULL))
        isPy = true;
    else if (SearchPathA(NULL, (base + ".bat").c_str(), NULL, MAX_PATH, found, NULL))
        isBat = true;
    setInteractiveConsoleMode();
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
    BOOL ok = FALSE;


    if (isPy) {
        std::string exec =
            "python \"" + std::string(found) + "\"" +
            (args.empty() ? "" : " " + args);

        ok = CreateProcessA(
            NULL,
            &exec[0],
            NULL, NULL,
            TRUE,
            CREATE_NEW_PROCESS_GROUP,
            NULL, NULL,
            &si, &pi
        );

        if (ok) {
            child = pi;
            childAlive = true;
            WaitForSingleObject(pi.hProcess, INFINITE);
            childAlive = false;
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        SetConsoleMode(hIn, oldMode);
        return; 
    }
    if (isBat) {
        std::string exec =
            "cmd /c \"" + std::string(found) + "\"" +
            (args.empty() ? "" : " " + args);

        ok = CreateProcessA(
            NULL,
            &exec[0],
            NULL, NULL,
            TRUE,
            CREATE_NEW_PROCESS_GROUP,
            NULL, NULL,
            &si, &pi
        );

        if (ok) {
            child = pi;
            childAlive = true;
            WaitForSingleObject(pi.hProcess, INFINITE);
            childAlive = false;
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        SetConsoleMode(hIn, oldMode);
        return; 
    }

    if (isExe) {
        std::string exec =
            "\"" + std::string(found) + "\"" +
            (args.empty() ? "" : " " + args);

        ok = CreateProcessA(
            NULL,
            &exec[0],
            NULL, NULL,
            TRUE,
            CREATE_NEW_PROCESS_GROUP,
            NULL, NULL,
            &si, &pi
        );

        if (ok) {
            child = pi;
            childAlive = true;
            WaitForSingleObject(pi.hProcess, INFINITE);
            childAlive = false;
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        SetConsoleMode(hIn, oldMode);
        return; 
    }
    {
        std::string cmdExec = "cmd.exe /c " + cmd + " 2>nul";

        ok = CreateProcessA(
            NULL,
            &cmdExec[0],
            NULL, NULL,
            TRUE,
            CREATE_NEW_PROCESS_GROUP,
            NULL, NULL,
            &si,
            &pi
        );

        if (ok) {
            child = pi;
            childAlive = true;
            WaitForSingleObject(pi.hProcess, INFINITE);
            childAlive = false;
            DWORD exitCode = 1;
            GetExitCodeProcess(pi.hProcess, &exitCode);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            if (exitCode == 0) {
                SetConsoleMode(hIn, oldMode);
                
                return; 
            }
        }
    }


    {
        setShellConsoleMode();
        std::string psExec =
            "powershell -NoProfile -Command " + cmd;

        if (CreateProcessA(
            NULL,
            &psExec[0],
            NULL, NULL,
            TRUE,
            CREATE_NEW_PROCESS_GROUP,
            NULL, NULL,
            &si, &pi
        )) {
            child = pi;
            childAlive = true;
            WaitForSingleObject(pi.hProcess, INFINITE);
            childAlive = false;
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    SetConsoleMode(hIn, oldMode);
    setShellConsoleMode();

    setTitle();

}



void enableCopyMode() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hIn, &mode);

    mode |= ENABLE_EXTENDED_FLAGS;
    mode |= ENABLE_QUICK_EDIT_MODE;
    mode |= ENABLE_MOUSE_INPUT;      
    mode &= ~ENABLE_PROCESSED_INPUT; 

    SetConsoleMode(hIn, mode);
}
vector<string> history(10);
int historyIndex = -1;
int historySize = 0;
void pushHistory(const string& cmd) {
    if (cmd.empty()) return;


    if (!history.empty() && history[0] == cmd)
        return;

    for (int i = 9; i > 0; --i)
        history[i] = history[i - 1];

    history[0] = cmd;

    if (historySize < 10)
        historySize++;

    historyIndex = -1; 
}
BOOL WINAPI CtrlHandler(DWORD ctrlType)
{
    switch (ctrlType)
    {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        {
            if (childAlive && child.hProcess)
            {

                SetConsoleCtrlHandler(NULL, TRUE);

                GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);


                WaitForSingleObject(child.hProcess, 1000);


                DWORD code = 0;
                if (GetExitCodeProcess(child.hProcess, &code) &&
                    code == STILL_ACTIVE)
                {
                    TerminateProcess(child.hProcess, 0);
                }

                SetConsoleCtrlHandler(NULL, FALSE);
                return TRUE;
            }
            return TRUE;
        }

        case CTRL_CLOSE_EVENT:
        {
            if (childAlive && child.hProcess)
            {
                SetConsoleCtrlHandler(NULL, TRUE);

                GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
                WaitForSingleObject(child.hProcess, 800);

                DWORD code = 0;
                if (GetExitCodeProcess(child.hProcess, &code) &&
                    code == STILL_ACTIVE)
                {
                    TerminateProcess(child.hProcess, 0);
                }

                SetConsoleCtrlHandler(NULL, FALSE);


                return TRUE;
            }
            return TRUE;
        }
    }

    return FALSE;
}

void fixWrappedCursor()
{
    CONSOLE_SCREEN_BUFFER_INFO cs;
    GetConsoleScreenBufferInfo(hOut, &cs);

    SHORT width = cs.dwSize.X;

    if (inputStartX + cursorPos >= width) {
        SHORT linesDown = (inputStartX + cursorPos) / width;
        inputStartX = (inputStartX + cursorPos) % width;

        COORD newPos;
        newPos.X = inputStartX;
        newPos.Y = cs.dwCursorPosition.Y + linesDown;

        SetConsoleCursorPosition(hOut, newPos);
    }
}


int main(int argc, char* argv[]) {
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    enableCopyMode();
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
    setTitle();
    setBetterFont();
    isRoot = isRunningAsAdmin();
    handleArgs(argc,argv);
    loadGhostCommands();
    while (running) {
        buffer.clear(); ghost.clear(); cursorPos = 0;
        drawPrompt();

        while (true) {
            int ch = _getch();

            if (ch == 13) break;      
            

            if (ch == 224) {
                ch = _getch();
                if (ch == 75 && cursorPos > 0) cursorPos--;       
                if (ch == 77 && cursorPos < buffer.size()) cursorPos++; 
                if (ch == 72) { 
                    if (historySize == 0) continue;
                    if (historyIndex + 1 < historySize)
                        historyIndex++;
                    buffer = history[historyIndex];
                    cursorPos = buffer.size();
                    updateGhost();
                    redrawLine();
                    continue;
                }
                if (ch == 80) { 
                    if (historyIndex > 0) {
                        historyIndex--;
                        buffer = history[historyIndex];
                    } else {
                        historyIndex = -1;
                        buffer.clear();
                    }
                    cursorPos = buffer.size();
                    updateGhost();
                    redrawLine();
                    continue;
                }
                redrawLine(); continue;
            }


            if (ch == 8) {
                if (cursorPos > 0) {
                    buffer.erase(cursorPos - 1, 1);
                    cursorPos--;
                }
                updateGhost(); redrawLine(); continue;
            }

            if (ch == 9 && !ghost.empty()) {
                buffer += ghost;
                cursorPos = buffer.size();
                ghost.clear();
                redrawLine(); continue;
            }

            if (isprint(ch)) {
                buffer.insert(buffer.begin() + cursorPos, (char)ch);
                cursorPos++;
                updateGhost(); redrawLine();
            }
        }

        cout << "\n";
        if (!buffer.empty())
            pushHistory(buffer);
            executeCommand(buffer);
    }
}
