# Lab 11: 文件 I/O

## 目标
- 学会用汇编调用 C 库的文件函数
- 掌握 fopen/fprintf/fgets/fclose 的调用方式
- 理解 FILE* 指针的传递

## 为什么用 C 库而不是 syscall？

| 方式 | 优点 | 缺点 |
|------|------|------|
| 底层 API (syscall / CreateFile) | 最贴近系统 | 参数复杂，两平台完全不同 |
| C 库 (fopen/fprintf/fgets) | UCRT 直接提供，接口统一 | 依赖 C 运行时 |

本课程选择 C 库，因为实际项目中汇编几乎都是和 C 混合使用。

## 关键代码片段（Windows x64）

```asm
# fopen("output.txt", "w"): 第 1/2 参数 = RCX/RDX
lea filename(%rip), %rcx
lea mode_w(%rip), %rdx
call fopen
test %rax, %rax
jz .error
mov %rax, %rbx             # 保存 FILE* (callee-saved, 跨调用存活)

# fprintf(fp, content): FILE* -> RCX, 字符串 -> RDX
mov %rbx, %rcx
lea content(%rip), %rdx
xor %eax, %eax             # variadic: AL = 0
call fprintf

# fgets(buf, 256, fp): 3 个参数走 RCX/RDX/R8
lea read_buf(%rip), %rcx
mov $256, %rdx
mov %rbx, %r8              # 第 3 参数 = FILE*
call fgets

# fclose(fp)
mov %rbx, %rcx
call fclose
```

错误处理模板：`test %rax, %rax; jz .error` —— fopen 失败返回 NULL。

## 练习

### 练习 1: 运行观察
```bash
mingw32-make run
# File written successfully.
# Read back: Hello from Assembly!
# ---
# (cat output.txt 的内容)
```

### 练习 2: 逐行读完整个文件
现在只 fgets 一次（第一行）。加一个循环读到 EOF 为止

### 练习 3: 追加模式
用 "a" 模式再打开一次文件，追加一行，验证原有内容保留

## 验证

```bash
mingw32-make build && mingw32-make test
```

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 2
```asm
.read_loop:
    lea read_buf(%rip), %rcx
    mov $256, %rdx
    mov %rbx, %r8
    call fgets
    test %rax, %rax        # fgets 返回 NULL = EOF
    jz .read_done
    lea fmt_read(%rip), %rcx
    lea read_buf(%rip), %rdx
    xor %eax, %eax
    call printf
    jmp .read_loop
.read_done:
```

### 练习 3
把 `lea mode_w(%rip), %rdx` 的字符串换成：
```asm
mode_a: .asciz "a"
```
重复"写一段 + 关闭"流程。"a" 模式不截断文件，新内容接在末尾。
对比 "w"：每次打开都会清空重来。
