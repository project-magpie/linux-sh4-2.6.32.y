# Shared between Makefile and Makefile.modpost

hostprogs-y		+= ksymhash mk_elfconfig
always			:= $(hostprogs-y) empty.o

ksymhash-objs	:= ksymhash.o elflib.o

# dependencies on generated files need to be listed explicitly

$(obj)/ksymhash.o : $(obj)/elflib.o
$(obj)/elflib.o   : $(obj)/elfconfig.h

quiet_cmd_elfconfig = MKELF   $@
      cmd_elfconfig = $(obj)/mk_elfconfig $(ARCH) < $< > $@

$(obj)/elfconfig.h: $(obj)/empty.o $(obj)/mk_elfconfig FORCE
	$(call if_changed,elfconfig)

targets += elfconfig.h

# Post-process vmlinux image to populate ksymtabs with GNU hash values

quiet_cmd_ksymhash = SYMHASH
      cmd_ksymhash = scripts/ksymhash/ksymhash

ifdef CONFIG_LKM_ELF_HASH
define rule_ksymhash
	$(Q)$(if $($(quiet)cmd_ksymhash),								\
		echo '  $($(quiet)cmd_ksymhash) $@' &&)						\
	$(cmd_ksymhash) $@
endef
else
define rule_ksymhash
endef
endif
