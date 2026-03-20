#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "PieceTable.cpp"

class TextRenderer {
    public:
        void Draw(HDC hdc, const PieceTable& table, int x, int y, int maxWidth) {
            std::string text = table.get_text();
            std::vector<std::string> words = split_words(text);
    
            SIZE sz;
            int curX = x;
            int curY = y;
            int lineHeight = get_line_height(hdc);
    
            for (const auto& word : words) {
                int w = measure_word(hdc, word);
                // 如果遇到换行符，强制换行
                if (word == "\n") {
                    curX = x;
                    curY += lineHeight;
                    continue;
                }
                if (curX + w > x + maxWidth) {
                    if (w > maxWidth) {
                        draw_long_word(hdc, word, curX, curY, x, maxWidth, lineHeight);
                        continue; // curX 和 curY 已在函数内部更新
                    }  else{
                        // 换行
                        curX = x;
                        curY += lineHeight;
                    }
                }
                              
                TextOutA(hdc, curX, curY, word.c_str(), (int)word.size());
                curX += w;
            }
        }

        int GetLineHeight(HDC hdc) {
            return get_line_height(hdc);
        }

        POINT GetCursorCoordinate(HDC hdc, const PieceTable& table, int x, int y, int maxWidth, int cursorPos) {
            std::string text = table.get_text();
            std::vector<std::string> words = split_words(text);
            
            int curX = x;
            int curY = y;
            int lineHeight = get_line_height(hdc);
            int currentPos = 0;

            if (cursorPos <= 0) return {curX, curY};

            for (const auto& word : words) {
                int wordLen = (int)word.size();
                int w = measure_word(hdc, word);

                if (word == "\n") {
                    if (cursorPos <= currentPos) return {curX, curY};
                    curX = x;
                    curY += lineHeight;
                    currentPos += wordLen;
                    if (cursorPos == currentPos) return {curX, curY};
                    continue;
                }

                if (curX + w > x + maxWidth) {
                    if (w > maxWidth) {
                        int start = 0;
                        while (start < wordLen) {
                            int end = start + 1;
                            int lastFit = start;
                            int remainingWidth = x + maxWidth - curX;

                            while (end <= wordLen) {
                                SIZE sz;
                                GetTextExtentPoint32A(hdc, word.c_str() + start, end - start, &sz);
                                if (sz.cx > remainingWidth) break;
                                lastFit = end;
                                ++end;
                            }

                            if (lastFit == start) lastFit = start + 1;

                            if (cursorPos <= currentPos + lastFit) {
                                SIZE sz = {0, 0};
                                if (cursorPos - currentPos - start > 0) {
                                    GetTextExtentPoint32A(hdc, word.c_str() + start, cursorPos - currentPos - start, &sz);
                                }
                                return {curX + sz.cx, curY};
                            }

                            SIZE sz;
                            GetTextExtentPoint32A(hdc, word.c_str() + start, lastFit - start, &sz);
                            curX += sz.cx;
                            start = lastFit;

                            if (start < wordLen) {
                                curY += lineHeight;
                                curX = x;
                            }
                        }
                        currentPos += wordLen;
                        if (cursorPos == currentPos) return {curX, curY};
                        continue;
                    } else {
                        curX = x;
                        curY += lineHeight;
                    }
                }

                if (cursorPos < currentPos + wordLen) {
                    SIZE sz = {0, 0};
                    if (cursorPos - currentPos > 0) {
                        GetTextExtentPoint32A(hdc, word.c_str(), cursorPos - currentPos, &sz);
                    }
                    return {curX + sz.cx, curY};
                }

                curX += w;
                currentPos += wordLen;
                if (cursorPos == currentPos) return {curX, curY};
            }

            return {curX, curY};
        }

        int GetPosFromCoordinate(HDC hdc, const PieceTable& table, int x, int y, int maxWidth, int targetX, int targetY) {
            if (targetY < y) return 0;
            std::string text = table.get_text();
            std::vector<std::string> words = split_words(text);
            
            int curX = x;
            int curY = y;
            int lineHeight = get_line_height(hdc);
            int currentPos = 0;

            for (const auto& word : words) {
                int wordLen = (int)word.size();
                int w = measure_word(hdc, word);

                if (word == "\n") {
                    if (targetY >= curY && targetY < curY + lineHeight) return currentPos;
                    curX = x;
                    curY += lineHeight;
                    currentPos += wordLen;
                    continue;
                }

                if (curX + w > x + maxWidth) {
                    if (w > maxWidth) {
                        int start = 0;
                        while (start < wordLen) {
                            int end = start + 1;
                            int lastFit = start;
                            int remainingWidth = x + maxWidth - curX;

                            while (end <= wordLen) {
                                SIZE sz;
                                GetTextExtentPoint32A(hdc, word.c_str() + start, end - start, &sz);
                                if (sz.cx > remainingWidth) break;
                                lastFit = end;
                                ++end;
                            }
                            if (lastFit == start) lastFit = start + 1;
                            
                            if (targetY >= curY && targetY < curY + lineHeight) {
                                if (targetX <= curX) return currentPos + start;
                                for (int i = start; i < lastFit; ++i) {
                                    SIZE sz;
                                    GetTextExtentPoint32A(hdc, word.c_str() + start, i - start + 1, &sz);
                                    if (curX + sz.cx > targetX) return currentPos + i;
                                }
                                return currentPos + lastFit;
                            }

                            SIZE sz;
                            GetTextExtentPoint32A(hdc, word.c_str() + start, lastFit - start, &sz);
                            curX += sz.cx;
                            start = lastFit;

                            if (start < wordLen) {
                                curY += lineHeight;
                                curX = x;
                            }
                        }
                        currentPos += wordLen;
                        continue;
                    } else {
                        if (targetY >= curY && targetY < curY + lineHeight) return currentPos;
                        curX = x;
                        curY += lineHeight;
                    }
                }

                if (targetY >= curY && targetY < curY + lineHeight) {
                    if (targetX <= curX) return currentPos;
                    if (targetX < curX + w) {
                        for (int i = 0; i < wordLen; ++i) {
                            SIZE sz;
                            GetTextExtentPoint32A(hdc, word.c_str(), i + 1, &sz);
                            if (curX + sz.cx > targetX) return currentPos + i;
                        }
                    }
                }

                curX += w;
                currentPos += wordLen;
            }

            return currentPos;
        }
    
    private:
        std::unordered_map<std::string, int> cache;
    
        std::vector<std::string> split_words(const std::string& text) {
            std::vector<std::string> words;
            std::string cur;
            for (char c : text) {
                if (c == ' ' || c == '\t') {
                    if (!cur.empty()) {
                        words.push_back(cur);
                        cur.clear();
                    }
                    words.push_back(std::string(1, c));
                } else if (c == '\n') {
                    if (!cur.empty()) {
                        words.push_back(cur);
                        cur.clear();
                    }
                    words.push_back("\n");
                } 
                else if(c == '.' || c == ',' || c == '!' || c == '?' || c == ';' || c == ':') {
                    cur.push_back(c);
                    words.push_back(cur);
                    cur.clear();
                } else {
                    cur.push_back(c);
                }
            }
            if (!cur.empty()) words.push_back(cur);
            return words;
        }
    
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
    
        // void draw_long_word(HDC hdc, const std::string& word, int x, int y, int maxWidth, int lineHeight) {
        //     int start = 0;
        //     while (start < (int)word.size()) {
        //         int end = start + 1;
        //         int lastFit = start;
        //         while (end <= (int)word.size()) {
        //             SIZE sz;
        //             GetTextExtentPoint32A(hdc, word.c_str() + start, end - start, &sz);
        //             if (x + sz.cx > x + maxWidth) break;
        //             lastFit = end;
        //             ++end;
        //         }
        //         if (lastFit == start) lastFit = start + 1; // 至少绘制一个字符
        //         TextOutA(hdc, x, y, word.c_str() + start, lastFit - start);
        //         start = lastFit;
        //         y += lineHeight;
        //     }
        // }

        void draw_long_word(HDC hdc, const std::string& word,
            int& curX, int& curY,
            int xStart, int maxWidth, int lineHeight) {
            int start = 0;
            while (start < (int)word.size()) {
                int end = start + 1;
                int lastFit = start;
                int remainingWidth = xStart + maxWidth - curX;

                // 在当前行剩余宽度内尽量放下更多字符
                while (end <= (int)word.size()) {
                    SIZE sz;
                    GetTextExtentPoint32A(hdc, word.c_str() + start, end - start, &sz);
                    if (sz.cx > remainingWidth) break;
                    lastFit = end;
                    ++end;
                }

                if (lastFit == start) lastFit = start + 1; // 至少绘制一个字符

                SIZE sz;
                GetTextExtentPoint32A(hdc, word.c_str() + start, lastFit - start, &sz);
                TextOutA(hdc, curX, curY, word.c_str() + start, lastFit - start);

                curX += sz.cx;
                start = lastFit;

                // 如果还有剩余字符，换行
                if (start < (int)word.size()) {
                    curY += lineHeight;
                    curX = xStart;
                }
            }
        }

    };