请使用C++和Win32 API编写一个完整的Piece Table类，并附带能够在空白窗口中运行的单元测试。

要求：
1. 定义Piece Table类，支持以下操作：
   - void insert(int pos, const std::string& text)  // 在位置pos插入文本
   - void erase(int pos, int len)                   // 从位置pos删除len个字符
   - std::string get_text() const                    // 返回整个文本
   - char char_at(int pos) const                      // 可选，用于随机访问

2. Piece Table内部使用两个缓冲区（original buffer和add buffer），以及一个片段列表（piece table）。每个片段应包含来源标识、起始偏移和长度。

3. 提供完整的实现，注意资源管理（使用RAII）和效率（如插入/删除时只修改片段元数据，不复制大量字符）。

4. 编写一个单元测试函数，测试以下场景：
   - 空文档插入一段文本，检查get_text()是否正确。
   - 在中间插入文本，验证结果。
   - 删除开头、中间、末尾的文本，验证。
   - 混合插入删除操作，验证最终文本。
   每个测试应输出“通过”或“失败”信息。

5. 创建一个简单的Win32窗口（包含WinMain和窗口过程），窗口背景为白色。当窗口收到WM_PAINT消息时，在窗口上绘制测试结果（例如列出每个测试的名称和结果，或者直接显示“所有测试通过”）。或者，在窗口创建后立即运行测试，并将测试结果通过MessageBox显示出来。

请提供完整的、可以直接编译运行的代码。代码应包含必要的头文件（<windows.h>, <string>, <vector>等），并遵循良好的编码风格。