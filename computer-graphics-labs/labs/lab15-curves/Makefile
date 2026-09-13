LAB_NAME := lab15
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --out out/lab15_curves.ppm
test: all
	$(BIN) --self-test
