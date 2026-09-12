# Lab 16: 迷你 Bootloader (16 位实模式)
# 不用 ld: UCRT64 的 ld 只支持 PE。as --32 出 COFF, objcopy 直出裸二进制。
AS      = as
OBJCOPY = objcopy

.PHONY: build run test clean

build: boot.bin

boot.bin: boot.s
	$(AS) --32 $< -o boot.o
	$(OBJCOPY) -O binary -j .text boot.o $@

run: boot.bin
	@echo "Starting QEMU (press Ctrl+A, X to exit)..."
	qemu-system-x86_64 -drive format=raw,file=boot.bin 2>/dev/null || \
	echo "QEMU not installed. Install with: pacman -S mingw-w64-ucrt-x86_64-qemu"

test: boot.bin
	@size=$$(stat -c%s boot.bin); \
	if [ "$$size" -eq 512 ] && od -An -tx1 -j 510 -N 2 boot.bin | grep -qi "55 aa"; then \
		echo "PASS boot (512 bytes, 55AA signature)"; \
	else \
		echo "FAIL boot (size=$$size or bad signature)"; exit 1; \
	fi

clean:
	rm -f boot.o boot.bin
