# Lab 07: 数组与字符串

## 目标
- 掌握内存寻址模式
- 学会处理数组和字符串
- 理解 REP 前缀族

## 寻址模式速查 (AT&T)

```asm
mov $42, %rax                 # 立即数
mov %rbx, %rax                # 寄存器
mov my_var(%rip), %rax        # 直接寻址 (符号地址)
mov (%rbx), %rax              # 寄存器间接寻址
mov 16(%rbx), %rax            # 基址 + 偏移
mov 16(%rbx,%rcx,8), %rax     # 基址 + 变址*比例 + 偏移
#   rbx=基址, rcx=索引, 8=比例(元素大小), 16=偏移
```

## REP 前缀族

| 指令 | 功能 |
|------|------|
| rep movsb | 重复复制字节 (memcpy) |
| rep stosb | 重复存储字节 (memset) |
| repnz scasb | 重复扫描字节直到相等 (strchr/strlen) |

## 关键代码片段

```asm
# 字符串长度: RCX = str -> RAX = len
my_strlen:
    xor %eax, %eax
.strlen_loop:
    cmpb $0, (%rcx,%rax)     # 检查 '\0'
    je .strlen_done
    inc %rax
    jmp .strlen_loop
.strlen_done:
    ret

# rep movsb 复制
lea dst(%rip), %rdi       # 目标
lea src(%rip), %rsi       # 源
mov $src_len, %rcx        # 字节数
cld                       # 正向 (DF=0)
rep movsb                 # 批量复制!
```

## ⚠️ 两个 Windows 特有的坑

**1. RDI/RSI 是 callee-saved。** main 里用它们做 rep movsb，用完要恢复：
```asm
main:
    push %rbp
    push %rbx
    push %rdi              # 用了就要还
    push %rsi
    sub $40, %rsp          # 3 个额外 push (奇数) → $40
    ...
    add $40, %rsp
    pop %rsi
    pop %rdi
    pop %rbx
    pop %rbp
```

**2. 寄存器别名陷阱。** strcmp 里第二字节不能用 %cl——它是 %rcx 的低字节，
而 %rcx 正是推进中的指针：
```asm
# ❌ mov (%rdx), %cl 会破坏 RCX 的低 8 位!
# ✅ 换用 %r8b (R8 不是指针)
mov (%rcx), %al
mov (%rdx), %r8b
cmp %r8b, %al
```

## 练习

### 练习 1: 运行观察
```bash
mingw32-make run
# strlen("Hello, Assembly World!") = 22
# rep movsb copy: "Hello, Assembly World!"
# strcmp result (equal) = 0
```

### 练习 2: rep stosb
用 `rep stosb` 把 dst 清零（memset 的汇编版）

### 练习 3: 反向复制
把 `cld` 换成 `std`（DF=1），复制结果会怎样？怎么修正让结果正确？

### 练习 4: my_strcat
实现 `strcat(dst, src)`：找到 dst 的 '\0'，然后把 src 连过去

## 验证

```bash
mingw32-make build && mingw32-make test
```

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 2
```asm
lea dst(%rip), %rdi
xor %eax, %eax            # 填 0
mov $100, %rcx            # dst 大小
rep stosb
```

### 练习 3
`std` 后 rep movsb 从高地址往低地址复制，dst 内容变成**倒序**。
正确用法：源和目标区间重叠时（如 memmove 场景），DF=1 + 从末尾开始
可以安全复制；不重叠时用 cld 正向即可。记得用完 `cld` 恢复 DF=0！
（ABI 要求进入函数/交还控制流时 DF=0）

### 练习 4
```asm
/* my_strcat: RCX = dst*, RDX = src* -> RAX = dst* */
my_strcat:
    mov %rcx, %r8          # 记住 dst 起点
.find_end:
    cmpb $0, (%rcx)
    je .append
    inc %rcx
    jmp .find_end
.append:
    mov (%rdx), %al
    mov %al, (%rcx)
    test %al, %al
    jz .done
    inc %rcx
    inc %rdx
    jmp .append
.done:
    mov %r8, %rax          # 返回 dst
    ret
```
注意 `%al` 是 `%rax` 的低字节——这里 RAX 当返回值还没用，安全；
如果 RAX 同时当指针就会踩 lab README 上面那个别名陷阱。
