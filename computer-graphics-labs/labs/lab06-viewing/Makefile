LAB_NAME := lab06
include ../../make/lab.mk

.PHONY: run test
run: all
	$(BIN) --out-prefix out/lab06_view
test: all
	$(BIN) --self-test
