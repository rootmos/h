THIS := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
ROOT ?= $(realpath $(THIS)/..)

EXEs = hlua hpython hnode

.PHONY: all
all: $(ROOT)/README.md $(foreach exe, $(EXEs), $(ROOT)/$(exe)/README.md)

$(ROOT)/README.md: README.in.md deps-table.md
	./include "$<" "$@"

deps-table.md: $(foreach arch, ubuntu archlinux, $(ROOT)/build/$(arch)/deps)
	./deps-table "$@"

define exe_rule
usage.$(strip $(1)).md: $$(ROOT)/$(strip $(1))/$(strip $(1))
	./usage "$$<" "$$@"

$$(ROOT)/$(strip $(1))/README.md: $(strip $(1)).in.md usage.$(strip $(1)).md
	./include "$$<" "$$@"
endef

$(foreach exe, $(EXEs), $(eval $(call exe_rule, $(exe))))

.PHONY: clean
clean:
	rm -f deps-table.md usage.*.md
