#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include "PieceTable.cpp"


// ---------------- Unit Tests ----------------
std::string run_tests() {
    std::ostringstream oss;
    auto check = [&](const std::string& name, const std::string& got, const std::string& expect) {
        if (got == expect) {
            oss << name << ": PASS\n";
        } else {
            oss << name << ": FAIL (got=\"" << got << "\", expect=\"" << expect << "\")\n";
        }
    };

    PieceTable pt;
    pt.insert(0, "Hello");
    check("Insert in Empty", pt.get_text(), "Hello");

    pt.insert(5, " World");
    check("Insert in Mid", pt.get_text(), "Hello World");

    pt.erase(0, 5);
    check("Delete the Beginning", pt.get_text(), " World");

    pt.insert(0, "Hi");
    check("Mixed Insertion", pt.get_text(), "Hi World");

    pt.erase(2, 1);
    check("Delete the Mid", pt.get_text(), "HiWorld");

    pt.erase(5, 1);
    check("Delete the End", pt.get_text(), "HiWord");

    return oss.str();
}

// ---------------- Win32 GUI ----------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static std::string test_results;
    switch (msg) {
    case WM_CREATE:
        test_results = run_tests();
        MessageBoxA(hwnd, test_results.c_str(), "Test Result", MB_OK);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        SetBkMode(hdc, TRANSPARENT);
        DrawTextA(hdc, test_results.c_str(), -1, &rect,
                  DT_LEFT | DT_TOP | DT_WORDBREAK);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    static TCHAR CLASS_NAME[] = TEXT("PieceTableWinClass");
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "Piece Table Unit Tests",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        600, 400, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
