LAB_NAME := lab17
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --out out/lab17_light.ppm
test: all
	$(BIN) --self-test
