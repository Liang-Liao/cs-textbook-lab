// MiniC v3 测试：if/else 与块语句（悬空 else 的主场）
// else 绑定最近的 if（C 语义）：
//   if (1) if (0) print 1; else print 2;
// 等价于 if (1) { if (0) print 1; else print 2; }
// 所以打印 2（若 else 绑到外层 if 则什么都不打印）
if (1) if (0) print 1; else print 2;

// 完整分支
if (1 < 2) print 10; else print 20;

// 嵌套块
{
    print 1 + 1;
    { print 2 + 2; }
    ;
}

// 条件里允许任意表达式优先级
if (3 > 2 == 1) print 100;

// 块作为 if 体
if (0) { print 111; print 222; } else { print 333; }
