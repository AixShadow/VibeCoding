#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include "TextRender.cpp"


void UpdateScrollInfo(HWND hwnd, const PieceTable& table, TextRenderer& renderer, int& scrollY) {
    HDC hdc = GetDC(hwnd);
    RECT rect;
    GetClientRect(hwnd, &rect);
    int maxWidth = rect.right - 20;
    if (maxWidth < 1) maxWidth = 1;

    int totalHeight = renderer.GetTotalHeight(hdc, table, maxWidth) + 20; // 附加些许底边距防止紧贴
    ReleaseDC(hwnd, hdc);

    SCROLLINFO si = {};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0;
    si.nMax = totalHeight;
    si.nPage = rect.bottom;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

    int maxScrollPos = si.nMax - si.nPage + 1;
    if (maxScrollPos < 0) maxScrollPos = 0;
    if (scrollY > maxScrollPos) scrollY = maxScrollPos;
    if (scrollY < 0) scrollY = 0;

    si.fMask = SIF_POS;
    si.nPos = scrollY;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

void EnsureCursorVisible(HWND hwnd, const PieceTable& table, TextRenderer& renderer, int cursorPos, int& scrollY) {
    HDC hdc = GetDC(hwnd);
    RECT rect;
    GetClientRect(hwnd, &rect);
    int maxWidth = rect.right - 20;
    if (maxWidth < 1) maxWidth = 1;
    
    // 探测相对源点的绝对Y坐标
    POINT pt = renderer.GetCursorCoordinate(hdc, table, 10, 10, maxWidth, cursorPos);
    int lineHeight = renderer.GetLineHeight(hdc);
    ReleaseDC(hwnd, hdc);

    bool needsScroll = false;
    int oldScroll = scrollY;
    
    if (pt.y < scrollY) {
        scrollY = pt.y - 10;
        if (scrollY < 0) scrollY = 0;
        needsScroll = true;
    } else if (pt.y + lineHeight > scrollY + rect.bottom) {
        scrollY = pt.y + lineHeight - rect.bottom + 10;
        needsScroll = true;
    }

    if (needsScroll) {
        SCROLLINFO si = {};
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);
        
        int maxScrollPos = si.nMax - si.nPage + 1;
        if (maxScrollPos < 0) maxScrollPos = 0;
        if (scrollY > maxScrollPos) scrollY = maxScrollPos;
        
        si.fMask = SIF_POS;
        si.nPos = scrollY;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        
        int dy = oldScroll - scrollY;
        ScrollWindowEx(hwnd, 0, dy, nullptr, nullptr, nullptr, nullptr, SW_INVALIDATE | SW_ERASE);
    }
}

void UpdateCaretPosition(HWND hwnd, const PieceTable& table, TextRenderer& renderer, int cursorPos, int scrollY) {
    HDC hdc = GetDC(hwnd);
    RECT rect;
    GetClientRect(hwnd, &rect);
    int maxWidth = rect.right - 20;
    if (maxWidth < 1) maxWidth = 1;
    
    POINT pt = renderer.GetCursorCoordinate(hdc, table, 10, 10 - scrollY, maxWidth, cursorPos);
    SetCaretPos(pt.x, pt.y);
    ReleaseDC(hwnd, hdc);
}

// ---------------- Unit Tests ----------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static PieceTable table;
    static TextRenderer renderer;
    static int cursorPos = 0;
    static int scrollY = 0;
    
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
        UpdateCaretPosition(hwnd, table, renderer, cursorPos, scrollY);
        ShowCaret(hwnd);
        return 0;
    }
    case WM_KILLFOCUS: {
        HideCaret(hwnd);
        DestroyCaret();
        return 0;
    }
    case WM_SIZE: {
        UpdateScrollInfo(hwnd, table, renderer, scrollY);
        UpdateCaretPosition(hwnd, table, renderer, cursorPos, scrollY);
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
            POINT pt = renderer.GetCursorCoordinate(hdc, table, 10, 10 - scrollY, maxWidth, cursorPos);
            int lineHeight = renderer.GetLineHeight(hdc);
            
            if (wParam == VK_UP) pt.y -= lineHeight;
            else pt.y += lineHeight;
            
            cursorPos = renderer.GetPosFromCoordinate(hdc, table, 10, 10 - scrollY, maxWidth, pt.x, pt.y);
            ReleaseDC(hwnd, hdc);
            moved = true;
        } else if (wParam == VK_HOME) {
            cursorPos = 0;
            moved = true;
        } else if (wParam == VK_END) {
            cursorPos = maxPos;
            moved = true;
        }
        
        if (moved) {
            EnsureCursorVisible(hwnd, table, renderer, cursorPos, scrollY);
            UpdateCaretPosition(hwnd, table, renderer, cursorPos, scrollY);
        }
        return 0;
    }
    case WM_CHAR: {
        if (wParam == VK_BACK) {
            if (cursorPos > 0) {
                table.erase(cursorPos - 1, 1);
                cursorPos--;
                UpdateScrollInfo(hwnd, table, renderer, scrollY);
                EnsureCursorVisible(hwnd, table, renderer, cursorPos, scrollY);
                UpdateCaretPosition(hwnd, table, renderer, cursorPos, scrollY);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
        } else if (wParam == VK_RETURN) {
            table.insert(cursorPos, "\n");
            cursorPos++;
            UpdateScrollInfo(hwnd, table, renderer, scrollY);
            EnsureCursorVisible(hwnd, table, renderer, cursorPos, scrollY);
            UpdateCaretPosition(hwnd, table, renderer, cursorPos, scrollY);
            InvalidateRect(hwnd, nullptr, TRUE);
        } else if (wParam >= 32 && wParam <= 255) { 
            char c = (char)wParam;
            std::string s(1, c);
            table.insert(cursorPos, s);
            cursorPos++;
            UpdateScrollInfo(hwnd, table, renderer, scrollY);
            EnsureCursorVisible(hwnd, table, renderer, cursorPos, scrollY);
            UpdateCaretPosition(hwnd, table, renderer, cursorPos, scrollY);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }
    case WM_VSCROLL: {
        SCROLLINFO si = {};
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);
        
        int yPos = si.nPos;
        int oldYPos = yPos;
        
        switch (LOWORD(wParam)) {
            case SB_TOP: yPos = si.nMin; break;
            case SB_BOTTOM: yPos = si.nMax; break;
            case SB_LINEUP: yPos -= 20; break;
            case SB_LINEDOWN: yPos += 20; break;
            case SB_PAGEUP: yPos -= si.nPage; break;
            case SB_PAGEDOWN: yPos += si.nPage; break;
            case SB_THUMBTRACK:
            case SB_THUMBPOSITION: 
                yPos = si.nTrackPos; 
                break;
        }
        
        // 关键计算公式：MaxScrollPos = TotalLines - VisibleLines (以像素近似)
        int maxScrollPos = si.nMax - si.nPage + 1;
        if (maxScrollPos < 0) maxScrollPos = 0;
        if (yPos > maxScrollPos) yPos = maxScrollPos;
        if (yPos < 0) yPos = 0;
        
        if (yPos != oldYPos) {
            int dy = oldYPos - yPos;
            scrollY = yPos;
            
            si.fMask = SIF_POS;
            si.nPos = scrollY;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            
            ScrollWindowEx(hwnd, 0, dy, nullptr, nullptr, nullptr, nullptr, SW_INVALIDATE | SW_ERASE);
            UpdateCaretPosition(hwnd, table, renderer, cursorPos, scrollY);
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        int scrollLines = 3;
        SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0);
        
        HDC hdc = GetDC(hwnd);
        int step = renderer.GetLineHeight(hdc) * scrollLines;
        ReleaseDC(hwnd, hdc);

        SCROLLINFO si = {};
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);
        
        int yPos = si.nPos - (zDelta / WHEEL_DELTA) * step;
        
        int maxScrollPos = si.nMax - si.nPage + 1;
        if (maxScrollPos < 0) maxScrollPos = 0;
        if (yPos > maxScrollPos) yPos = maxScrollPos;
        if (yPos < 0) yPos = 0;
        
        if (yPos != si.nPos) {
            int dy = si.nPos - yPos;
            scrollY = yPos;
            
            si.fMask = SIF_POS;
            si.nPos = scrollY;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            
            ScrollWindowEx(hwnd, 0, dy, nullptr, nullptr, nullptr, nullptr, SW_INVALIDATE | SW_ERASE);
            UpdateCaretPosition(hwnd, table, renderer, cursorPos, scrollY);
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
        renderer.Draw(hdc, table, 10, 10 - scrollY, maxWidth);
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
        WS_OVERLAPPEDWINDOW | WS_VSCROLL, CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}