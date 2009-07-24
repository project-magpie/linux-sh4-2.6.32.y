#include "elflib.h"

void fatal(const char *fmt, ...)
{
	va_list arglist;

	fprintf(stderr, "FATAL: ");

	va_start(arglist, fmt);
	vfprintf(stderr, fmt, arglist);
	va_end(arglist);

	exit(1);
}

void *grab_file(const char *filename, unsigned long *size)
{
	struct stat st;
	void *map;
	int fd;

	fd = open(filename, O_RDWR);
	if (fd < 0 || fstat(fd, &st) != 0)
		return NULL;

	*size = st.st_size;
	map = mmap(NULL, *size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);

	if (map == MAP_FAILED)
		return NULL;
	return map;
}

void release_file(void *file, unsigned long size)
{
	munmap(file, size);
}

static inline
    void set_ksymtable(struct elf_info *info, enum ksymtab_type type,
		       Elf_Ehdr *hdr, Elf_Shdr *sechdrs, unsigned int secidx,
		       const char *secname)
{

	info->ksym_tables[type].start = (struct kernel_symbol *)
	    ((void *)hdr + sechdrs[secidx].sh_offset);
	info->ksym_tables[type].stop = (struct kernel_symbol *)
	    ((void *)hdr + sechdrs[secidx].sh_offset + sechdrs[secidx].sh_size);
	info->ksym_tables[type].name = strdup(secname);
	info->ksym_tables[type].entries =
	    sechdrs[secidx].sh_size / sizeof(struct kernel_symbol);
}

int parse_elf(struct elf_info *info, const char *filename)
{
	unsigned int i;
	Elf_Ehdr *hdr;
	Elf_Shdr *sechdrs;
	Elf_Sym *sym;
	char *lkm_suffix;

	hdr = grab_file(filename, &info->size);
	if (!hdr) {
		perror(filename);
		exit(1);
	}
	info->hdr = hdr;
	if (info->size < sizeof(*hdr)) {
		/* file too small, assume this is an empty .o file */
		return 0;
	}
	/* Is this a valid ELF file? */
	if ((hdr->e_ident[EI_MAG0] != ELFMAG0) ||
	    (hdr->e_ident[EI_MAG1] != ELFMAG1) ||
	    (hdr->e_ident[EI_MAG2] != ELFMAG2) ||
	    (hdr->e_ident[EI_MAG3] != ELFMAG3)) {
		/* Not an ELF file - silently ignore it */
		return 0;
	}

	/* Check if it is the vmlinux or lkm */
	lkm_suffix = strstr(filename, ".ko");
	if (lkm_suffix && (strlen(lkm_suffix) == 3))
		/* Likely this is a lkm */
		info->is_lkm = 1;
	else {
		info->is_lkm = 0;
		/* Don't care */
		info->base_addr = 0;
	}

	/* Fix endianness in ELF header */
	hdr->e_shoff = TO_NATIVE(hdr->e_shoff);
	hdr->e_shstrndx = TO_NATIVE(hdr->e_shstrndx);
	hdr->e_shnum = TO_NATIVE(hdr->e_shnum);
	hdr->e_machine = TO_NATIVE(hdr->e_machine);
	hdr->e_type = TO_NATIVE(hdr->e_type);
	sechdrs = (void *)hdr + hdr->e_shoff;
	info->sechdrs = sechdrs;

	/* Fix endianness in section headers */
	for (i = 0; i < hdr->e_shnum; i++) {
		sechdrs[i].sh_type = TO_NATIVE(sechdrs[i].sh_type);
		sechdrs[i].sh_offset = TO_NATIVE(sechdrs[i].sh_offset);
		sechdrs[i].sh_size = TO_NATIVE(sechdrs[i].sh_size);
		sechdrs[i].sh_link = TO_NATIVE(sechdrs[i].sh_link);
		sechdrs[i].sh_name = TO_NATIVE(sechdrs[i].sh_name);
		sechdrs[i].sh_info = TO_NATIVE(sechdrs[i].sh_info);
		sechdrs[i].sh_addr = TO_NATIVE(sechdrs[i].sh_addr);
	}
	/* Find symbol tables and text section. */
	for (i = 1; i < hdr->e_shnum; i++) {
		const char *secstrings
		    = (void *)hdr + sechdrs[hdr->e_shstrndx].sh_offset;
		const char *secname;

		if (sechdrs[i].sh_offset > info->size) {
			fatal("%s is truncated. sechdrs[i].sh_offset=%u > \
			sizeof(*hrd) = %ul\n", filename,
			(unsigned int)sechdrs[i].sh_offset, sizeof(*hdr));
			return 0;
		}
		secname = secstrings + sechdrs[i].sh_name;

		if (strcmp(secname, ".text") == 0)
			info->base_addr =
			    sechdrs[i].sh_addr - sechdrs[i].sh_offset;

		if (strcmp(secname, "__ksymtab") == 0)
			set_ksymtable(info, KSYMTAB, hdr, sechdrs, i, secname);
		else if (strcmp(secname, "__ksymtab_unused") == 0)
			set_ksymtable(info, KSYMTAB_UNUSED, hdr, sechdrs, i,
				      secname);
		else if (strcmp(secname, "__ksymtab_gpl") == 0)
			set_ksymtable(info, KSYMTAB_GPL, hdr, sechdrs, i,
				      secname);
		else if (strcmp(secname, "__ksymtab_unused_gpl") == 0)
			set_ksymtable(info, KSYMTAB_UNUSED_GPL, hdr, sechdrs, i,
				      secname);
		else if (strcmp(secname, "__ksymtab_gpl_future") == 0)
			set_ksymtable(info, KSYMTAB_GPL_FUTURE, hdr, sechdrs, i,
				      secname);
		else if (strcmp(secname, "__ksymtab_strings") == 0)
			info->kstrings = (void *)hdr + sechdrs[i].sh_offset;
		else if (strcmp(secname, ".undef.hash") == 0) {
			info->symtab_hash.start = (void *)
			    hdr + sechdrs[i].sh_offset;
			info->symtab_hash.stop = (void *)
			    hdr + sechdrs[i].sh_offset + sechdrs[i].sh_size;
		}

		if (sechdrs[i].sh_type != SHT_SYMTAB)
			continue;

		info->symtab.start = (void *)hdr + sechdrs[i].sh_offset;
		info->symtab.stop = (void *)hdr +
		    sechdrs[i].sh_offset + sechdrs[i].sh_size;
		info->strtab = (void *)hdr +
		    sechdrs[sechdrs[i].sh_link].sh_offset;
	}
	if (!info->symtab.start)
		fatal("%s has no symtab?\n", filename);

	/* Fix endianness in symbols */
	for (sym = info->symtab.start; sym < info->symtab.stop; sym++) {
		sym->st_shndx = TO_NATIVE(sym->st_shndx);
		sym->st_name = TO_NATIVE(sym->st_name);
		sym->st_value = TO_NATIVE(sym->st_value);
		sym->st_size = TO_NATIVE(sym->st_size);
	}
	return 1;
}

void parse_elf_finish(struct elf_info *info)
{
	release_file(info->hdr, info->size);
}
