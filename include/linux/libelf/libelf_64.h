#ifndef _LIBELF_64_H_
#define _LIBELF_64_H_

struct ELF64_info {
	uint8_t	*base;	/* Base address of ELF image in memory  */
	Elf64_Ehdr	*header; /* Base address of ELF header in memory */
	uint32_t	size;	/* Total size of ELF data in bytes */
	uint32_t	mmapped;	/* Set to 1 if ELF file mmapped */
	Elf64_Shdr	*secbase;	/* Section headers base address */
	Elf64_Phdr	*progbase;	/* Program headers base address */
	char		*strtab;	/* String table for section headers */
	uint32_t	strtabsize;	/* Size of string table */
	uint32_t	strsecindex;	/* Section header index for strings */
	uint32_t	numsections;	/* Number of sections */
	uint32_t	numpheaders;	/* Number of program headers */
};

extern unsigned int ELF64_checkIdent(Elf64_Ehdr *);
extern struct ELF64_info *ELF64_initFromMem(uint8_t *, uint32_t, int);
extern uint32_t ELF64_free(struct ELF64_info *);
extern Elf64_Shdr *ELF64_getSectionByIndex(const struct ELF64_info *, uint32_t);
extern Elf64_Shdr *ELF64_getSectionByNameCheck(const struct ELF64_info *,
					const char *, uint32_t *, int, int);
extern void ELF64_printHeaderInfo(const struct ELF64_info *);
extern void ELF64_printSectionInfo(const struct ELF64_info *);
extern unsigned long ELF64_findBaseAddrCheck(Elf64_Ehdr *, Elf64_Shdr *,
					unsigned long *, int, int);
extern int ELF64_searchSectionType(const struct ELF64_info *, const char *,
				int *);
extern unsigned long ELF64_checkPhMemSize(const struct ELF64_info *);
extern unsigned long ELF64_checkPhMinVaddr(const struct ELF64_info *);

static inline Elf64_Shdr *ELF64_getSectionByName(const struct ELF64_info *info,
					const char *secname, uint32_t *index)
{
	return ELF64_getSectionByNameCheck(info, secname, index,
						SHF_NULL, SHT_NULL);
}
static inline unsigned long ELF64_findBaseAddr(Elf64_Ehdr *hdr,
				Elf64_Shdr *sechdrs, unsigned long *base)
{
	return ELF64_findBaseAddrCheck(hdr, sechdrs, base, SHF_NULL, SHT_NULL);
}

#endif /* _LIBELF_32_H_ */
