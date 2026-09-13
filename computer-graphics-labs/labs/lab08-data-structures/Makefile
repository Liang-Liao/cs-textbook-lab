LAB_NAME := lab08
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --spheres 200 --out out/lab08_accel.ppm
test: all
	$(BIN) --self-test
