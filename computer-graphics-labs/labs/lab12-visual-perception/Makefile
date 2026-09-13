LAB_NAME := lab12
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --out out/lab12_perception.ppm
test: all
	$(BIN) --self-test
