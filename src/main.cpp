#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include "TextRender.cpp"


void UpdateCaretPosition(HWND hwnd, const PieceTable& table, TextRenderer& renderer, int cursorPos) {
    HDC hdc = GetDC(hwnd);
    RECT rect;
    GetClientRect(hwnd, &rect);
    int maxWidth = rect.right - 20;
    if (maxWidth < 1) maxWidth = 1;
    
    POINT pt = renderer.GetCursorCoordinate(hdc, table, 10, 10, maxWidth, cursorPos);
    SetCaretPos(pt.x, pt.y);
    ReleaseDC(hwnd, hdc);
}

// ---------------- Unit Tests ----------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static PieceTable table;
    static TextRenderer renderer;
    static int cursorPos = 0;
    
    switch (msg) {
    case WM_CREATE:
        table.insert(0, "This is a simple test of the TextRenderer class."
                        "It should automatically wrap words when they exceed the maximum width."
                        "HereIsAVeryLongWordThatShouldBeSplitAcrossLinesAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA."
                        "Also test spaces and\nexplicit newlines.");
        cursorPos = table.get_text().length();
        return 0;
    case WM_SETFOCUS: {
        HDC hdc = GetDC(hwnd);
        int lineHeight = renderer.GetLineHeight(hdc);
        ReleaseDC(hwnd, hdc);
        CreateCaret(hwnd, nullptr, 2, lineHeight);
        UpdateCaretPosition(hwnd, table, renderer, cursorPos);
        ShowCaret(hwnd);
        return 0;
    }
    case WM_KILLFOCUS: {
        HideCaret(hwnd);
        DestroyCaret();
        return 0;
    }
    case WM_SIZE: {
        UpdateCaretPosition(hwnd, table, renderer, cursorPos);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }
    case WM_KEYDOWN: {
        std::string text = table.get_text();
        int maxPos = (int)text.length();
        bool moved = false;
        if (wParam == VK_LEFT) {
            if (cursorPos > 0) {
                cursorPos--;
                moved = true;
            }
        } else if (wParam == VK_RIGHT) {
            if (cursorPos < maxPos) {
                cursorPos++;
                moved = true;
            }
        } else if (wParam == VK_UP || wParam == VK_DOWN) {
            HDC hdc = GetDC(hwnd);
            RECT rect;
            GetClientRect(hwnd, &rect);
            int maxWidth = rect.right - 20;
            if (maxWidth < 1) maxWidth = 1;
            POINT pt = renderer.GetCursorCoordinate(hdc, table, 10, 10, maxWidth, cursorPos);
            int lineHeight = renderer.GetLineHeight(hdc);
            
            if (wParam == VK_UP) pt.y -= lineHeight;
            else pt.y += lineHeight;
            
            cursorPos = renderer.GetPosFromCoordinate(hdc, table, 10, 10, maxWidth, pt.x, pt.y);
            ReleaseDC(hwnd, hdc);
            moved = true;
        } else if (wParam == VK_HOME) {
            cursorPos = 0;
            moved = true;
        } else if (wParam == VK_END) {
            cursorPos = maxPos;
            moved = true;
        }
        
        if (moved) UpdateCaretPosition(hwnd, table, renderer, cursorPos);
        return 0;
    }
    case WM_CHAR: {
        if (wParam == VK_BACK) {
            if (cursorPos > 0) {
                table.erase(cursorPos - 1, 1);
                cursorPos--;
                UpdateCaretPosition(hwnd, table, renderer, cursorPos);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
        } else if (wParam == VK_RETURN) {
            table.insert(cursorPos, "\n");
            cursorPos++;
            UpdateCaretPosition(hwnd, table, renderer, cursorPos);
            InvalidateRect(hwnd, nullptr, TRUE);
        } else if (wParam >= 32 && wParam <= 255) { 
            char c = (char)wParam;
            std::string s(1, c);
            table.insert(cursorPos, s);
            cursorPos++;
            UpdateCaretPosition(hwnd, table, renderer, cursorPos);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        int maxWidth = rect.right - 20;
        if (maxWidth < 1) maxWidth = 1;
        SetBkMode(hdc, TRANSPARENT);
        renderer.Draw(hdc, table, 10, 10, maxWidth);
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
    const char CLASS_NAME[] = "TextRendererWinClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "TextRenderer Demo",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}