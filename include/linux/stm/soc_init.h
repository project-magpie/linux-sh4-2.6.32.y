
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
		_pio_port, _ptx, _prx, _pcts, _prts)			\
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
			.pio_port       = _pio_port,			\
			.pio_pin        = { _pclk, _pdin, _pdout },	\
		}                                                       \
	}								\
}

#define USB_WRAPPER(_port, _wrapper_base, _protocol_base)	\
{								\
	.ahb2stbus_wrapper_glue_base = _wrapper_base,		\
	.ahb2stbus_protocol_base = _protocol_base,		\
	.power_up = usb_power_up,				\
	.initialised = 0,					\
	.port_number = _port,					\
}


#define USB_EHCI_DEVICE(_port, _base, _irq)				\
{									\
	.name = "stm-ehci",						\
	.id=_port,							\
	.dev = {							\
		.dma_mask = &st40_dma_mask,				\
		.coherent_dma_mask = 0xffffffful,			\
		.platform_data = &usb_wrapper[_port],			\
	},								\
	.num_resources = 2,						\
	.resource = (struct resource[]) {				\
		[0] = {							\
			.start = _base,					\
			.end   = _base + 0xff,				\
			.flags = IORESOURCE_MEM,			\
		},							\
		[1] = {							\
			.start = _irq,					\
			.end   = _irq,					\
			.flags = IORESOURCE_IRQ,			\
		},							\
	},								\
}									\


#define USB_OHCI_DEVICE(_port, _base, _irq)				\
{									\
	.name = "stm-ohci",						\
	.id=_port,							\
	.dev = {							\
		.dma_mask = &st40_dma_mask,				\
		.coherent_dma_mask = 0xffffffful,			\
		.platform_data = &usb_wrapper[_port],			\
	},								\
	.num_resources = 2,						\
	.resource = (struct resource[]) {				\
		[0] = {							\
			.start = _base,					\
			.end   = _base + 0xff,				\
			.flags = IORESOURCE_MEM,			\
		},							\
		[1] = {							\
			.start = _irq,					\
			.end   = _irq,					\
			.flags = IORESOURCE_IRQ,			\
		}							\
	}								\
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
