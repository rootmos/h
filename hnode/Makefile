ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
include $(ROOT)/../build/common.makefile

CFLAGS += -I/usr/include/node
LDFLAGS += -lnode

CFLAGS += $(shell $(PKG_CONFIG) --cflags libuv)
LDFLAGS += $(shell $(PKG_CONFIG) --libs libuv)

ifeq ($(CXX), g++)
MAJOR = $(shell g++ -dumpfullversion | cut -d. -f1)
ifeq ($(shell expr $(MAJOR) \>= 14), 1)
CFLAGS += -Wno-error=template-id-cdtor
endif
endif

EXE ?= hnode
SRC ?= main.cpp

.PHONY: build
build: $(EXE)

$(EXE).cpp: $(SRC) filter.bpfc main.jsc \
	capabilities.c seccomp.c version.c r.h
	$(SINGLE_FILE) -o "$@" "$<"

%.jsc: %.js
	$(C_ARRAY) -zo"$@" -i"$<"

.PHONY: clean
clean:
	rm -f $(EXE) $(EXE).cpp *.bpfc version.c *.jsc
