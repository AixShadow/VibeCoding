## 第一次询问
请使用C++和Win32 API编写一个文本渲染类 `TextRenderer`，它能接收之前生成的 `PieceTable` 对象，并在窗口上绘制带自动换行的文本。具体要求如下：

1. **核心功能**：
   - 提供一个 `Draw` 方法：`void Draw(HDC hdc, const PieceTable& table, int x, int y, int maxWidth)`，将 `table` 中的文本从 `(x, y)` 开始绘制，当文本宽度超过 `maxWidth` 时自动换行。
   - 使用 `GetTextExtentPoint32` 精确测量字符串宽度，确保文本位置准确。

2. **高效换行算法**：
   - 避免逐字符调用 `GetTextExtentPoint32`（性能差）。
   - 采用基于“单词”或“块”的测量方法：
     - 将文本按空格（或常见分隔符）分割成单词列表（保留空格作为单独单词或附加到前一个单词后，取决于你希望保留空格）。
     - 遍历单词，累积当前行的宽度（包括单词间的空格），若加上下一个单词会超过 `maxWidth`，则换行。
     - 若单个单词宽度超过 `maxWidth`（例如长英文单词），则需要在单词内部进行分割（可退化为逐字符测量该单词，但这种情况较少，或采用二分法查找最大可显示前缀）。
   - 可考虑缓存已测量单词的宽度（例如使用 `std::unordered_map<std::string, int>`），避免重复测量相同单词。

3. **与Win32窗口集成**：
   - 提供一个简单的Win32窗口（与之前的测试窗口类似），在 `WM_PAINT` 中调用 `TextRenderer::Draw`，绘制一段从 `PieceTable` 获取的测试文本。
   - 测试文本应包含多种情况：短单词、长单词、空格、换行符（可选，若需要处理 `\n` 可强制换行），以验证自动换行效果。
   - 设置窗口背景为白色，字体使用系统默认字体或创建一种等宽字体以便观察，不要使用中文。

4. **代码完整性与说明**：
   - 提供完整的C++代码，包含必要的头文件（`<windows.h>`, `<string>`, `<vector>`, `<unordered_map>` 等），并确保可以与之前生成的 `PieceTable` 类一起编译运行。
   - 如果 `PieceTable` 接口与假设不符，请说明需要如何调整，或提供一个简化的 `PieceTable` 模拟类以便演示。

请确保代码遵循RAII原则，无内存泄漏，并添加必要注释说明算法关键点。

## 问题
没能处理好长单词，对于长单词渲染函数内部，应该传入curX和curY的引用，否则会导致长单词与后续单词渲染覆盖

## 第二次对话
请修改 TextRenderer 类中的 draw_long_word 函数以及调用它的 Draw 函数，解决长单词换行后与后续单词重叠、以及未能利用当前行剩余空间的问题。

当前 Draw 函数在处理长单词时存在两个缺陷：
draw_long_word 的 y 参数是按值传递，导致外部 curY 无法更新，后续单词仍绘制在原来的行，造成重叠。
draw_long_word 总是从 curX 开始绘制长单词，但忽略了当前行可能已有部分内容（curX 可能大于 x），导致未能充分利用当前行剩余空间；而且在绘制完长单词后，curX 被硬重置为 x，可能丢失了长单词最后一行之后的可继续绘制的机会。

要求进行以下修改：
修改 draw_long_word 的函数签名，使其接受三个位置参数：
int& curX：当前横坐标引用，用于更新绘制位置。
int& curY：当前纵坐标引用，用于更新绘制位置。
int xStart：行首固定横坐标（即每行的起始位置，通常就是传入的 x）。
原型应为：
void draw_long_word(HDC hdc, const std::string& word, int& curX, int& curY, int xStart, int maxWidth, int lineHeight)

draw_long_word 内部实现应改为：
计算当前行剩余宽度：remainingWidth = xStart + maxWidth - curX。
从单词开头开始，尝试在剩余宽度内绘制尽可能多的字符。测量方法保持原样（使用 GetTextExtentPoint32A 逐字符增长，找到在当前剩余宽度内能容纳的最大前缀）。
绘制该前缀于 (curX, curY)，然后更新 curX += 已绘宽度。
如果单词还有剩余部分，则换行：curY += lineHeight，curX = xStart，然后继续处理剩余部分，此时剩余宽度恢复为 maxWidth。
重复直到整个单词绘制完毕。

在 Draw 函数中，调用 draw_long_word 的方式应调整为：
在循环中，当发现单词宽度 w > maxWidth（即单词本身宽度超过一整行）时，调用 draw_long_word(hdc, word, curX, curY, x, maxWidth, lineHeight);

请一并检查并修正可能出现的逻辑遗漏。