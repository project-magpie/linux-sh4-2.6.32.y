/*
 * Copyright STMicroelectronics Ltd (2009)
 *
 * Author: Carmelo Amoroso <carmelo.amoroso@st.com>
 *
 */

#include "modpost.h"

#undef DEBUG
#ifdef DEBUG
#define debugp(__msg...) fprintf(stdout, __msg)
#else
#define debugp(__msg...)	/* nothing */
#endif

#if KERNEL_ELFCLASS == ELFCLASS32
/* It needs to match sizeof within kernel
 * as defined in include/linux/module.h
 */
#define ksym_t      uint32_t
#define kstr_t      uint32_t
#define ksym_hash_t uint32_t
#else
#define ksym_t      uint64_t
#define kstr_t      uint64_t
#define ksym_hash_t uint64_t
#endif

/*
 * It matches with struct kernel_symbol defined
 * in include/linux/module.h when CONFIG_LKM_ELF_HASH
 * is configured
 */

struct kernel_symbol {
	ksym_t value;
	kstr_t name;
	ksym_hash_t hash_value;
};

#define KSTART(_elf, _idx) \
(struct kernel_symbol *) ((void *) _elf->hdr + _elf->sechdrs[_idx].sh_offset)

#define KSTOP(_elf, _idx) \
(struct kernel_symbol *) ((void *) _elf->hdr + _elf->sechdrs[_idx].sh_offset \
+ _elf->sechdrs[_idx].sh_size)

#define EMPTY_SLOT -1

/*
 * This maps the ELF hash table
 * The entries in the .hash table always have a size of 32 bits.
 */
struct elf_htable {
	uint32_t nbucket;
	uint32_t nchain;
	uint32_t *elf_buckets;
	uint32_t *chains;
};

struct export_sect {
	const char *name;
	Elf_Section sec;
};
