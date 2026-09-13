LAB_NAME := lab21
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --out out/lab21_pipeline.txt
test: all
	$(BIN) --self-test
