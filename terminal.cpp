#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <conio.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;
bool isRoot = false;
/* ================= Font fix ================= */
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
/* ================= Console ================= */
HANDLE hOut;
SHORT inputStartX = 0;
bool autocompleteEnabled = true;
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

/* ================= Prompt ================= */
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



void drawPrompt() {
    CONSOLE_SCREEN_BUFFER_INFO cs;
    GetConsoleScreenBufferInfo(hOut, &cs);
    if(isRoot){
        setColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
        cout<< "kali@root";
        resetColor();
        cout<< ":~$ ";
    }
    else{
    setColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    cout << "kali@user";
    resetColor();
    cout << ":~$ ";
    }
    inputStartX = cs.dwCursorPosition.X + PROMPT.size();
}

/* ================= Input ================= */
string buffer, ghost;
size_t cursorPos = 0;

/* ================= Utils ================= */
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
/* ================= Autocomplete ================= */
vector<string> baseCmds = {"clear","pwd","ping ","whoami","ifconfig","cls","dir","ls","bash","cd","sudo su","apt","apt install ","apt search ","apt list","apt uninstall"};

vector<string> currentDirFiles() {
    vector<string> r;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("*", &fd);
    if (h == INVALID_HANDLE_VALUE) return r;
    do {
        if (string(fd.cFileName) != "." && string(fd.cFileName) != "..")
            r.push_back(fd.cFileName);
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

/* ================= Redraw ================= */
void redrawLine() {
    CONSOLE_SCREEN_BUFFER_INFO cs;
    GetConsoleScreenBufferInfo(hOut, &cs);

    COORD start{inputStartX, cs.dwCursorPosition.Y};
    SetConsoleCursorPosition(hOut, start);

    DWORD w;
    FillConsoleOutputCharacterA(
        hOut, ' ',
        cs.dwSize.X - inputStartX,
        start, &w
    );

    SetConsoleCursorPosition(hOut, start);
    cout << buffer;

    if (!ghost.empty()) {
        setColor(FOREGROUND_INTENSITY);
        cout << ghost;
        resetColor();
    }

    COORD cur{(SHORT)(inputStartX + cursorPos), start.Y};
    SetConsoleCursorPosition(hOut, cur);
}

/* ================= ls / dir ================= */
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

        if (isDir)
            setColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        else if (isExe) {
            setColor(FOREGROUND_BLUE | FOREGROUND_INTENSITY);
            n = n.substr(0, n.size() - 4);
        }

        cout << n << "\n";
        resetColor();

    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

/* ================= Tool resolver ================= */
string resolveTool(const string& cmd, string& workdir, bool& isPython) {
    isPython = false;
    auto paths = splitPATH();
    paths.push_back(".");

    for (auto& p : paths) {
        string exe = p + "\\" + cmd + ".exe";
        if (fileExists(exe)) {
            workdir = p;
            return "\"" + exe + "\""; // 🔥 exe كما هو
        }

        string py = p + "\\" + cmd + ".py";
        if (fileExists(py)) {
            workdir = p;
            isPython = true;
            return "\"" + py + "\"";  // نرجع الملف فقط
        }
    }

    workdir.clear();
    return cmd; // builtin أو أمر عادي
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
    sei.lpFile = exePath;          // 🔥 تطبيقك نفسه
    sei.lpParameters = params.c_str();
    sei.lpDirectory  = cwd;        // 🔥 نفس المسار الحالي
    sei.nShow = SW_SHOW;

    if (ShellExecuteExA(&sei)) {
        ExitProcess(0); // نغلق النسخة العادية بدون قتل الكونسول الجديدة
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

    // طلب admin ونحن لسنا admin → إعادة تشغيل
    if (wantAdmin && !isRunningAsAdmin()) {
        string extra;
        if (hasCommand) {
            extra = "-c \"" + command + "\"";
        }
        relaunchAsAdmin(extra);
        return;
    }

    // نحن هنا:
    // - إما admin
    // - أو لا نحتاج admin

    if (wantAdmin && isRunningAsAdmin()) {
        isRoot = true;
    }

    // تنفيذ -c
    if (hasCommand) {
        executeCommand(command);
        running = false; // مثل cmd /c
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
    mode &= ~ENABLE_PROCESSED_INPUT; // فقط هنا

    SetConsoleMode(hIn, mode);
}
void setInteractiveConsoleMode() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = originalConsoleMode;

    mode |= ENABLE_PROCESSED_INPUT;   // 🔥 ضروري
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

    // وراثة stdin / stdout / stderr
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
        TRUE,   // مهم: توريث الـ handles
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

    // sudo su
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
    
    // ghost add <cmd>
    if (cmdline.rfind("//ghost add ", 0) == 0) {
        string cmd = cmdline.substr(11); // بعد "ghost add "
        ghostAdd(trim(cmd));
        return;
    }

    // ghost remove <cmd>
    if (cmdline.rfind("//ghost remove ", 0) == 0) {
        string cmd = cmdline.substr(14); // بعد "ghost remove "
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
        SetCurrentDirectoryA(cmdline.substr(3).c_str());
        return;
    }
    std::string cmd = trim(input);
    if (cmd.empty()) return;

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD oldMode = 0;
    GetConsoleMode(hIn, &oldMode);

    // فصل الأمر عن الوسائط
    std::string base = cmd.substr(0, cmd.find(' '));
    std::string args = (cmd.find(' ') != std::string::npos)
        ? cmd.substr(cmd.find(' ') + 1)
        : "";

    char found[MAX_PATH]{};
    bool isExe = false;
    bool isPy  = false;

    if (SearchPathA(NULL, (base + ".exe").c_str(), NULL, MAX_PATH, found, NULL))
        isExe = true;
    else if (SearchPathA(NULL, (base + ".py").c_str(), NULL, MAX_PATH, found, NULL))
        isPy = true;
    setInteractiveConsoleMode();
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
    BOOL ok = FALSE;

    /* =====================================================
       1️⃣ Python script → تشغيل مباشر ثم RETURN
       ===================================================== */
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
        return; // 🔥 مهم جداً
    }

    /* =====================================================
       2️⃣ EXE حقيقي → تشغيل مباشر ثم RETURN
       ===================================================== */
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
        return; // 🔥 مهم جداً
    }

    /* =====================================================
       3️⃣ CMD داخلي (بدون إظهار خطأ)
       ===================================================== */
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
                
                return; // نجح CMD → لا نكمل
            }
        }
    }

    /* =====================================================
       4️⃣ PowerShell فقط إذا CMD فشل
       ===================================================== */
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
    mode |= ENABLE_MOUSE_INPUT;      // مهم
    mode &= ~ENABLE_PROCESSED_INPUT; // مهم جدًا

    SetConsoleMode(hIn, mode);
}
vector<string> history(10);
int historyIndex = -1;
int historySize = 0;
void pushHistory(const string& cmd) {
    if (cmd.empty()) return;

    // منع تكرار نفس الأمر مباشرة
    if (!history.empty() && history[0] == cmd)
        return;

    for (int i = 9; i > 0; --i)
        history[i] = history[i - 1];

    history[0] = cmd;

    if (historySize < 10)
        historySize++;

    historyIndex = -1; // إعادة المؤشر
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
                // نمنع التيرمينال من استقبال Ctrl+C
                SetConsoleCtrlHandler(NULL, TRUE);

                // نرسل Ctrl+C لكل العمليات المرتبطة بالكونسول
                // (أفضل خيار بدون تعديل executeCommand)
                GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);

                // ننتظر قليلًا
                WaitForSingleObject(child.hProcess, 1000);

                // إذا ما توقف → نقتله قسريًا
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

                // ❗ نمنع إغلاق التيرمينال
                return TRUE;
            }
            return TRUE;
        }
    }

    return FALSE;
}


/* ================= Main ================= */
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

            if (ch == 13) break;      // Enter
            

            if (ch == 224) {
                ch = _getch();
                if (ch == 75 && cursorPos > 0) cursorPos--;        // ←
                if (ch == 77 && cursorPos < buffer.size()) cursorPos++; // →
                if (ch == 72) { // UP
                    if (historySize == 0) continue;
                    if (historyIndex + 1 < historySize)
                        historyIndex++;
                    buffer = history[historyIndex];
                    cursorPos = buffer.size();
                    updateGhost();
                    redrawLine();
                    continue;
                }
                if (ch == 80) { // DOWN
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
