LAB_NAME := lab14
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --out out/lab14_sampling.ppm
test: all
	$(BIN) --self-test
