LAB_NAME := lab00
include ../../make/lab.mk

.PHONY: run test

run: all
	$(BIN) --width 256 --height 256 --out out/lab00_gradient.ppm
	@echo "Wrote out/lab00_gradient.ppm"

test: all
	$(BIN) --self-test
