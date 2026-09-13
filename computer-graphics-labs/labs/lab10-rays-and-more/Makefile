LAB_NAME := lab10
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --spp 16 --out out/lab10_rays.ppm
test: all
	$(BIN) --self-test
