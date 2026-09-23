# modeling-labs root Makefile (Windows + MSYS2 UCRT64 / mingw32-make)
# labs/<id> = src(业务算法) + test(bin/test) + vendor(前序 lab copy)
# 无 common/：算法正本在各 lab 的 src/。

MSYS2_ROOT ?= C:/msys64
SHELL      := $(MSYS2_ROOT)/usr/bin/sh.exe
.SHELLFLAGS := -c
export PATH := $(MSYS2_ROOT)\ucrt64\bin;$(MSYS2_ROOT)\usr\bin;$(PATH)

CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror=implicit-function-declaration
LDFLAGS +=
LDLIBS  += -lm
EXEEXT  ?=

LABS    := A1-numeric-la A2-prob-stat A3-convex-opt B1-line-search-gd B2-newton-quasi-tr B3-least-squares B4-lp-ip B5-constrained B6-sgd-adam B7-prox-admm C1-rng-mc C2-mcmc C3-sa C4-ts-vns-ils C5-ga-de C6-es-cmaes C7-pso-aco C8-constraint-mo C9-bayes-opt D1-interp-spline D2-ode-sim D3-graph-combo M1-curve-fitting M2-global-bench M3-e2e-param-est

define LAB_SHORT_ALIASES
lab-A1: lab-A1-numeric-la
check-A1: check-A1-numeric-la
lab-A2: lab-A2-prob-stat
check-A2: check-A2-prob-stat
lab-A3: lab-A3-convex-opt
check-A3: check-A3-convex-opt
lab-B1: lab-B1-line-search-gd
check-B1: check-B1-line-search-gd
lab-B2: lab-B2-newton-quasi-tr
check-B2: check-B2-newton-quasi-tr
lab-B3: lab-B3-least-squares
check-B3: check-B3-least-squares
lab-B4: lab-B4-lp-ip
check-B4: check-B4-lp-ip
lab-B5: lab-B5-constrained
check-B5: check-B5-constrained
lab-B6: lab-B6-sgd-adam
check-B6: check-B6-sgd-adam
lab-B7: lab-B7-prox-admm
check-B7: check-B7-prox-admm
lab-C1: lab-C1-rng-mc
check-C1: check-C1-rng-mc
lab-C2: lab-C2-mcmc
check-C2: check-C2-mcmc
lab-C3: lab-C3-sa
check-C3: check-C3-sa
lab-C4: lab-C4-ts-vns-ils
check-C4: check-C4-ts-vns-ils
lab-C5: lab-C5-ga-de
check-C5: check-C5-ga-de
lab-C6: lab-C6-es-cmaes
check-C6: check-C6-es-cmaes
lab-C7: lab-C7-pso-aco
check-C7: check-C7-pso-aco
lab-C8: lab-C8-constraint-mo
check-C8: check-C8-constraint-mo
lab-C9: lab-C9-bayes-opt
check-C9: check-C9-bayes-opt
lab-D1: lab-D1-interp-spline
check-D1: check-D1-interp-spline
lab-D2: lab-D2-ode-sim
check-D2: check-D2-ode-sim
lab-D3: lab-D3-graph-combo
check-D3: check-D3-graph-combo
lab-M1: lab-M1-curve-fitting
check-M1: check-M1-curve-fitting
lab-M2: lab-M2-global-bench
check-M2: check-M2-global-bench
lab-M3: lab-M3-e2e-param-est
check-M3: check-M3-e2e-param-est
endef
$(eval $(LAB_SHORT_ALIASES))

.PHONY: all check clean labs vendor-sync test-list \
        lab-A1 check-A1 lab-A2 check-A2 lab-A3 check-A3 \
        lab-B1 check-B1 lab-B2 check-B2 lab-B3 check-B3 \
        lab-B4 check-B4 lab-B5 check-B5 lab-B6 check-B6 \
        lab-B7 check-B7 lab-C1 check-C1 lab-C2 check-C2 lab-C3 check-C3 lab-C4 check-C4 lab-C5 check-C5 \
        lab-C6 check-C6 lab-C7 check-C7 lab-C8 check-C8 lab-C9 check-C9 \
        lab-D1 check-D1 lab-D2 check-D2 lab-D3 check-D3 lab-M1 check-M1 lab-M2 check-M2 lab-M3 check-M3 \
        $(addprefix lab-,$(LABS)) $(addprefix check-,$(LABS))

all: labs

labs:
	@for lab in $(LABS); do \
		$(MAKE) -C labs/$$lab || exit 1; \
	done

$(addprefix lab-,$(LABS)): lab-%:
	$(MAKE) -C labs/$*

$(addprefix check-,$(LABS)): check-%: lab-%
	$(MAKE) -C labs/$* check

check: all
	@fail=0; \
	for lab in $(LABS); do \
		echo "==== check $$lab ===="; \
		$(MAKE) -C labs/$$lab check || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then echo "CHECK FAILED"; exit 1; fi; \
	echo "ALL CHECKS PASSED"

vendor-sync:
	python scripts/sync_vendor.py --all

clean:
	@for lab in $(LABS); do \
		$(MAKE) -C labs/$$lab clean || true; \
	done
