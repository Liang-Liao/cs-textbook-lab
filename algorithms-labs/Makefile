# Top-level Makefile for CLRS algorithms lab.
#
#   make              - build common + all labs
#   make ch02         - build labs whose directory tag is ch02
#   make test         - run all lab tests
#   make test-ch02    - run tests for labs tagged ch02
#   make clean

ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
export ROOT

CC = gcc
export CC
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
export CFLAGS

# labs/ch02_getting_started/ -> ch02_getting_started
LABS := $(sort $(patsubst %/,%,$(dir $(wildcard $(ROOT)/labs/ch*/Makefile))))
LAB_NAMES := $(notdir $(LABS))
# ch02_getting_started -> ch02
LAB_TAGS := $(sort $(foreach l,$(LAB_NAMES),$(word 1,$(subst _, ,$(l)))))

.PHONY: all common clean test
.PHONY: $(LAB_NAMES) $(LAB_TAGS) $(addprefix test-,$(LAB_NAMES)) \
        $(addprefix test-,$(LAB_TAGS))

all: common $(addprefix build-,$(LAB_NAMES))

common:
	$(MAKE) -C $(ROOT)/common all

# Direct lab name: make ch02_getting_started
$(LAB_NAMES): %: build-%

.PHONY: $(addprefix build-,$(LAB_NAMES))
$(addprefix build-,$(LAB_NAMES)): build-%: common
	$(MAKE) -C $(ROOT)/labs/$* all

# Tag alias: make ch02 -> every lab starting with ch02
define TAG_RULE
.PHONY: $(1)
$(1): $$(foreach l,$$(LAB_NAMES),$$(if $$(filter $(1)%,$$(l)),build-$$(l)))
endef
$(foreach tag,$(LAB_TAGS),$(eval $(call TAG_RULE,$(tag))))

test: all
	@fail=0; \
	for lab in $(LAB_NAMES); do \
	  echo "==== test $$lab ===="; \
	  $(MAKE) -C $(ROOT)/labs/$$lab test || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then echo "Some tests failed."; exit 1; fi; \
	echo "All lab tests passed."

# make test-ch02_getting_started
$(addprefix test-,$(LAB_NAMES)): test-%: build-%
	$(MAKE) -C $(ROOT)/labs/$* test

# make test-ch02
define TEST_TAG_RULE
.PHONY: test-$(1)
test-$(1): $$(foreach l,$$(LAB_NAMES),$$(if $$(filter $(1)%,$$(l)),test-$$(l)))
endef
$(foreach tag,$(LAB_TAGS),$(eval $(call TEST_TAG_RULE,$(tag))))

clean:
	$(MAKE) -C $(ROOT)/common clean
	@for lab in $(LAB_NAMES); do \
	  $(MAKE) -C $(ROOT)/labs/$$lab clean; \
	done
	rm -rf $(ROOT)/build
