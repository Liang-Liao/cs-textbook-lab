# 推荐资源

## 经典教材

### 入门级
- **《汇编语言》王爽** — 中文经典，讲 16 位 DOS。建议快速翻阅理解基本概念后直接跳到 64 位
- **《Assembly Language Step-by-Step》Jeff Duntemann** — 英文入门经典

### 现代 x86-64
- **《Introduction to 64 Bit Assembly Programming》Ray Seyfarth** — 直接教 64 位
- **《Modern X86 Assembly Language Programming》Daniel Kusswurm** — 覆盖 32/64 位 + SIMD
- **《Computer Systems: A Programmer's Perspective》(CSAPP)** — 第三章机器级编程是必读（其示例是 Linux/AT&T，和本课程语法一致，ABI 差异按 WINDOWS.md 对照表换算）

### 参考手册
- **Intel® 64 and IA-32 Architectures Software Developer's Manual** — 官方指令集手册（查指令用），Vol.1 + Vol.2
- **AMD64 Architecture Programmer's Manual** — AMD 版本
- **Microsoft x64 ABI 官方文档**: https://learn.microsoft.com/en-us/cpp/build/x64-software-conventions — 本课程调用约定的权威出处（shadow space、寄存器归属、variadic 规则）
- **Microsoft PE Format 规范**: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format — lab14 的背景材料
- **Agner Fog 优化手册**: https://www.agner.org/optimize/ — 指令延迟/吞吐与微架构，lab13 进阶必读

## 在线资源

### 教程
- [x86-64 Assembly Tutorial (ReadTheDocs)](https://x86-64-assembly.readthedocs.io/zh-cn/latest/) — 中文教程（System V 约定，注意换算）
- [CS:APP3e Labs](http://csapp.cs.cmu.edu/3e/labs.html) — CMU 官方实验
- [Compiler Explorer (Godbolt)](https://godbolt.org/) — 在线看 C→汇编，选 x86-64 gcc (Windows) 目标可直接对照本课程

### GitHub 仓库
- [0xAX/asm](https://github.com/0xAX/asm) — "Learning assembly for Linux x86_64"，⭐2000+（System V，对照学习）
- [NekoSilverFox/Assembly](https://github.com/NekoSilverFox/Assembly) — 王爽《汇编语言》300 个例程
- [SJTU-IPADS/OS-Course-Lab](https://github.com/SJTU-IPADS/OS-Course-Lab) — 上交 OS 实验
- [osdev.org Wiki](https://wiki.osdev.org/) — lab16 之后想继续写 bootloader/内核的必去之地

### 工具
- [Godbolt Compiler Explorer](https://godbolt.org/) — 写 C 看汇编
- [Defuse Online x86 Assembler](https://defuse.ca/online-x86-assembler.htm) — 在线汇编
- [ASCII Table](https://www.asciitable.com/) — 字符编码参考

## 学习路径建议

```
Week 1-2:  Lab 01-05 (基础指令、控制流、宏)
Week 3-4:  Lab 06-09 (栈、函数、数组、位运算、浮点)
Week 5-6:  Lab 10-12 (C互操作、文件IO、SIMD)
Week 7-8:  Lab 13-16 (性能、PE格式、shellcode、bootloader)
```

**核心建议**：每个 Lab 都要用 GDB 单步调试，观察寄存器变化。光看代码不跑等于没学。
