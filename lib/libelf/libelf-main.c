/*
 * File     : libelf-main.c
 * Synopsis : Utility library for handling ELF files
 * Author   : Carl Shaw <carl.shaw@st.com>
 * Author   : Giuseppe Condorelli <giuseppe.condorelli@st.com>
 *
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 */

#include <linux/libelf.h>
#include <linux/string.h>
#include <linux/module.h>

#define ELF_CHECK_FLAG(x)	({x ? x : ~SHF_NULL; })
#define ELF_CHECK_TYPE(x)	({x ? x : ~SHT_NULL; })

/* Check elf file identity */
unsigned int ELF_checkIdent(Elf32_Ehdr *hdr)
{
	return memcmp(hdr->e_ident, ELFMAG, SELFMAG);
}
EXPORT_SYMBOL(ELF_checkIdent);

/* Initialise in-memory ELF file */
struct ELFinfo *ELF_initFromMem(uint8_t *elffile,
				uint32_t size, int mmapped)
{
	Elf32_Shdr	*sec;
	struct ELFinfo *elfinfo;

	elfinfo = (struct ELFinfo *)kmalloc(sizeof(struct ELFinfo), GFP_KERNEL);

	if (elfinfo == NULL)
		return NULL;

	elfinfo->mmapped = mmapped;
	elfinfo->size = size;

	elfinfo->base = (uint8_t *)elffile;
	elfinfo->header = (Elf32_Ehdr *)elffile;

	/* Check that image is really an ELF file */

	if (ELF_checkIdent((Elf32_Ehdr *)elffile))
		return NULL;

	/* Make sure it is 32 bit, little endian and current version */
	if (elffile[EI_CLASS] != ELFCLASS32 ||
		elffile[EI_DATA] != ELFDATA2LSB ||
		elffile[EI_VERSION] != EV_CURRENT)
		return NULL;

	/* Make sure we are a data file, relocatable or executable file */
	if ((elfinfo->header)->e_type > 3) {
		printk(KERN_ERR"Invalid ELF object type\n");
		return NULL;
	}

	/*
	 * Cache commonly-used addresses and values
	 */

	elfinfo->secbase = (Elf32_Shdr *)(elfinfo->base +
				(elfinfo->header)->e_shoff);
	elfinfo->progbase = (Elf32_Phdr *)(elfinfo->base +
				(elfinfo->header)->e_phoff);

	/* Get number of sections */

	if ((elfinfo->header)->e_shnum != 0) {
		/* Normal numbering */
		elfinfo->numsections = (elfinfo->header)->e_shnum;
	} else {
		/* Extended numbering */
		elfinfo->numsections = (elfinfo->secbase)->sh_size;
	}

	/* Get number of program headers */

	if ((elfinfo->header)->e_phnum != 0xffff) { /* PN_XNUM */
		/* Normal numbering */
		elfinfo->numpheaders = (elfinfo->header)->e_phnum;
	} else {
		/* Extended numbering */
		elfinfo->numpheaders = (elfinfo->secbase)->sh_info;
	}

	/* Get address of string table */

	if ((elfinfo->header)->e_shstrndx != SHN_HIRESERVE) {
		/* Normal numbering */
		elfinfo->strsecindex = (elfinfo->header)->e_shstrndx;
	} else {
		/* Extended numbering */
		elfinfo->strsecindex = (elfinfo->secbase)->sh_link;
	}

	sec = (Elf32_Shdr *)((uint8_t *)(elfinfo->secbase) +
		((elfinfo->header)->e_shentsize * elfinfo->strsecindex));
	elfinfo->strtab  = (char *)(elfinfo->base + sec->sh_offset);

	return elfinfo;
}
EXPORT_SYMBOL(ELF_initFromMem);

/* Free up memory-based resources */
uint32_t ELF_free(struct ELFinfo *elfinfo)
{
	kfree((void *)elfinfo);

	return 0;
}
EXPORT_SYMBOL(ELF_free);

Elf32_Shdr *ELF_getSectionByIndex(const struct ELFinfo *elfinfo, uint32_t index)
{
	return (Elf32_Shdr *)((uint8_t *)(elfinfo->secbase) +
				((elfinfo->header)->e_shentsize * index));
}
EXPORT_SYMBOL(ELF_getSectionByIndex);

/*
 * Search for section starting from its name. Also shflag and shtype are given
 * to restrict search for those sections matching them.
 * No flags check will be performed if SHF_NULL and SHT_NULL will be given.
 */
Elf32_Shdr *ELF_getSectionByNameCheck(const struct ELFinfo *elfinfo,
				 const char *secname,
				 uint32_t *index, int shflag, int shtype)
{
	uint32_t 	i;
	char 		*str;
	Elf32_Shdr	*sec;

	if (index)
		*index = 0;

	for (i = 0; i < elfinfo->numsections; i++) {
		if ((elfinfo->secbase[i].sh_flags & ELF_CHECK_FLAG(shflag)) &&
		(elfinfo->secbase[i].sh_type & ELF_CHECK_TYPE(shtype))) {
			sec = ELF_getSectionByIndex(elfinfo, i);
			str = elfinfo->strtab + sec->sh_name;
			if (strcmp(secname, str) == 0) {
				if (index)
					*index = i;
				return sec;
			}
		}
	}

	return NULL;
}
EXPORT_SYMBOL(ELF_getSectionByNameCheck);

unsigned long ELF_findBaseAddrCheck(Elf32_Ehdr *hdr, Elf32_Shdr *sechdrs,
				unsigned long *base, int shflag, int shtype)
{
	unsigned int i;
	int prev_index = 0;

	for (i = 1; i < hdr->e_shnum; i++)
		if ((sechdrs[i].sh_flags & ELF_CHECK_FLAG(shflag))
			&& (sechdrs[i].sh_type & ELF_CHECK_TYPE(shtype))) {
			if (sechdrs[i].sh_addr < *base)
				*base = sechdrs[i].sh_addr;
			prev_index = i;
		}
	return prev_index;
}
EXPORT_SYMBOL(ELF_findBaseAddrCheck);

/*
 * Check if the given section is present in the elf file. This function
 * also returns the index where section was found, if it was.
 */
int ELF_searchSectionType(const struct ELFinfo *elfinfo, const char *name,
				int *index)
{
	uint32_t	i, n;
	Elf32_Shdr	*sec;
	struct typess   elftypes[] = {ELF_TYPES};

	for (i = 0; i < elfinfo->numsections; i++) {
		sec = ELF_getSectionByIndex(elfinfo, i);
		for (n = 0; elftypes[n].name != NULL; n++)
			if ((strcmp(elftypes[n].name, name) == 0) &&
				(elftypes[n].val == sec->sh_type)) {
					if (index != NULL)
						*index = i;
					return 0;
			}
	}
	return 1;
}
EXPORT_SYMBOL(ELF_searchSectionType);

unsigned long ELF_checkPhMemSize(const struct ELFinfo *elfinfo)
{
	int i;
	unsigned long memsz = 0;

	for (i = 0; i < elfinfo->header->e_phnum; i++)
		if (elfinfo->progbase[i].p_type == PT_LOAD)
			memsz += elfinfo->progbase[i].p_memsz;
	return memsz;
}
EXPORT_SYMBOL(ELF_checkPhMemSize);

unsigned long ELF_checkPhMinVaddr(const struct ELFinfo *elfinfo)
{
	int i;
	unsigned long min_vaddr;

	min_vaddr = elfinfo->progbase[0].p_vaddr;
	for (i = 1; i < elfinfo->header->e_phnum; i++)
		if ((elfinfo->progbase[i].p_type == PT_LOAD) &&
			((elfinfo->progbase[i].p_vaddr < min_vaddr) |
						(min_vaddr == 0)))
			min_vaddr = elfinfo->progbase[i].p_vaddr;
	return min_vaddr;
}
EXPORT_SYMBOL(ELF_checkPhMinVaddr);
