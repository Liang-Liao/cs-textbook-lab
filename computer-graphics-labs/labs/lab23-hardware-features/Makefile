LAB_NAME := lab23
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --out out/lab23_features.ppm
test: all
	$(BIN) --self-test
