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
    static int selectionAnchor = 0; // 选区起始点，与 cursorPos 构成选区范围
    static int scrollY = 0;
    
    switch (msg) {
    case WM_CREATE:
        table.insert(0, "This is a simple test of the TextRenderer class."
                        "It should automatically wrap words when they exceed the maximum width."
                        "HereIsAVeryLongWordThatShouldBeSplitAcrossLinesAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA."
                        "Also test spaces and\nexplicit newlines.");
        renderer.InvalidateCache(0);
        cursorPos = table.get_text().length();
        selectionAnchor = cursorPos;
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
            // 简单的导航逻辑：移动光标时重置选区 (如需 Shift 选择需在此添加判断)
            if (GetKeyState(VK_SHIFT) >= 0) {
                selectionAnchor = cursorPos;
            }
            EnsureCursorVisible(hwnd, table, renderer, cursorPos, scrollY);
            UpdateCaretPosition(hwnd, table, renderer, cursorPos, scrollY);
            InvalidateRect(hwnd, nullptr, TRUE); // 重绘以更新选区显示
        }
        return 0;
    }
    case WM_CHAR: {
        // 处理选区替换逻辑
        if (selectionAnchor != cursorPos) {
            int start = std::min(selectionAnchor, cursorPos);
            int end = std::max(selectionAnchor, cursorPos);
            renderer.InvalidateCache(start);
            table.erase(start, end - start);
            cursorPos = start;
            selectionAnchor = cursorPos; // 选区已删除，重置
            // 如果是退格键，删除选区后即可结束；如果是普通字符，继续执行插入
            if (wParam == VK_BACK) {
                UpdateScrollInfo(hwnd, table, renderer, scrollY);
                EnsureCursorVisible(hwnd, table, renderer, cursorPos, scrollY);
                UpdateCaretPosition(hwnd, table, renderer, cursorPos, scrollY);
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0; 
            }
        }

        if (wParam == VK_BACK) {
            if (cursorPos > 0) {
                renderer.InvalidateCache(cursorPos - 1);
                table.erase(cursorPos - 1, 1);
                cursorPos--;
                UpdateScrollInfo(hwnd, table, renderer, scrollY);
                EnsureCursorVisible(hwnd, table, renderer, cursorPos, scrollY);
                UpdateCaretPosition(hwnd, table, renderer, cursorPos, scrollY);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
        } else if (wParam == VK_RETURN) {
            renderer.InvalidateCache(cursorPos);
            table.insert(cursorPos, "\n");
            cursorPos++;
            UpdateScrollInfo(hwnd, table, renderer, scrollY);
            EnsureCursorVisible(hwnd, table, renderer, cursorPos, scrollY);
            UpdateCaretPosition(hwnd, table, renderer, cursorPos, scrollY);
            InvalidateRect(hwnd, nullptr, TRUE);
        } else if (wParam >= 32 && wParam <= 255) { 
            char c = (char)wParam;
            std::string s(1, c);
            renderer.InvalidateCache(cursorPos);
            table.insert(cursorPos, s);
            cursorPos++;
            selectionAnchor = cursorPos; // 插入后重置选区锚点
            UpdateScrollInfo(hwnd, table, renderer, scrollY);
            EnsureCursorVisible(hwnd, table, renderer, cursorPos, scrollY);
            UpdateCaretPosition(hwnd, table, renderer, cursorPos, scrollY);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        HDC hdc = GetDC(hwnd);
        RECT rect;
        GetClientRect(hwnd, &rect);
        int maxWidth = rect.right - 20;
        if (maxWidth < 1) maxWidth = 1;

        // 点击时更新光标位置并重置选区起点
        cursorPos = renderer.GetPosFromCoordinate(hdc, table, 10, 10 - scrollY, maxWidth, x, y);
        // 如果按住 Shift，则保留 anchor 扩展选区，否则重置
        if (GetKeyState(VK_SHIFT) >= 0) {
            selectionAnchor = cursorPos;
        }
        
        ReleaseDC(hwnd, hdc);
        
        SetCapture(hwnd); // 捕获鼠标以便拖拽到窗口外
        UpdateCaretPosition(hwnd, table, renderer, cursorPos, scrollY);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (wParam & MK_LBUTTON) { // 鼠标左键拖拽中
            int x = (short)LOWORD(lParam); // 转为 short 以处理负坐标
            int y = (short)HIWORD(lParam);
            HDC hdc = GetDC(hwnd);
            RECT rect;
            GetClientRect(hwnd, &rect);
            int maxWidth = rect.right - 20;
            if (maxWidth < 1) maxWidth = 1;

            // 拖拽时只更新 cursorPos，selectionAnchor 保持不变，从而形成选区
            cursorPos = renderer.GetPosFromCoordinate(hdc, table, 10, 10 - scrollY, maxWidth, x, y);
            ReleaseDC(hwnd, hdc);

            EnsureCursorVisible(hwnd, table, renderer, cursorPos, scrollY); // 支持拖拽自动滚动
            UpdateCaretPosition(hwnd, table, renderer, cursorPos, scrollY);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        ReleaseCapture();
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
        
        int sStart = -1, sEnd = -1;
        if (selectionAnchor != cursorPos) {
            sStart = std::min(selectionAnchor, cursorPos);
            sEnd = std::max(selectionAnchor, cursorPos);
        }
        renderer.Draw(hdc, table, 10, 10 - scrollY, maxWidth, sStart, sEnd);
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