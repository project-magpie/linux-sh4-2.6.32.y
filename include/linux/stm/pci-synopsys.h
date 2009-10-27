#ifndef LINUX_STM_PCI_SYNOPSYS_H
#define LINUX_STM_PCI_SYNOPSYS_H

u8 pci_synopsys_inb(unsigned long port);
u16 pci_synopsys_inw(unsigned long port);
u32 pci_synopsys_inl(unsigned long port);

u8 pci_synopsys_inb_p(unsigned long port);
u16 pci_synopsys_inw_p(unsigned long port);
u32 pci_synopsys_inl_p(unsigned long port);

void pci_synopsys_insb(unsigned long port, void *dst, unsigned long count);
void pci_synopsys_insw(unsigned long port, void *dst, unsigned long count);
void pci_synopsys_insl(unsigned long port, void *dst, unsigned long count);

void pci_synopsys_outb(u8 val, unsigned long port);
void pci_synopsys_outw(u16 val, unsigned long port);
void pci_synopsys_outl(u32 val, unsigned long port);

void pci_synopsys_outb_p(u8 val, unsigned long port);
void pci_synopsys_outw_p(u16 val, unsigned long port);
void pci_synopsys_outl_p(u32 val, unsigned long port);

void pci_synopsys_outsb(unsigned long port, const void *src,
			unsigned long count);
void pci_synopsys_outsw(unsigned long port, const void *src,
			unsigned long count);
void pci_synopsys_outsl(unsigned long port, const void *src,
			unsigned long count);

/*
 * We have to hook all the in/out functions as they cannot be memory
 * mapped with the synopsys PCI IP
 *
 * Also, for PCI we use the generic iomap implementation, and so do
 * not need the ioport_map function, instead using the generic cookie
 * based implementation.
 */
#define STM_PCI_IO_MACHINE_VEC			\
	.mv_inb = pci_synopsys_inb,		\
	.mv_inw = pci_synopsys_inw,		\
	.mv_inl = pci_synopsys_inl,		\
	.mv_outb = pci_synopsys_outb,		\
	.mv_outw = pci_synopsys_outw,		\
	.mv_outl = pci_synopsys_outl,		\
	.mv_inb_p = pci_synopsys_inb_p,		\
	.mv_inw_p = pci_synopsys_inw,		\
	.mv_inl_p = pci_synopsys_inl,		\
	.mv_outb_p = pci_synopsys_outb_p,	\
	.mv_outw_p = pci_synopsys_outw,		\
	.mv_outl_p = pci_synopsys_outl,		\
	.mv_insb = pci_synopsys_insb,		\
	.mv_insw = pci_synopsys_insw,		\
	.mv_insl = pci_synopsys_insl,		\
	.mv_outsb = pci_synopsys_outsb,		\
	.mv_outsw = pci_synopsys_outsw,		\
	.mv_outsl = pci_synopsys_outsl,

#endif /* LINUX_STM_PCI_SYNOPSYS_H */
