CC = gcc
PP ?= ./pp

CFLAGS = -Wall
LDFLAGS =
#EXTRA_CFLAGS ?= -O2 -Werror
LOG_LEVEL ?= INFO
EXTRA_CFLAGS ?= -DLOG_LEVEL=LOG_$(LOG_LEVEL)
EXTRA_LDFLAGS ?=

EXE ?= landlock
SRC ?= main.c

.PHONY: build
build: $(EXE)

$(EXE): $(SRC) r.h
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o "$@" "$<" $(LDFLAGS) $(EXTRA_LDFLAGS)

.PHONY: clean
clean:
	rm -f "$(EXE)"
