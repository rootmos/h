CC = gcc
PP ?= ./pp

# CFLAGS = -Wall -Werror -O2
CFLAGS = -Wall
LDFLAGS =
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
