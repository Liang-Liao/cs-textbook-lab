LAB_NAME := lab11
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --out out/lab11_color.ppm
test: all
	$(BIN) --self-test
