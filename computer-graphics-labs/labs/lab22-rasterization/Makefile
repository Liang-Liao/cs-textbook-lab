LAB_NAME := lab22
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --out out/lab22_raster.ppm
test: all
	$(BIN) --self-test
