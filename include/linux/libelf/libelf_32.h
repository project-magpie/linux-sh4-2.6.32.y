#ifndef _LIBELF_32_H_
#define _LIBELF_32_H_

struct ELF32_info {
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

extern unsigned int ELF32_checkIdent(Elf32_Ehdr *);
extern struct ELF32_info *ELF32_initFromMem(uint8_t *, uint32_t, int);
extern uint32_t ELF32_free(struct ELF32_info *);
extern Elf32_Shdr *ELF32_getSectionByIndex(const struct ELF32_info *, uint32_t);
extern Elf32_Shdr *ELF32_getSectionByNameCheck(const struct ELF32_info *,
					const char *, uint32_t *, int, int);
extern void ELF32_printHeaderInfo(const struct ELF32_info *);
extern void ELF32_printSectionInfo(const struct ELF32_info *);
extern unsigned long ELF32_findBaseAddrCheck(Elf32_Ehdr *, Elf32_Shdr *,
					unsigned long *, int, int);
extern int ELF32_searchSectionType(const struct ELF32_info *, const char *,
				int *);
extern unsigned long ELF32_checkPhMemSize(const struct ELF32_info *);
extern unsigned long ELF32_checkPhMinVaddr(const struct ELF32_info *);

static inline Elf32_Shdr *ELF32_getSectionByName(const struct ELF32_info *info,
					const char *secname, uint32_t *index)
{
	return ELF32_getSectionByNameCheck(info, secname, index,
						SHF_NULL, SHT_NULL);
}
static inline unsigned long ELF32_findBaseAddr(Elf32_Ehdr *hdr,
				Elf32_Shdr *sechdrs, unsigned long *base)
{
	return ELF32_findBaseAddrCheck(hdr, sechdrs, base, SHF_NULL, SHT_NULL);
}

#endif /* _LIBELF_32_H_ */
