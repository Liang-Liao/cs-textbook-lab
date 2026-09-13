LAB_NAME := lab05
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --out-prefix out/lab05_xform
test: all
	$(BIN) --self-test
