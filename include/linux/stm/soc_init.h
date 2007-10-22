
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


#define STASC_DEVICE(_base, _irq, _pio_port, _ptx, _prx, _pcts, _prts)	\
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
			.flags	= IORESOURCE_IRQ			\
		}							\
	},								\
	.dev = {							\
		.platform_data = &(struct stasc_uart_data) {		\
			.pio_port	= _pio_port,			\
			.pio_pin	= { _ptx, _prx, _pcts, _prts },	\
		}							\
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
	.name = "ST40-ehci",						\
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
	.name = "ST40-ohci",						\
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
