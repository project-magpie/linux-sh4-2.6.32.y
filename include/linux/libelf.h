#ifndef _LIBELF_H_
#define _LIBELF_H_

#include <linux/elf.h>

/*
 * We use this macro to refer to ELF types independent of the wordsize.
 * `ElfW(TYPE)' is used in place of `Elf32_TYPE' or `Elf64_TYPE'.
 */
#define ElfW(type)  _ElfW(Elf, __LIBELF_WORDSIZE, type)
#define _ElfW(e, w, t)    _ElfW_1(e, w, _##t)
#define _ElfW_1(e, w, t)  e##w##t


#define ELFW(func)  _ELFW(ELF, __LIBELF_WORDSIZE, func)
#define _ELFW(e, w, f)    _ELFW_1(e, w, _##f)
#define _ELFW_1(e, w, f)  e##w##f

struct typess {
	uint32_t	val;
	char 		*name;
};

#define ELF_TYPES	{0, "NULL"}, \
	{1, "PROGBITS"}, \
	{2, "SYMTAB"}, \
	{3, "STRTAB"}, \
	{4, "RELA"}, \
	{5, "HASH"}, \
	{6, "DYNAMIC"}, \
	{7, "NOTE"}, \
	{8, "NOBITS"}, \
	{9, "REL"}, \
	{10, "SHLIB"}, \
	{11, "DYNSYM"}, \
	{14, "INIT_ARRAY"}, \
	{15, "FINI_ARRAY"}, \
	{16, "PREINIT_ARRAY"}, \
	{17, "GROUP"}, \
	{18, "SYMTAB_SHNDX"}, \
	{0x6ffffff6, "GNU_HASH"}, \
	{0x6ffffff7, "GNU_PRELINK_LIBLIST"}, \
	{0x6ffffff8, "CHECKSUM"}, \
	{0x6ffffffd, "GNU version definitions"}, \
	{0x6ffffffe, "GNU version needs"}, \
	{0x6fffffff, "GNU version symbol table"}, \
	{0xffffffff, NULL}

#ifdef CONFIG_LIBELF_32
#include "libelf/libelf_32.h"
#endif

#ifdef CONFIG_LIBELF_64
#include "libelf/libelf_64.h"
#endif

#endif /* _LIBELF_H_ */
