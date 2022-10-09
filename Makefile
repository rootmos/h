CC = gcc
CFLAGS = -Wall
LDFLAGS = -llua
#EXTRA_CFLAGS ?= -O2 -Werror
EXTRA_CFLAGS ?=

EXE ?= run
SRC ?= main.c

.PHONY: build
build: $(EXE)

$(EXE): $(SRC) r.h
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o "$@" "$<" $(LDFLAGS)

.PHONY: clean
clean:
	rm -f "$(EXE)"
