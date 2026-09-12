# Lab 14: PE 可执行文件剖析
TARGET = mini
SRCS   = mini.s
include ../common/rules.mk

# 拆开 PE/COFF 二进制看结构 (Windows 没有 readelf, objdump 全包)
analyze: $(EXE)
	@echo "===== 1. 文件格式 (objdump -f) ====="
	@objdump -f $(EXE)
	@echo ""
	@echo "===== 2. 节表 (objdump -h) ====="
	@objdump -h $(EXE)
	@echo ""
	@echo "===== 3. 符号表 (nm) ====="
	@nm $(EXE)
	@echo ""
	@echo "===== 4. 反汇编 main (objdump -d) ====="
	@objdump -d $(EXE) | sed -n '/<main>:/,/ret/p'
	@echo ""
	@echo "===== 5. PE 头 + 导入表 (objdump -x) ====="
	@objdump -x $(EXE) | sed -n '1,25p'
	@objdump -x $(EXE) | sed -n '/The Import Tables/,$$p' | head -40
