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

        void InvalidateCache(int pos) {
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

        void EnsureLayout(HDC hdc, const std::string& text, int maxWidth) {
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
            if (currentStart >= (int)text.length()) {
                if (currentStart > 0 && currentStart == (int)text.length() && text.back() == '\n') {
                    // 末尾显式换行的状态
                } else if (currentStart > (int)text.length()) {
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
            while (curPos < (int)text.length()) {
                int wordStart = curPos;
                int wordEnd = wordStart;
                char c = text[wordEnd];
                
                if (c == ' ' || c == '\t' || c == '\n') {
                    wordEnd++;
                } else if (c == '.' || c == ',' || c == '!' || c == '?' || c == ';' || c == ':') {
                    wordEnd++;
                } else {
                    while (wordEnd < (int)text.length()) {
                        char nextC = text[wordEnd];
                        if (nextC == ' ' || nextC == '\t' || nextC == '\n' || 
                            nextC == '.' || nextC == ',' || nextC == '!' || 
                            nextC == '?' || nextC == ';' || nextC == ':') {
                            break;
                        }
                        wordEnd++;
                    }
                }

                std::string word = text.substr(wordStart, wordEnd - wordStart);
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

        void Draw(HDC hdc, const PieceTable& table, int x, int y, int maxWidth) {
            std::string text = table.get_text();
            EnsureLayout(hdc, text, maxWidth);
            
            RECT clipBox;
            GetClipBox(hdc, &clipBox);

            // 空间检索优化：利用二分查找快速定位当前视口首行对应的 Piece 节点
            auto itStart = std::lower_bound(lines.begin(), lines.end(), clipBox.top - y, 
                [](const LineInfo& line, int targetY) { return line.yPos < targetY; });
            
            if (itStart != lines.begin()) --itStart; // 向前包容一行以防顶层截断

            auto itEnd = std::lower_bound(lines.begin(), lines.end(), clipBox.bottom - y, 
                [](const LineInfo& line, int targetY) { return line.yPos <= targetY; });
            if (itEnd != lines.end()) ++itEnd;

            // 视口剪裁：仅对当前视口高度范围内的 Piece 段执行 DrawText (以行为单位合并输出更疾速)
            for (auto it = itStart; it != itEnd && it != lines.end(); ++it) {
                int startPos = it->startPos;
                int endPos = (it + 1 != lines.end()) ? (it + 1)->startPos : (int)text.length();
                
                int drawLen = endPos - startPos;
                if (drawLen > 0 && text[startPos + drawLen - 1] == '\n') {
                    drawLen--;
                }
                
                if (drawLen > 0) {
                    TextOutA(hdc, x, y + it->yPos, text.c_str() + startPos, drawLen);
                }
            }
        }

        int GetLineHeight(HDC hdc) {
            return get_line_height(hdc);
        }

        int GetTotalHeight(HDC hdc, const PieceTable& table, int maxWidth) {
            std::string text = table.get_text();
            EnsureLayout(hdc, text, maxWidth);
            int lineHeight = get_line_height(hdc);
            return lines.back().yPos + lineHeight;
        }

        POINT GetCursorCoordinate(HDC hdc, const PieceTable& table, int x, int y, int maxWidth, int cursorPos) {
            std::string text = table.get_text();
            EnsureLayout(hdc, text, maxWidth);

            auto it = std::upper_bound(lines.begin(), lines.end(), cursorPos, 
                [](int p, const LineInfo& line) { return p < line.startPos; });
            
            if (it != lines.begin()) --it;

            int startPos = it->startPos;
            int drawLen = cursorPos - startPos;
            
            SIZE sz = {0, 0};
            if (drawLen > 0) {
                GetTextExtentPoint32A(hdc, text.c_str() + startPos, drawLen, &sz);
            }

            return {x + sz.cx, y + it->yPos};
        }

        int GetPosFromCoordinate(HDC hdc, const PieceTable& table, int x, int y, int maxWidth, int targetX, int targetY) {
            std::string text = table.get_text();
            EnsureLayout(hdc, text, maxWidth);

            int relY = targetY - y;
            if (relY < 0) return 0;

            auto it = std::upper_bound(lines.begin(), lines.end(), relY, 
                [](int target, const LineInfo& line) { return target < line.yPos; });
            
            if (it != lines.begin()) --it;

            int startPos = it->startPos;
            int endPos = (it + 1 != lines.end()) ? (it + 1)->startPos : (int)text.length();
            
            int searchLen = endPos - startPos;
            if (searchLen > 0 && text[startPos + searchLen - 1] == '\n') {
                searchLen--;
            }

            if (targetX <= x) return startPos;

            for (int i = 0; i < searchLen; ++i) {
                SIZE sz;
                GetTextExtentPoint32A(hdc, text.c_str() + startPos, i + 1, &sz);
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