LAB_NAME := lab09
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --out out/lab09_shading.ppm
test: all
	$(BIN) --self-test
