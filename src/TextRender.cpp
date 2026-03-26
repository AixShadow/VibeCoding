#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include "PieceTable.cpp"

class TextRenderer {
    public:
        struct LineInfo {
            int startPos;
            int yPos;
        };

        TextRenderer() {
            lines.push_back({0, 0});
        }

        void InvalidateCache(const PieceTable& table, int pos) {
            cachedText = table.get_text();
            if (lines.empty()) return;
            
            // 找到包含被修改字符 pos 的那一行
            auto it = std::upper_bound(lines.begin(), lines.end(), pos,
                [](int p, const LineInfo& line) { return p < line.startPos; });
            
            if (it != lines.begin()) {
                --it;
            }
            // 失效策略：仅标记受影响行之后的索引为 dirty，保留此前稳定的布局历史
            lines.erase(it + 1, lines.end());
        }

        void EnsureLayout(HDC hdc, int maxWidth) {
            int lineHeight = get_line_height(hdc);

            if (maxWidth != validLineWidth) {
                lines.clear();
                lines.push_back({0, 0});
                validLineWidth = maxWidth;
            }

            if (lines.empty()) {
                lines.push_back({0, 0});
            }

            int currentStart = lines.back().startPos;
            if (currentStart >= (int)cachedText.length()) {
                if (currentStart > 0 && currentStart == (int)cachedText.length() && cachedText.back() == '\n') {
                    // 末尾显式换行的状态
                } else if (currentStart > (int)cachedText.length()) {
                    lines.clear();
                    lines.push_back({0, 0});
                    currentStart = 0;
                } else {
                    return;
                }
            }

            int curX = 0;
            int curPos = currentStart;
            int currentY = lines.back().yPos;

            // 增量式延迟测距：只从被销毁（dirty）之后的起始点开始测距与分行
            while (curPos < (int)cachedText.length()) {
                int wordStart = curPos;
                int wordEnd = wordStart;
                char c = cachedText[wordEnd];
                
                if (c == ' ' || c == '\t' || c == '\n') {
                    wordEnd++;
                } else if (c == '.' || c == ',' || c == '!' || c == '?' || c == ';' || c == ':') {
                    wordEnd++;
                } else {
                    while (wordEnd < (int)cachedText.length()) {
                        char nextC = cachedText[wordEnd];
                        if (nextC == ' ' || nextC == '\t' || nextC == '\n' || 
                            nextC == '.' || nextC == ',' || nextC == '!' || 
                            nextC == '?' || nextC == ';' || nextC == ':') {
                            break;
                        }
                        wordEnd++;
                    }
                }

                std::string word = cachedText.substr(wordStart, wordEnd - wordStart);
                int w = measure_word(hdc, word);

                if (word == "\n") {
                    curX = 0;
                    curPos = wordEnd;
                    currentY += lineHeight;
                    lines.push_back({curPos, currentY});
                    continue;
                }

                if (curX + w > maxWidth) {
                    if (w > maxWidth) {
                        int start = 0;
                        while (start < (int)word.size()) {
                            int end = start + 1;
                            int lastFit = start;
                            int remainingWidth = maxWidth - curX;

                            while (end <= (int)word.size()) {
                                SIZE sz;
                                GetTextExtentPoint32A(hdc, word.c_str() + start, end - start, &sz);
                                if (sz.cx > remainingWidth) break;
                                lastFit = end;
                                ++end;
                            }

                            if (lastFit == start) lastFit = start + 1;

                            SIZE sz;
                            GetTextExtentPoint32A(hdc, word.c_str() + start, lastFit - start, &sz);
                            curX += sz.cx;
                            start = lastFit;

                            if (start < (int)word.size()) {
                                curX = 0;
                                currentY += lineHeight;
                                lines.push_back({wordStart + start, currentY});
                            }
                        }
                    } else {
                        curX = w;
                        currentY += lineHeight;
                        lines.push_back({wordStart, currentY});
                    }
                } else {
                    curX += w;
                }

                curPos = wordEnd;
            }
        }

        void Draw(HDC hdc, const PieceTable& table, int x, int y, int maxWidth, int selStart = -1, int selEnd = -1) {
            EnsureLayout(hdc, maxWidth);
            
            RECT clipBox;
            GetClipBox(hdc, &clipBox);

            // 空间检索优化：利用二分查找快速定位当前视口首行对应的 Piece 节点
            auto itStart = std::lower_bound(lines.begin(), lines.end(), clipBox.top - y, 
                [](const LineInfo& line, int targetY) { return line.yPos < targetY; });
            
            if (itStart != lines.begin()) --itStart; // 向前包容一行以防顶层截断

            auto itEnd = std::lower_bound(lines.begin(), lines.end(), clipBox.bottom - y, 
                [](const LineInfo& line, int targetY) { return line.yPos <= targetY; });
            if (itEnd != lines.end()) ++itEnd;

            COLORREF oldBk = GetBkColor(hdc);
            COLORREF oldTxt = GetTextColor(hdc);
            COLORREF selBk = GetSysColor(COLOR_HIGHLIGHT);
            COLORREF selTxt = GetSysColor(COLOR_HIGHLIGHTTEXT);
            int oldMode = GetBkMode(hdc);

            // 视口剪裁：仅对当前视口高度范围内的 Piece 段执行 DrawText (以行为单位合并输出更疾速)
            for (auto it = itStart; it != itEnd && it != lines.end(); ++it) {
                int startPos = it->startPos;
                int endPos = (it + 1 != lines.end()) ? (it + 1)->startPos : (int)cachedText.length();
                
                int drawLen = endPos - startPos;
                bool hasNewline = false;
                if (drawLen > 0 && cachedText[startPos + drawLen - 1] == '\n') {
                    drawLen--;
                    hasNewline = true;
                }
                
                int currentX = x;
                int currentY = y + it->yPos;

                // 检查选区交集
                int s = -1, e = -1;
                if (selStart != -1 && selEnd != -1 && selStart < selEnd) {
                    s = std::max(startPos, selStart);
                    e = std::min(startPos + drawLen, selEnd);
                }

                if (s != -1 && s < e) {
                    // 1. 选区前 (正常显示)
                    if (startPos < s) {
                        int len = s - startPos;
                        TextOutA(hdc, currentX, currentY, cachedText.c_str() + startPos, len);
                        SIZE sz;
                        GetTextExtentPoint32A(hdc, cachedText.c_str() + startPos, len, &sz);
                        currentX += sz.cx;
                    }

                    // 2. 选区中 (高亮显示)
                    {
                        int len = e - s;
                        SetBkColor(hdc, selBk);
                        SetTextColor(hdc, selTxt);
                        SetBkMode(hdc, OPAQUE);
                        TextOutA(hdc, currentX, currentY, cachedText.c_str() + s, len);
                        SetBkMode(hdc, oldMode); // 恢复透明模式
                        SetBkColor(hdc, oldBk);
                        SetTextColor(hdc, oldTxt);
                        
                        SIZE sz;
                        GetTextExtentPoint32A(hdc, cachedText.c_str() + s, len, &sz);
                        currentX += sz.cx;
                    }

                    // 3. 选区后 (正常显示)
                    if (e < startPos + drawLen) {
                        int len = (startPos + drawLen) - e;
                        TextOutA(hdc, currentX, currentY, cachedText.c_str() + e, len);
                    }
                } else {
                    // 无选中内容，直接绘制整行
                    if (drawLen > 0) {
                        TextOutA(hdc, currentX, currentY, cachedText.c_str() + startPos, drawLen);
                    }
                    // 为了处理换行符高亮，需要计算 currentX
                    if (hasNewline && selStart <= (startPos + drawLen) && selEnd > (startPos + drawLen)) {
                         SIZE sz;
                         GetTextExtentPoint32A(hdc, cachedText.c_str() + startPos, drawLen, &sz);
                         currentX += sz.cx;
                    }
                }

                // 绘制换行符选中效果(光标宽度的色块)
                if (hasNewline && selStart <= (startPos + drawLen) && selEnd > (startPos + drawLen)) {
                    SIZE sz;
                    GetTextExtentPoint32A(hdc, " ", 1, &sz);
                    RECT r = {currentX, currentY, currentX + sz.cx, currentY + get_line_height(hdc)};
                    HBRUSH hBrush = CreateSolidBrush(selBk);
                    FillRect(hdc, &r, hBrush);
                    DeleteObject(hBrush);
                }
            }
        }

        int GetLineHeight(HDC hdc) {
            return get_line_height(hdc);
        }

        int GetTotalHeight(HDC hdc, const PieceTable& table, int maxWidth) {
            EnsureLayout(hdc, maxWidth);
            int lineHeight = get_line_height(hdc);
            return lines.back().yPos + lineHeight;
        }
        

        POINT GetCursorCoordinate(HDC hdc, const PieceTable& table, int x, int y, int maxWidth, int cursorPos) {
            EnsureLayout(hdc, maxWidth);

            auto it = std::upper_bound(lines.begin(), lines.end(), cursorPos, 
                [](int p, const LineInfo& line) { return p < line.startPos; });
            
            if (it != lines.begin()) --it;

            int startPos = it->startPos;
            int drawLen = cursorPos - startPos;
            
            SIZE sz = {0, 0};
            if (drawLen > 0 && drawLen <= (int)cachedText.length() - startPos) {
                GetTextExtentPoint32A(hdc, cachedText.c_str() + startPos, drawLen, &sz);
            }

            return {x + sz.cx, y + it->yPos};
        }

        int GetPosFromCoordinate(HDC hdc, const PieceTable& table, int x, int y, int maxWidth, int targetX, int targetY) {
            EnsureLayout(hdc, maxWidth);

            int relY = targetY - y;
            if (relY < 0) return 0;

            auto it = std::upper_bound(lines.begin(), lines.end(), relY, 
                [](int target, const LineInfo& line) { return target < line.yPos; });
            
            if (it != lines.begin()) --it;

            int startPos = it->startPos;
            int endPos = (it + 1 != lines.end()) ? (it + 1)->startPos : (int)cachedText.length();
            
            int searchLen = endPos - startPos;
            if (searchLen > 0 && cachedText[startPos + searchLen - 1] == '\n') {
                searchLen--;
            }

            if (targetX <= x) return startPos;

            for (int i = 0; i < searchLen; ++i) {
                SIZE sz;
                GetTextExtentPoint32A(hdc, cachedText.c_str() + startPos, i + 1, &sz);
                if (x + sz.cx > targetX) {
                    return startPos + i;
                }
            }

            return startPos + searchLen;
        }
    
    private:
        std::vector<LineInfo> lines;
        int validLineWidth = -1;
        std::unordered_map<std::string, int> cache;
        std::string cachedText;
    
        int measure_word(HDC hdc, const std::string& word) {
            auto it = cache.find(word);
            if (it != cache.end()) return it->second;
            SIZE sz;
            GetTextExtentPoint32A(hdc, word.c_str(), (int)word.size(), &sz);
            cache[word] = sz.cx;
            return sz.cx;
        }
    
        int get_line_height(HDC hdc) {
            TEXTMETRIC tm;
            GetTextMetrics(hdc, &tm);
            return tm.tmHeight + tm.tmExternalLeading;
        }
    };