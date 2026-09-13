LAB_NAME := lab13
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --width 320 --height 180 --spp 32 --out out/lab13_pathtrace.ppm
test: all
	$(BIN) --self-test
