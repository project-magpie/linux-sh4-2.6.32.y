
#define STPIO_DEVICE(_id, _base, _irq)					\
{									\
	.name		= "stpio",					\
	.id		= _id,						\
	.num_resources	= 2,						\
	.resource	= (struct resource[]) {				\
		{							\
			.start	= _base,				\
			.end	= _base + 0x100,			\
			.flags	= IORESOURCE_MEM			\
		}, {							\
			.start	= _irq,					\
			.flags	= IORESOURCE_IRQ			\
		}							\
	},								\
}


#define STASC_DEVICE(_base, _irq, _fdma_req_rx, _fdma_req_tx,		\
		_pio_port, _ptx, _prx, _pcts, _prts, _dptx, _dprx,	\
		_dpcts, _dprts)						\
{									\
	.name		= "stasc",					\
	.num_resources	= 2,						\
	.resource	= (struct resource[]) {				\
		{							\
			.start	= _base,				\
			.end	= _base + 0x100,			\
			.flags	= IORESOURCE_MEM			\
		}, {							\
			.start	= _irq,					\
			.end	= _irq,					\
			.flags	= IORESOURCE_IRQ			\
		}, {							\
			.start	= _fdma_req_rx,				\
			.end    = _fdma_req_rx,				\
			.flags	= IORESOURCE_DMA			\
		}, {							\
			.start	= _fdma_req_tx,				\
			.end    = _fdma_req_tx,				\
			.flags	= IORESOURCE_DMA			\
		}							\
	},								\
	.dev = {							\
		.platform_data = &(struct stasc_uart_data) {		\
			.pio_port	= _pio_port,			\
			.pio_pin	= { _ptx, _prx, _pcts, _prts },	\
			.pio_direction	= { _dptx, _dprx, _dpcts, _dprts},\
		}							\
	}								\
}

#define STSSC_DEVICE(_base, _irq, _pio_port, _pclk, _pdin, _pdout)	\
{									\
	.num_resources  = 2,						\
	.resource       = (struct resource[]) {				\
		{							\
			.start  = _base,				\
			.end    = _base + 0x10C,			\
			.flags  = IORESOURCE_MEM			\
		}, {							\
			.start  = _irq,					\
			.flags  = IORESOURCE_IRQ			\
		}							\
	},								\
	.dev = {							\
		.platform_data = &(struct ssc_pio_t ) {			\
			.pio = {					\
				{ _pio_port, _pclk },			\
				{ _pio_port, _pdin },			\
				{ _pio_port, _pdout }			\
			}						\
		}                                                       \
	}								\
}

#define USB_WRAPPER(_port, _wrapper_base, _protocol_base, _flags)	\
{									\
	.ahb2stbus_wrapper_glue_base = _wrapper_base,			\
	.ahb2stbus_protocol_base = _protocol_base,			\
	.flags = _flags,						\
}

#define USB_DEVICE(_port, _eh_base, _eh_irq, _oh_base, _oh_irq, _wrapper)\
{									\
	.name = "st-usb",						\
	.id = _port,							\
	.dev = {							\
		.dma_mask = &st40_dma_mask,				\
		.coherent_dma_mask = DMA_32BIT_MASK,			\
		.platform_data = _wrapper,				\
	},								\
	.num_resources = 4,						\
	.resource = (struct resource[]) {				\
		[0] = {							\
			.start = _eh_base,				\
			.end   = _eh_base + 0xff,			\
			.flags = IORESOURCE_MEM,			\
		},							\
		[1] = {							\
			.start = _eh_irq,				\
			.end   = _eh_irq,				\
			.flags = IORESOURCE_IRQ,			\
		},							\
		[2] = {							\
			.start = _oh_base,				\
			.end   = _oh_base + 0xff,			\
			.flags = IORESOURCE_MEM,			\
		},							\
		[3] = {							\
			.start = _oh_irq,				\
			.end   = _oh_irq,				\
			.flags = IORESOURCE_IRQ,			\
		},							\
	},								\
}

#define EMI_NAND_DEVICE(_id)							\
{										\
	.name		= "gen_nand",						\
	.id		= _id,							\
	.num_resources	= 1,							\
	.resource	= (struct resource[]) {					\
		{								\
			.flags		= IORESOURCE_MEM,			\
		}								\
	},									\
	.dev		= {							\
		.platform_data	= &(struct platform_nand_data) {		\
			.chip		=					\
			{							\
				.nr_chips		= 1,			\
				.options		= NAND_NO_AUTOINCR,	\
				.part_probe_types 	= nand_part_probes,	\
			},							\
			.ctrl		=					\
			{							\
				.cmd_ctrl		= nand_cmd_ctrl,	\
				.write_buf		= nand_write_buf,	\
				.read_buf		= nand_read_buf,	\
			}							\
		}								\
	}									\
}

#define SATA_DEVICE(_port, _base, _irq_hostc, _irq_dmac, _private)	\
{									\
	.name = "sata_stm",						\
	.id = _port,							\
	.dev = {							\
		.platform_data = _private,				\
	},								\
	.num_resources = 3,						\
	.resource = (struct resource[]) {				\
		[0] = {							\
			.start = _base,					\
			.end   = _base + 0xfff,				\
			.flags = IORESOURCE_MEM,			\
		},							\
		[1] = {							\
			.start = _irq_hostc,				\
			.end   = _irq_hostc,				\
			.flags = IORESOURCE_IRQ,			\
		},							\
		[2] = {							\
			.start = _irq_dmac,				\
			.end   = _irq_dmac,				\
			.flags = IORESOURCE_IRQ,			\
		}							\
	}								\
}


#define PCI_DEVICE(_emiss_addr, _ahb_pci_addr, _pci_window_addr, _pci_window_size, _dma_irq, _irq_serr) \
{											\
	.name = "pci_stm",								\
	.id = 0,									\
	.num_resources = 6,								\
	.resource = (struct resource[]) {						\
		[0] = {									\
			.start = _emiss_addr,						\
			.end = (_emiss_addr) + 0x17fc,					\
			.flags = IORESOURCE_MEM,					\
		},									\
		[1] = {									\
			.start = _ahb_pci_addr,						\
			.end = (_ahb_pci_addr) + 0xff,					\
			.flags = IORESOURCE_MEM,					\
		},									\
		[2] = {									\
			.start = _pci_window_addr,					\
			.end = (_pci_window_addr) + (_pci_window_size) - 1,		\
			.flags = IORESOURCE_MEM,					\
		},									\
		[3] = {									\
			.start = 0x1024,						\
			.end = 0xffff,							\
			.flags = IORESOURCE_IO,						\
		},									\
		[4] = {									\
				.name = "PCI DMA",					\
				.start = _dma_irq,					\
				.end = _dma_irq,					\
				.flags= IORESOURCE_IRQ,					\
		},									\
		[5] = {									\
				.name = "PCI SERR",					\
				.start = _irq_serr,					\
				.end = _irq_serr,					\
				.flags= IORESOURCE_IRQ,					\
		}									\
	}										\
}





