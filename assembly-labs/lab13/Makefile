# Lab 13: 性能优化
# 计时数字每次不同, 用 grep 按格式校验 (覆盖标准 test 规则)
TARGET      = performance
SRCS        = performance.s
LAB_NO_TEST = 1
include ../common/rules.mk

test: $(EXE)
	@./$(EXE) > .test.out 2>&1
	@if grep -Eq '^Normal sum = 1000000, time = [0-9]+ us$$' .test.out && \
	    grep -Eq '^Unrolled sum = 1000000, time = [0-9]+ us$$' .test.out; then \
		echo "PASS $(TARGET)"; \
	else \
		echo "FAIL $(TARGET)"; cat .test.out; rm -f .test.out; exit 1; \
	fi
	@rm -f .test.out
