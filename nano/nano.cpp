#include <windows.h>
#include <fstream>
#include <vector>
#include <string>

using namespace std;

HANDLE hIn, hOut;
vector<string> buffer;
string filename;
int cx = 0, cy = 0;
CONSOLE_SCREEN_BUFFER_INFO csbi;

/* ================= SCREEN ================= */

void clearScreen() {
    GetConsoleScreenBufferInfo(hOut, &csbi);
    DWORD size = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD written;
    COORD home = {0,0};

    FillConsoleOutputCharacter(hOut, ' ', size, home, &written);
    FillConsoleOutputAttribute(hOut, csbi.wAttributes, size, home, &written);
    SetConsoleCursorPosition(hOut, home);
}

void draw() {
    clearScreen();
    for (int i = 0; i < buffer.size(); i++) {
        DWORD written;
        WriteConsoleA(hOut, buffer[i].c_str(), buffer[i].size(), &written, nullptr);
        WriteConsoleA(hOut, "\n", 1, &written, nullptr);
    }

    COORD pos = {(SHORT)cx, (SHORT)cy};
    SetConsoleCursorPosition(hOut, pos);
}

/* ================= FILE ================= */

void loadFile() {
    ifstream f(filename);
    string line;
    if (!f) {
        buffer.push_back("");
        return;
    }
    while (getline(f, line))
        buffer.push_back(line);
    if (buffer.empty())
        buffer.push_back("");
}

void saveFile() {
    ofstream f(filename, ios::binary);
    for (int i = 0; i < buffer.size(); i++) {
        f << buffer[i];
        if (i + 1 < buffer.size()) f << "\n";
    }
}

/* ================= CLIPBOARD ================= */
bool running = true;
void copyLine() {
    OpenClipboard(nullptr);
    EmptyClipboard();
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, buffer[cy].size() + 1);
    memcpy(GlobalLock(hMem), buffer[cy].c_str(), buffer[cy].size() + 1);
    GlobalUnlock(hMem);
    SetClipboardData(CF_TEXT, hMem);
    CloseClipboard();
}

void pasteClipboard() {
    OpenClipboard(nullptr);
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData) {
        char* text = (char*)GlobalLock(hData);
        if (text) {
            buffer[cy].insert(cx, text);
            cx += strlen(text);
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
}

/* ================= MAIN ================= */

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    filename = argv[1];

    hIn = GetStdHandle(STD_INPUT_HANDLE);
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD mode;
    GetConsoleMode(hIn, &mode);
    mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    SetConsoleMode(hIn, mode);

    loadFile();
    draw();

    while (running) {
        INPUT_RECORD rec;
        DWORD read;
        ReadConsoleInput(hIn, &rec, 1, &read);

        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown)
            continue;

        auto &k = rec.Event.KeyEvent;
        bool ctrl = k.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED);
        bool shift = k.dwControlKeyState & SHIFT_PRESSED;

        if (k.wVirtualKeyCode == VK_ESCAPE ||
        (ctrl && k.wVirtualKeyCode == 'Q')) {
        break;
        }


    if (ctrl && k.wVirtualKeyCode == 'S') {
        saveFile();
        draw();
        continue;
    }


        switch (k.wVirtualKeyCode) {
        case VK_LEFT:
            if (cx > 0) cx--;
            break;
        case VK_RIGHT:
            if (cx < buffer[cy].size()) cx++;
            break;
        case VK_UP:
            if (cy > 0) {
                cy--;
                cx = min(cx, (int)buffer[cy].size());
            }
            break;
        case VK_DOWN:
            if (cy + 1 < buffer.size()) {
                cy++;
                cx = min(cx, (int)buffer[cy].size());
            }
            break;
        case VK_RETURN: {
            string rest = buffer[cy].substr(cx);
            buffer[cy] = buffer[cy].substr(0, cx);
            buffer.insert(buffer.begin() + cy + 1, rest);
            cy++; cx = 0;
            break;
        }
        case VK_BACK:
            if (cx > 0) {
                buffer[cy].erase(cx - 1, 1);
                cx--;
            } else if (cy > 0) {
                cx = buffer[cy - 1].size();
                buffer[cy - 1] += buffer[cy];
                buffer.erase(buffer.begin() + cy);
                cy--;
            }
            break;
        default:
            if (k.uChar.AsciiChar >= 32) {
                buffer[cy].insert(cx, 1, k.uChar.AsciiChar);
                cx++;
            }
        }

        draw();
    }

    saveFile();
    return 0;
}
