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

struct ELF32info {
	uint8_t	*base;	/* Base address of ELF image in memory  */
	Elf32_Ehdr	*header; /* Base address of ELF header in memory */
	uint32_t	size;	/* Total size of ELF data in bytes */
	uint32_t	mmapped;	/* Set to 1 if ELF file mmapped */
	Elf32_Shdr	*secbase;	/* Section headers base address */
	Elf32_Phdr	*progbase;	/* Program headers base address */
	char		*strtab;	/* String table for section headers */
	uint32_t	strtabsize;	/* Size of string table */
	uint32_t	strsecindex;	/* Section header index for strings */
	uint32_t	numsections;	/* Number of sections */
	uint32_t	numpheaders;	/* Number of program headers */
};

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

extern unsigned int ELF32_checkIdent(Elf32_Ehdr *);
extern struct ELF32info *ELF32_initFromMem(uint8_t *, uint32_t, int);
extern uint32_t ELF32_free(struct ELF32info *);
extern Elf32_Shdr *ELF32_getSectionByIndex(const struct ELF32info *, uint32_t);
extern Elf32_Shdr *ELF32_getSectionByNameCheck(const struct ELF32info *,
					const char *, uint32_t *, int, int);
extern void ELF32_printHeaderInfo(const struct ELF32info *);
extern void ELF32_printSectionInfo(const struct ELF32info *);
extern unsigned long ELF32_findBaseAddrCheck(Elf32_Ehdr *, Elf32_Shdr *,
					unsigned long *, int, int);
extern int ELF32_searchSectionType(const struct ELF32info *, const char *,
				int *);
extern unsigned long ELF32_checkPhMemSize(const struct ELF32info *);
extern unsigned long ELF32_checkPhMinVaddr(const struct ELF32info *);

static inline Elf32_Shdr *ELF32_getSectionByName(
					const struct ELF32info *elfinfo,
					const char *secname, uint32_t *index)
{
	return ELF32_getSectionByNameCheck(elfinfo, secname, index,
						SHF_NULL, SHT_NULL);
}
static inline unsigned long ELF32_findBaseAddr(Elf32_Ehdr *hdr,
				Elf32_Shdr *sechdrs, unsigned long *base)
{
	return ELF32_findBaseAddrCheck(hdr, sechdrs, base, SHF_NULL, SHT_NULL);
}

#endif /* _LIBELF_H_ */
