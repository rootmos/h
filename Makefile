CC = gcc
PP ?= ./pp
TCC ?= 0
TCC_BUNDLER ?= ./tcc-bundler

CFLAGS = -Wall
LDFLAGS = -llua
#EXTRA_CFLAGS ?= -O2 -Werror
EXTRA_CFLAGS ?=

EXE ?= run
SRC ?= main.c

.PHONY: build
build: $(EXE)

$(EXE): $(SRC) filter.bpfc r.h
ifeq ($(TCC), 1)
	$(TCC_BUNDLER) $(LDFLAGS) -o "$(@)" "$<"
else
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o "$@" "$<" $(LDFLAGS)
endif

%.bpfc: %.bpf
	$(PP) "$<" | bpf_asm -c > "$@"

.PHONY: clean
clean:
	rm -f "$(EXE)" *.bpfc
