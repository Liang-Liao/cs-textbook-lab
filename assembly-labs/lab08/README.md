# Lab 08: 位运算

## 目标
- 掌握 AND/OR/XOR/NOT/SHL/SHR 指令
- 理解位运算的实际应用
- 学会位操作技巧

## 位运算指令 (AT&T)

```asm
and %rbx, %rax     # rax &= rbx
or  %rbx, %rax     # rax |= rbx
xor %rbx, %rax     # rax ^= rbx
not %rax            # rax = ~rax
shl $3, %rax        # rax <<= 3 (乘以 8)
shr $2, %rax        # rax >>= 2 (无符号除以 4)
sar $2, %rax        # 算术右移 (有符号除以 4)
```

位域操作三兄弟：
```asm
bts $5, %rax        # Bit Test and Set:    置位第 5 位
btr $5, %rax        # Bit Test and Reset:  清零第 5 位
btc $3, %rax        # Bit Test and Complement: 翻转第 3 位
```

## 常见技巧

```asm
xor %rax, %rax      # 清零 (比 mov $0, %rax 更短)
test %rax, %rax     # 检查是否为零 (不修改 rax)
test $1, %rax       # 检查最低位 (奇偶判断)
```

## 关键代码片段

```asm
# popcount (Brian Kernighan 算法)
# n & (n-1) 每次清除最低位的 1
mov $0b10110100, %rax
xor %r12, %r12           # count = 0
.count_loop:
    test %rax, %rax
    jz .count_done
    mov %rax, %rbx
    dec %rbx
    and %rbx, %rax       # 清除最低位的 1
    inc %r12
    jmp .count_loop

# XOR 交换 (不用临时变量)
mov $10, %rax            # a = 10
mov $20, %rbx            # b = 20
xor %rbx, %rax           # a ^= b
xor %rax, %rbx           # b ^= a  (b = 原来的 a)
xor %rbx, %rax           # a ^= b  (a = 原来的 b)
```

## 练习

### 练习 1: 运行观察
```bash
mingw32-make run
# 42 is even
# popcount(0b10110100) = 4
# After bts 5, btr 5, btc 3: rax = 8
# 7 * 10 (via shifts) = 70
# XOR swap: a=20, b=10
```

### 练习 2: 换成 popcnt 指令
x86 有硬件 popcount 指令 `popcnt %rax, %rcx`（需要 -msse4.2 或
`-mpopcnt`），改写并验证结果一致

### 练习 3: 快速除/乘 2 的幂
用移位实现 100/8 和 100*8，负数用 sar 试试 -100>>2 的结果

## 验证

```bash
mingw32-make build && mingw32-make test
```

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 2
```asm
mov $0b10110100, %rax
popcnt %rax, %r12        # 一条指令 = 4 (Makefile 加 -mpopcnt)
```
Brian Kernighan 循环的价值在于理解"清除最低位 1"这个技巧本身；
实际代码直接用 popcnt（Kaby Lake 之后 1 周期吞吐）。

### 练习 3
```asm
mov $100, %rax
shr $3, %rax             # 100 / 8 = 12 (无符号, 余数丢弃)
mov $100, %rax
shl $3, %rax             # 100 * 8 = 800

mov $-100, %rax
sar $2, %rax             # -100 >> 2 = -25 (符号位保留)
                         # 若用 shr 会得到天文数字: 符号位被移走
```
记忆：shl=逻辑左移；shr=逻辑右移（补 0）；sar=算术右移（补符号位）。
除以 2 的幂（无符号）用 shr，有符号用 sar——但有符号除法向零取整，
负奇数时 sar 结果和 idiv 差 1（-3>>1 = -2，而 -3/2 = -1），这是经典考点。
