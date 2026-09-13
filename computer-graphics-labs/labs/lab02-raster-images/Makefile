LAB_NAME := lab02
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --out out/lab02_sampling.ppm
test: all
	$(BIN) --self-test
