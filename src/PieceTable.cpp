#include<string>
#include<vector>
#include<algorithm>
using namespace std;

// ---------------- Piece Table Implementation ----------------
class PieceTable {
    public:
        enum class BufferKind { Original, Add };
    
        struct Piece {
            BufferKind buffer;
            int offset;
            int length;
        };
    
        PieceTable(const std::string& original = "")
            : original_buffer(original) {
            if (!original.empty()) {
                pieces.push_back({ BufferKind::Original, 0, (int)original.size() });
            }
        }
    
        void insert(int pos, const std::string& text) {
            if (text.empty()) return;
            int add_offset = (int)add_buffer.size();
            add_buffer += text;
    
            int cur = 0;
            for (size_t i = 0; i < pieces.size(); ++i) {
                if (cur + pieces[i].length >= pos) {
                    int local_pos = pos - cur;
                    Piece p = pieces[i];
                    if (local_pos == 0) {
                        pieces.insert(pieces.begin() + i,
                            { BufferKind::Add, add_offset, (int)text.size() });
                    } else if (local_pos == p.length) {
                        pieces.insert(pieces.begin() + i + 1,
                            { BufferKind::Add, add_offset, (int)text.size() });
                    } else {
                        Piece left{ p.buffer, p.offset, local_pos };
                        Piece mid{ BufferKind::Add, add_offset, (int)text.size() };
                        Piece right{ p.buffer, p.offset + local_pos, p.length - local_pos };
                        pieces.erase(pieces.begin() + i);
                        pieces.insert(pieces.begin() + i, { left });
                        pieces.insert(pieces.begin() + i + 1, { mid });
                        pieces.insert(pieces.begin() + i + 2, { right });
                    }
                    return;
                }
                cur += pieces[i].length;
            }
            pieces.push_back({ BufferKind::Add, add_offset, (int)text.size() });
        }
    
        void erase(int pos, int len) {
            if (len <= 0) return;
            int cur = 0;
            for (size_t i = 0; i < pieces.size() && len > 0; ) {
                Piece& p = pieces[i];
                if (cur + p.length > pos) {
                    int local_pos = pos - cur;
                    int erase_len = min(len, p.length - local_pos);
                    if (erase_len == p.length) {
                        pieces.erase(pieces.begin() + i);
                        cur += p.length;
                    } else if (local_pos == 0) {
                        p.offset += erase_len;
                        p.length -= erase_len;
                        cur += erase_len;
                        ++i;
                    } else if (local_pos + erase_len == p.length) {
                        p.length = local_pos;
                        ++i;
                    } else {
                        Piece left{ p.buffer, p.offset, local_pos };
                        Piece right{ p.buffer, p.offset + local_pos + erase_len,
                                     p.length - local_pos - erase_len };
                        pieces.erase(pieces.begin() + i);
                        pieces.insert(pieces.begin() + i, left);
                        pieces.insert(pieces.begin() + i + 1, right);
                        ++i;
                    }
                    len -= erase_len;
                    pos += erase_len;
                } else {
                    cur += p.length;
                    ++i;
                }
            }
        }
    
        std::string get_text() const {
            std::string result;
            for (const auto& p : pieces) {
                const std::string& buf = (p.buffer == BufferKind::Original ? original_buffer : add_buffer);
                result.append(buf.substr(p.offset, p.length));
            }
            return result;
        }
    
        std::string get_text_range(int pos, int len) const {
            if (len <= 0) return "";
            std::string result;
            int cur = 0;
            int remaining = len;
            for (const auto& p : pieces) {
                if (remaining <= 0) break;
                int piece_end = cur + p.length;
                if (piece_end > pos) {
                    // 计算当前 piece 中需要提取的起始位置和长度
                    int offset_in_piece = std::max(0, pos - cur);
                    int take = std::min(p.length - offset_in_piece, remaining);
                    
                    const std::string& buf = (p.buffer == BufferKind::Original ? original_buffer : add_buffer);
                    result.append(buf.substr(p.offset + offset_in_piece, take));
                    
                    remaining -= take;
                    pos += take;
                }
                cur = piece_end;
            }
            return result;
        }

        char char_at(int pos) const {
            int cur = 0;
            for (const auto& p : pieces) {
                if (cur + p.length > pos) {
                    const std::string& buf = (p.buffer == BufferKind::Original ? original_buffer : add_buffer);
                    return buf[p.offset + (pos - cur)];
                }
                cur += p.length;
            }
            return '\0';
        }
    
    private:
        std::string original_buffer;
        std::string add_buffer;
        std::vector<Piece> pieces;
    };