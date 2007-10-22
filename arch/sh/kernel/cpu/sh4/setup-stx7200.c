/*
 * STx710x Setup
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/io.h>
#include <linux/stm/soc.h>
#include <linux/stm/pio.h>
#include <asm/sci.h>
#include <asm/irq-ilc.h>
#include <linux/stm/fdma-plat.h>
#include <linux/stm/fdma-reqs.h>

#define SYSCONF_BASE 0xfd704000
#define SYSCONF_DEVICEID	(SYSCONF_BASE + 0x000)
#define SYSCONF_SYS_STA(n)	(SYSCONF_BASE + 0x008 + ((n) * 4))
#define SYSCONF_SYS_CFG(n)	(SYSCONF_BASE + 0x100 + ((n) * 4))

#define UHOST2C_BASE(N)                 (0xfd200000 + ((N)*0x00100000))

#define AHB2STBUS_WRAPPER_GLUE_BASE(N)  (UHOST2C_BASE(N))
#define AHB2STBUS_RESERVED1_BASE(N)     (UHOST2C_BASE(N) + 0x000e0000)
#define AHB2STBUS_RESERVED2_BASE(N)     (UHOST2C_BASE(N) + 0x000f0000)
#define AHB2STBUS_OHCI_BASE(N)          (UHOST2C_BASE(N) + 0x000ffc00)
#define AHB2STBUS_EHCI_BASE(N)          (UHOST2C_BASE(N) + 0x000ffe00)
#define AHB2STBUS_PROTOCOL_BASE(N)      (UHOST2C_BASE(N) + 0x000fff00)

static u64 st40_dma_mask = 0xfffffff;

static void usb_power_up(void* dev)
{
	struct platform_device *pdev = dev;
	struct plat_usb_data *usb_wrapper = pdev->dev.platform_data;
	unsigned long sysconf;
	int port = usb_wrapper->port_number;
	struct stpio_pin *pio;
	const unsigned char power_pins[3] = {1, 3, 4};
	const unsigned char oc_pins[3] = {0, 2, 5};

	/* Power up port */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(22));
	sysconf &= ~(1<<(3+port));
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(22));

	/* Configure PIO pins */
	pio = stpio_request_pin(7, power_pins[port], "USB power",
				STPIO_ALT_OUT);
	stpio_set_pin(pio, 1);
	pio = stpio_request_pin(7, oc_pins[port], "USB oc",
				STPIO_ALT_BIDIR);
}

#define USB_WRAPPER(port)						\
{									\
	.ahb2stbus_wrapper_glue_base =AHB2STBUS_WRAPPER_GLUE_BASE(port),\
	.ahb2stbus_protocol_base = AHB2STBUS_PROTOCOL_BASE(port),	\
	.power_up = usb_power_up,					\
	.initialised = 0,						\
	.port_number = (port),						\
}

static struct plat_usb_data usb_wrapper[3] = {
	USB_WRAPPER(0),
	USB_WRAPPER(1),
	USB_WRAPPER(2)
};

#define USB_EHCI_DEVICE(port)						\
{									\
	.name = "ST40-ehci",						\
	.id=(port),							\
	.dev = {							\
		.dma_mask = &st40_dma_mask,				\
		.coherent_dma_mask = 0xffffffful,			\
		.platform_data = &usb_wrapper[port],			\
	},								\
	.num_resources = 2,						\
	.resource = (struct resource[]) {				\
		[0] = {							\
			.start = AHB2STBUS_EHCI_BASE(port),		\
			.end   = AHB2STBUS_EHCI_BASE(port) + 0xff,	\
			.flags = IORESOURCE_MEM,			\
		},							\
		[1] = {							\
			.start = ILC_IRQ(80+(port*2)),			\
			.end   = ILC_IRQ(80+(port*2)),			\
			.flags = IORESOURCE_IRQ,			\
		},							\
	},								\
}									\

static struct platform_device st40_ehci_devices[3] = {
	USB_EHCI_DEVICE(0),
	USB_EHCI_DEVICE(1),
	USB_EHCI_DEVICE(2),
};

#define USB_OHCI_DEVICE(port)						\
{									\
	.name = "ST40-ohci",						\
	.id=(port),							\
	.dev = {							\
		.dma_mask = &st40_dma_mask,				\
		.coherent_dma_mask = 0xffffffful,			\
		.platform_data = &usb_wrapper[port],			\
	},								\
	.num_resources = 2,						\
	.resource = (struct resource[]) {				\
		[0] = {							\
			.start = AHB2STBUS_OHCI_BASE(port),		\
			.end   = AHB2STBUS_OHCI_BASE(port) + 0xff,	\
			.flags = IORESOURCE_MEM,			\
		},							\
		[1] = {							\
			.start = ILC_IRQ(81+(port*2)),			\
			.end   = ILC_IRQ(81+(port*2)),			\
			.flags = IORESOURCE_IRQ,			\
		}							\
	}								\
}

static struct platform_device  st40_ohci_devices[3] = {
	USB_OHCI_DEVICE(0),
	USB_OHCI_DEVICE(1),
	USB_OHCI_DEVICE(2)
};

#ifdef CONFIG_STM_DMA

#include <linux/stm/7200_cut1_fdma2_firmware.h>

static struct fdma_regs stb7200_fdma_regs = {
	.fdma_id= FDMA2_ID,
	.fdma_ver = FDAM2_VER,
	.fdma_en = FDMA2_ENABLE_REG,
	.fdma_clk_gate = FDMA2_CLOCKGATE,
	.fdma_rev_id = FDMA2_REV_ID,
	.fdma_cmd_statn = STB7200_FDMA_CMD_STATn_REG,
	.fdma_ptrn = STB7200_FDMA_PTR_REG,
	.fdma_cntn = STB7200_FDMA_COUNT_REG,
	.fdma_saddrn = STB7200_FDMA_SADDR_REG,
	.fdma_daddrn = STB7200_FDMA_DADDR_REG,
	.fdma_req_ctln = STB7200_FDMA_REQ_CTLn_REG,
	.fdma_cmd_sta = FDMA2_CMD_MBOX_STAT_REG,
	.fdma_cmd_set = FDMA2_CMD_MBOX_SET_REG,
	.fdma_cmd_clr = FDMA2_CMD_MBOX_CLR_REG,
	.fdma_cmd_mask = FDMA2_CMD_MBOX_MASK_REG,
	.fdma_int_sta = FDMA2_INT_STAT_REG,
	.fdma_int_set = FDMA2_INT_SET_REG,
	.fdma_int_clr= FDMA2_INT_CLR_REG,
	.fdma_int_mask= FDMA2_INT_MASK_REG,
	.fdma_sync_reg= FDMA2_SYNCREG,
	.fdma_dmem_region = STB7200_DMEM_OFFSET,
	.fdma_imem_region = STB7200_IMEM_OFFSET,
};

static struct fdma_platform_device_data stb7200_fdma0_plat_data = {
	.registers_ptr = &stb7200_fdma_regs,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
	.fw_device_name = "stb7200_v1.4.bin",
	.fw.data_reg = (unsigned long*)&STB7200_DMEM_REGION,
	.fw.imem_reg = (unsigned long*)&STB7200_IMEM_REGION,
	.fw.imem_fw_sz = STB7200_IMEM_FIRMWARE_SZ,
	.fw.dmem_fw_sz = STB7200_DMEM_FIRMWARE_SZ,
	.fw.dmem_len = STB7200_DMEM_REGION_LENGTH,
	.fw.imem_len = STB7200_IMEM_REGION_LENGTH
};


static struct fdma_platform_device_data stb7200_fdma1_plat_data = {
	.registers_ptr = &stb7200_fdma_regs,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
	.fw_device_name = "stb7200_v1.4.bin",
	.fw.data_reg = (unsigned long*)&STB7200_DMEM_REGION,
	.fw.imem_reg = (unsigned long*)&STB7200_IMEM_REGION,
	.fw.imem_fw_sz = STB7200_IMEM_FIRMWARE_SZ,
	.fw.dmem_fw_sz = STB7200_DMEM_FIRMWARE_SZ,
	.fw.dmem_len = STB7200_DMEM_REGION_LENGTH,
	.fw.imem_len = STB7200_IMEM_REGION_LENGTH
};

#define stb7200_fdma0_plat_data_addr &stb7200_fdma0_plat_data
#define stb7200_fdma1_plat_data_addr &stb7200_fdma1_plat_data
#else
#define stb7200_fdma0_plat_data_addr NULL
#define stb7200_fdma1_plat_data_addr NULL
#endif /* CONFIG_STM_DMA */

static struct platform_device fdma0_7200_device = {
	.name		= "stmfdma",
	.id		= 0,
	.num_resources	= 2,
	.resource = (struct resource[2]) {
		[0] = {
			.start = STB7200_FDMA0_BASE,
			.end   = STB7200_FDMA0_BASE + 0x10000,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = LINUX_FDMA0_STB7200_IRQ_VECT,
			.end   = LINUX_FDMA0_STB7200_IRQ_VECT,
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = stb7200_fdma0_plat_data_addr,
	},
};

static struct platform_device fdma1_7200_device = {
	.name		= "stmfdma",
	.id		= 1,
	.resource = (struct resource[2]) {
		[0] = {
			.start = STB7200_FDMA1_BASE,
			.end   = STB7200_FDMA1_BASE + 0x10000,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = LINUX_FDMA1_STB7200_IRQ_VECT,
			.end   = LINUX_FDMA1_STB7200_IRQ_VECT,
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = stb7200_fdma1_plat_data_addr,
	},
};

static struct platform_device fdma_xbar_device = {
	.name		= "fdma-xbar",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[1]) {
		{
			.start	= STB7200_XBAR_BASE,
			.end	= STB7200_XBAR_BASE+(4*1024),
			.flags	= IORESOURCE_MEM,
		},
	},
};

static struct resource ssc_resource[] = {
	[0] = {
		.start = 0xfd040000,
		.end   = 0xfd040000 + 0x108,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = 0xfd041000,
		.end   = 0xfd041000 + 0x108,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.start = 0xfd042000,
		.end   = 0xfd042000 + 0x108,
		.flags = IORESOURCE_MEM,
	},
	[3] = {
		.start = 0xfd043000,
		.end   = 0xfd043000 + 0x108,
		.flags = IORESOURCE_MEM,
	},
	[4] = {
		.start = 0xfd044000,
		.end   = 0xfd044000 + 0x108,
		.flags = IORESOURCE_MEM,
	},
	[5] = {
		.start = ILC_IRQ(108),
		.end   = ILC_IRQ(108),
		.flags = IORESOURCE_IRQ,
	},
	[6] = {
		.start = ILC_IRQ(109),
		.end   = ILC_IRQ(109),
		.flags = IORESOURCE_IRQ,
	},
	[7] = {
		.start = ILC_IRQ(110),
		.end   = ILC_IRQ(110),
		.flags = IORESOURCE_IRQ,
	},
	[8] = {
		.start = ILC_IRQ(111),
		.end   = ILC_IRQ(111),
		.flags = IORESOURCE_IRQ,
	},
	[9] = {
		.start = ILC_IRQ(112),
		.end   = ILC_IRQ(112),
		.flags = IORESOURCE_IRQ,
	},
};

static struct plat_ssc_pio_t ssc_pio[] = {
	{2, 0, 2, 1, 0xff, 0xff},
	{3, 0, 3, 1, 3, 2},
	{4, 0, 4, 1, 0xff, 0xff},
	{5, 0, 5, 1, 5, 2},
	{7, 6, 7, 7, 0xff, 0xff},
};

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		((SSC_I2C_CAPABILITY                     ) << (0*2)) |
		((SSC_I2C_CAPABILITY | SSC_SPI_CAPABILITY) << (1*2)) |
		((SSC_I2C_CAPABILITY                     ) << (2*2)) |
		((SSC_I2C_CAPABILITY | SSC_SPI_CAPABILITY) << (3*2)) |
		((SSC_I2C_CAPABILITY                     ) << (4*2)),
	.pio         = ssc_pio
};

struct platform_device ssc_device = {
	.name = "ssc",
	.id = -1,
	.num_resources = ARRAY_SIZE(ssc_resource),
	.resource = ssc_resource,
	.dev = {
		.platform_data = &ssc_private_info
	}
};

#define RMII_MODE		(1<<0)
#define PHY_CLK_EXT		(1<<2)
#define MAC_SPEED		(1<<4)
#define VCI_ACK_SOURCE		(1<<6)
#define RESET			(1<<8)
#define DISABLE_MSG_READ	(1<<12)
#define DISABLE_MSG_WRITE	(1<<14)
/* Remaining bits define pad functions, default appears to work */

/* ETH MAC pad configuration */
void stx7200eth_hw_setup(int shift, int rmii_mode, int ext_clk)
{
	unsigned long sysconf;

	sysconf = ctrl_inl(SYSCONF_SYS_CFG(41));
	sysconf &= ~(DISABLE_MSG_READ << shift);
	sysconf &= ~(DISABLE_MSG_WRITE << shift);
	//sysconf |=  (VCI_ACK_SOURCE << shift);
	sysconf &= ~(VCI_ACK_SOURCE << shift);
	sysconf |=  (RESET << shift);

	if (rmii_mode) {
		sysconf |= (RMII_MODE << shift);
	} else {
		/* MII mode */
		sysconf &= ~(RMII_MODE << shift);
	}

	if (ext_clk) {
		sysconf |= (PHY_CLK_EXT << shift);
	} else {
		sysconf &= ~(PHY_CLK_EXT << shift);
	}

	ctrl_outl(sysconf, SYSCONF_SYS_CFG(41));
}

static void fix_mac_speed(void *priv, unsigned int speed)
{
	unsigned long sysconf;
	unsigned shift = (unsigned)priv;

	/* FIXME: lock needed here */

	sysconf = ctrl_inl(SYSCONF_SYS_CFG(41));
	if (speed == 100)
		sysconf |= (MAC_SPEED << shift);
	else
		sysconf &= ~(MAC_SPEED << shift);

	ctrl_outl(sysconf, SYSCONF_SYS_CFG(41));
}

static struct plat_stmmacenet_data stmmaceth_private_data[2] = {
{
	/* MAC0: STE101P */
	.bus_id = 0,
	.phy_addr = 0,
	.phy_mask = 0,
	.pbl = 32,
	.fix_mac_speed = fix_mac_speed,
	.bsp_priv = 0,
}, {
	/* MAC1: SMSC LAN 8700 */
	.bus_id = 1,
	.phy_addr = 1,
	.phy_mask = 0,
	.pbl = 32,
	.fix_mac_speed = fix_mac_speed,
	.bsp_priv = 1,
} };

static struct platform_device stmmaceth_device[2] = {
{
	.name		= "stmmaceth",
	.id		= 0,
	.num_resources	= 3,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd500000,
			.end	= 0xfd50ffff,
			.flags	= IORESOURCE_MEM,
		},
		{
			.name	= "macirq",
			.start	= ILC_IRQ(92),
			.end	= ILC_IRQ(92),
			.flags	= IORESOURCE_IRQ,
		},
		{
			.name	= "phyirq",
			/* This should be:
			 * .start	= ILC_IRQ(93),
			 * .end	= ILC_IRQ(93),
			 * but because the mb519 uses the MII0_MDINT line
			 * as MODE4, and the STE101P MDINT pin is O/C,
			 * there may or maynot be a pull-up resistor
			 * depending on switch SW1-4. Most of the time there
			 * isn't, so disable the interrupt.
			 */
			.start	= -1,
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &stmmaceth_private_data[0],
	}
}, {
	.name		= "stmmaceth",
	.id		= 1,
	.num_resources	= 3,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd510000,
			.end	= 0xfd51ffff,
			.flags	= IORESOURCE_MEM,
		},
		{
			.name	= "macirq",
			.start	= ILC_IRQ(94),
			.end	= ILC_IRQ(94),
			.flags	= IORESOURCE_IRQ,
		},
		{
			.name	= "phyirq",
			.start	= ILC_IRQ(95),
			.end	= ILC_IRQ(95),
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &stmmaceth_private_data[1],
	}
} };

static struct platform_device *stx7200_devices[] __initdata = {
	&stmmaceth_device[0],
	&stmmaceth_device[1],
	&st40_ehci_devices[0],
	&st40_ohci_devices[0],
	&st40_ehci_devices[1],
	&st40_ohci_devices[1],
	&st40_ehci_devices[2],
	&st40_ohci_devices[2],
	&ssc_device,
	&fdma0_7200_device,
	//&fdma1_7200_device,
	&fdma_xbar_device,
};

static int __init stx7200_devices_setup(void)
{
	return platform_add_devices(stx7200_devices, ARRAY_SIZE(stx7200_devices));
}
device_initcall(stx7200_devices_setup);

/*
 * INTC style interrupts
 */
static struct ipr_data ipr_map[] = {
	/* IRQ, IPR-idx, shift, priority */
	{ 16, 0, 12, 2 }, /* TMU0 TUNI*/
	{ 17, 0, 12, 2 }, /* TMU1 TUNI */
	{ 18, 0,  4, 2 }, /* TMU2 TUNI */
	{ 19, 0,  4, 2 }, /* TMU2 TIPCI */
	{ 27, 1, 12, 2 }, /* WDT ITI */
	{ 20, 0,  0, 2 }, /* RTC ATI (alarm) */
	{ 21, 0,  0, 2 }, /* RTC PRI (period) */
	{ 22, 0,  0, 2 }, /* RTC CUI (carry) */
	{ 23, 1,  4, 3 }, /* SCI ERI */
	{ 24, 1,  4, 3 }, /* SCI RXI */
	{ 25, 1,  4, 3 }, /* SCI TXI */
	{ 40, 2,  4, 3 }, /* SCIF ERI */
	{ 41, 2,  4, 3 }, /* SCIF RXI */
	{ 42, 2,  4, 3 }, /* SCIF BRI */
	{ 43, 2,  4, 3 }, /* SCIF TXI */
	{ 34, 2,  8, 7 }, /* DMAC DMTE0 */
	{ 35, 2,  8, 7 }, /* DMAC DMTE1 */
	{ 36, 2,  8, 7 }, /* DMAC DMTE2 */
	{ 37, 2,  8, 7 }, /* DMAC DMTE3 */
	{ 28, 2,  8, 7 }, /* DMAC DMAE */
};


static struct ipr_data ipr_irq_table[] = {
	/* IRQ, IPR-idx, shift, priority */
	{ 16, 0, 12, 2 }, /* TMU0 TUNI*/
	{ 17, 0,  8, 2 }, /* TMU1 TUNI */
	{ 18, 0,  4, 2 }, /* TMU2 TUNI */
	{ 27, 1, 12, 2 }, /* WDT ITI */
	{ 32, 2,  0, 7 }, /* HUDI */
};

static unsigned long ipr_offsets[] = {
	0xffd00004UL,	/* 0: IPRA */
	0xffd00008UL,	/* 1: IPRB */
	0xffd0000cUL,	/* 2: IPRC */
};

static struct ipr_desc ipr_irq_desc = {
	.ipr_offsets	= ipr_offsets,
	.nr_offsets	= ARRAY_SIZE(ipr_offsets),

	.ipr_data	= ipr_irq_table,
	.nr_irqs	= ARRAY_SIZE(ipr_irq_table),

	.chip = {
		.name	= "IPR-stx710x",
	},
};

static struct irq_chip stx7200_ipr_chip = {
	.name = "IPR",
};

void __init plat_irq_setup(void)
{
	int irq;

	register_ipr_controller(&ipr_irq_desc);
	for (irq=0; irq<16; irq++) {
		set_irq_chip(irq, &stx7200_ipr_chip);
		set_irq_chained_handler(irq, ilc_irq_demux);
	}
	init_IRQ_ilc();
}
