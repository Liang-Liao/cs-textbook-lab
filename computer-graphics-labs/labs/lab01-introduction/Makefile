LAB_NAME := lab01
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --out out/lab01_patterns.ppm
test: all
	$(BIN) --self-test
