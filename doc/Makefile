THIS := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
ROOT ?= $(realpath $(THIS)/..)
TOOLS ?= $(ROOT)/tools

EXEs = hlua hpython hnode hsh

.PHONY: all
all: \
	$(ROOT)/README.md \
	$(ROOT)/tools/README.md \
	$(foreach exe, $(EXEs), $(ROOT)/$(exe)/README.md)

$(ROOT)/README.md: README.in.md deps-table.md
	./include "$<" "$@"

$(ROOT)/tools/README.md: tools.in.md hlua.tests.txt
	./include "$<" "$@"

hlua.tests.txt:
	env TEST_ROOT="$(ROOT)/hlua" HUMAN=1 "$(TOOLS)/test-runner" > "$@"

deps-table.md: \
	$(ROOT)/build/ubuntu/deps \
	$(ROOT)/build/archlinux/PKGBUILD.template
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
	rm -f deps-table.md usage.*.md *.tests.txt
