/*
 * Copyright STMicroelectronics Ltd (2009)
 *
 * Author: Carmelo Amoroso <carmelo.amoroso@st.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/stat.h>
#include "elflib.h"

#define GET_KSTRING(__ksym, __offset) (unsigned char *)(__ksym->name + __offset)

#undef DEBUG
#ifdef DEBUG
#define debug(__msg...) fprintf(stdout, __msg)
#else
#define debug(__msg...)		/* nothing */
#endif

#define dump_undef(__undef, __hash) \
	debug("\tUnresolved: %s\thash = 0x%x\n", __undef, __hash)
#define dump_ksym(__ksym, __kstr) \
	debug("\tExported: %s\thash = 0x%x\n", __kstr, __ksym->hash_value)

static ksym_hash_t gnu_hash(const unsigned char *name)
{
	ksym_hash_t h = 5381;
	unsigned char c;
	for (c = *name; c != '\0'; c = *++name)
		h = h * 33 + c;
	return h & 0xffffffff;
}

static inline
    void compute_exported_hash(const struct elf_info *elf, enum ksymtab_type tp)
{

	struct kernel_symbol *sym;
	long s_offset;
	struct kernel_symbol *start = elf->ksym_tables[tp].start;
	struct kernel_symbol *stop = elf->ksym_tables[tp].stop;

	if (elf->is_lkm) {
		/*
		 * ksym->name is an offset with respect to the start of the
		 *  __ksymtab_strings
		 */
		s_offset = (long)elf->kstrings;
	} else {
		/*
		 * In this case, ksym->name is the absolute value of the string
		 * into the __ksymtab_strings
		 */
		s_offset = (long)elf->hdr - (long)elf->base_addr;
	}

	for (sym = start; sym < stop; sym++) {
		sym->hash_value = gnu_hash(GET_KSTRING(sym, s_offset));
		dump_ksym(sym, GET_KSTRING(sym, s_offset));
	}
}

static inline void compute_unresolved_hash(struct elf_info *elf)
{

	Elf_Sym *sym;
	unsigned int undef = 0;
	ksym_hash_t *hash_values = elf->symtab_hash.start;

	if (!elf->is_lkm) {
		elf->unresolved = undef;
		return;
	}

	for (sym = elf->symtab.start; sym < elf->symtab.stop; sym++) {
		if (sym->st_shndx == SHN_UNDEF) {
			/* undefined symbol */
			if (ELF_ST_BIND(sym->st_info) != STB_GLOBAL &&
			    ELF_ST_BIND(sym->st_info) != STB_WEAK)
				continue;
			else {
				/* GLOBAL or WEAK undefined symbols */
				*hash_values = gnu_hash((unsigned char *)
							(elf->strtab +
							 sym->st_name));
				dump_undef(elf->strtab + sym->st_name,
					   *hash_values);
				/*
				 * The hash_values array stored into the
				 * .undef.hash section is ordered as the
				 * undefined symbols of the .symtab
				 */
				hash_values++;
				undef++;
			}
		}
	}
	elf->unresolved = undef;
}

int main(int argc, char **argv)
{

	enum ksymtab_type k;
	struct elf_info info = { };

	if (!parse_elf(&info, argv[1]))
		exit(1);

	/* Skip __ksymtab_strings */
	for (k = KSYMTAB; k < KSYMTAB_ALL; k++) {

		if (info.ksym_tables[k].name) {

			/* Compute hash value for exported symbols */
			compute_exported_hash(&info, k);

			debug("ktable: %s [exported: %u]\n",
			      info.ksym_tables[k].name,
			      info.ksym_tables[k].entries);
		}
	}

	compute_unresolved_hash(&info);
	debug("\nModule: %s [unresolved: %u]\n", argv[1], info.unresolved);

	parse_elf_finish(&info);
	return 0;
}
