ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
include $(ROOT)/../build/common.makefile

PYTHON_PKG ?= python3-embed
CFLAGS += $(shell $(PKG_CONFIG) --cflags "$(PYTHON_PKG)")
LDFLAGS += $(shell $(PKG_CONFIG) --libs "$(PYTHON_PKG)")

EXE ?= hpython
SRC ?= main.c

.PHONY: build
build: $(EXE)

$(EXE): $(SRC) r.h filter.bpfc
ifeq ($(TCC), 1)
	$(TCC_BUNDLER) $(LDFLAGS) -o "$(@)" "$<"
else
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o "$@" "$<" $(LDFLAGS) $(EXTRA_LDFLAGS)
endif

%.bpfc: %.bpf
	$(BPFC) -o "$@" "$<"

.PHONY: clean
clean:
	rm -f "$(EXE)" *.bpfc
