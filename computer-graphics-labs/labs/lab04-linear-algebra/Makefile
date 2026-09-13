LAB_NAME := lab04
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --out out/lab04_vectors.ppm
test: all
	$(BIN) --self-test
