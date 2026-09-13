# Computer Graphics Labs — top-level Makefile
# Works with both MSYS2 bash and Windows cmd/PowerShell (no POSIX shell loops).
# Requires on PATH: gcc, mingw32-make (UCRT64)

MAKEFLAGS += --no-print-directory

LABS := \
  lab00-toolchain-smoke \
  lab01-introduction \
  lab02-raster-images \
  lab03-ray-tracing \
  lab04-linear-algebra \
  lab05-transformation-matrices \
  lab06-viewing \
  lab07-texture-mapping \
  lab08-data-structures \
  lab09-shading \
  lab10-rays-and-more \
  lab11-color \
  lab12-visual-perception \
  lab13-more-ray-tracing \
  lab14-sampling \
  lab15-curves \
  lab16-surfaces \
  lab17-light \
  lab18-materials \
  lab19-wave-optics \
  lab20-visibility \
  lab21-rt-hardware \
  lab22-rasterization \
  lab23-hardware-features

.PHONY: all list clean run test release $(LABS) FORCE

FORCE:

list:
	$(info Available labs:)
	$(foreach d,$(LABS),$(info   [ready] $(d)))

all: $(addprefix build-,$(LABS))

build-%: FORCE
	@echo ==== $* ====
	$(MAKE) -C labs/$* all

test: $(addprefix test-,$(LABS))

test-%: FORCE
	@echo ==== test $* ====
	$(MAKE) -C labs/$* test

release: $(addprefix release-,$(LABS))

release-%: FORCE
	@echo ==== release $* ====
	$(MAKE) -C labs/$* release

clean: $(addprefix clean-,$(LABS))

clean-%: FORCE
	$(MAKE) -C labs/$* clean

$(LABS): FORCE
	$(MAKE) -C labs/$@

ifndef LAB
run:
	$(error usage: make run LAB=lab03-ray-tracing)
else
run:
	$(MAKE) -C labs/$(LAB) run
endif
