LAB_NAME := lab03
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --width 400 --height 225 --out out/lab03_raytrace.ppm
test: all
	$(BIN) --self-test
