CC = gcc
PP ?= ./pp

CFLAGS = -Wall
LDFLAGS = -llua
#EXTRA_CFLAGS ?= -O2 -Werror
EXTRA_CFLAGS ?=

EXE ?= run
SRC ?= main.c

.PHONY: build
build: $(EXE)

$(EXE): $(SRC) filter.bpfc r.h
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o "$@" "$<" $(LDFLAGS)

%.bpfc: %.bpf
	$(PP) "$<" | bpf_asm -c > "$@"

.PHONY: clean
clean:
	rm -f "$(EXE)" *.bpfc
