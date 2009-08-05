hostprogs-y	:= modpost mk_elfconfig
always		:= $(hostprogs-y) empty.o

modpost-objs	:= modpost.o file2alias.o sumversion.o

ifdef CONFIG_LKM_ELF_HASH
modpost-objs += ktablehash.o
HOSTCFLAGS_modpost.o    += -DCONFIG_LKM_ELF_HASH
ifdef CONFIG_UNUSED_SYMBOLS
HOSTCFLAGS_ktablehash.o += -DCONFIG_UNUSED_SYMBOLS
endif
$(obj)/ktablehash.o: $(obj)/elfconfig.h
endif

# dependencies on generated files need to be listed explicitly

$(obj)/modpost.o $(obj)/file2alias.o $(obj)/sumversion.o: $(obj)/elfconfig.h

quiet_cmd_elfconfig = MKELF   $@
      cmd_elfconfig = $(obj)/mk_elfconfig $(ARCH) < $< > $@

$(obj)/elfconfig.h: $(obj)/empty.o $(obj)/mk_elfconfig FORCE
	$(call if_changed,elfconfig)

targets += elfconfig.h
