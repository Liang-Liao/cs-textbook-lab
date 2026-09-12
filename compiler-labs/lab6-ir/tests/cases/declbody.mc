// declbody.mc —— 声明不能直接作为 if/while 的执行体（与 C99 对齐）。
// 若放行，名字在编译期登记进外层作用域、运行期却要执行到才绑定：
// 条件为假即得"编译可见、运行未绑定"的分裂（lab5 触发 internal
// error，lab6 起 -eval 更是空指针解引用）。必须在语法层拒绝。
if (0) int x = 1;
while (0) float y = 2.0;
print 1;
