PROJECTS = hlua hpython
TARGETS = build clean test

define mk_rule
.PHONY: $(strip $(1)).$(strip $(2))
$(strip $(1)).$(strip $(2)):
	$$(MAKE) -C $(strip $(1)) $(strip $(2))
endef

define mk_project
$(foreach t, $(TARGETS), $$(eval $$(call mk_rule, $(strip $(1)), $t)))
endef

define mk_target
.PHONY: $(strip $(1))
$(strip $(1)): $(foreach p, $(PROJECTS), $p.$(strip $(1)))
endef

$(foreach t, $(TARGETS), $(eval $(call mk_target, $t)))
$(foreach p, $(PROJECTS), $(eval $(call mk_project, $p)))