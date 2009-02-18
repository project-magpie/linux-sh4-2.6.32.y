/*
 * STx7141 Setup
 *
 * Copyright (C) 2008 STMicroelectronics Limited
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
#include <linux/i2c.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/pio.h>
#include <linux/phy.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/ata_platform.h>
#include <linux/stm/fdma-plat.h>
#include <linux/stm/fdma-reqs.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <asm/irq-ilc.h>

static u64 st40_dma_mask = DMA_32BIT_MASK;

static struct {
	unsigned char syscfg;
	unsigned char lsb, msb;
} pio_sysconf[17][8] = {
	{
		/* PIO0 doesn't exist */
	}, {
		{ 19,  0,  1 },	/* PIO1[0] */
		{ 19,  2,  3 },	/* PIO1[1] */
		{ 19,  4,  5 },	/* PIO1[2] */
		{ 19,  5,  7 },	/* PIO1[3] */
		{ 19,  8,  8 },	/* PIO1[4] */
		{ 19,  9,  9 },	/* PIO1[5] */
		{ 19, 10, 10 },	/* PIO1[6] */
		{ 19, 11, 11 },	/* PIO1[7] */
	}, {
		{ 19, 12, 12 },	/* PIO2[0] */
		{ 19, 13, 13 },	/* PIO2[1] */
		{ 19, 14, 14 },	/* PIO2[2] */
		{ 19, 15, 15 },	/* PIO2[3] */
		{ 19, 16, 16 },	/* PIO2[4] */
		{ 19, 17, 17 },	/* PIO2[5] */
		{ 19, 18, 18 },	/* PIO2[6] */
		{ 19, 19, 19 },	/* PIO2[7] */
	}, {
		{ 19, 20, 20 },	/* PIO3[0] */
		{ 19, 21, 21 },	/* PIO3[1] */
		{ 19, 22, 23 },	/* PIO3[2] */
		{ 19, 24, 25 },	/* PIO3[3] */
		{ 19, 26, 27 },	/* PIO3[4] */
		{ 19, 28, 29 },	/* PIO3[5] */
		{ 19, 30, 31 },	/* PIO3[6] */
		{ 20,  0,  0 },	/* PIO3[7] */
	}, {
		{ 20,  1,  2 },	/* PIO4[0] */
		{ 20,  3,  4 },	/* PIO4[1] */
		{ 20,  5,  6 },	/* PIO4[2] */
		{ 20,  7,  8 },	/* PIO4[3] */
		{ 20,  9, 10 },	/* PIO4[4] */
		{ 20, 11, 12 },	/* PIO4[5] */
		{ 20, 13, 13 },	/* PIO4[6] */
		{ 20, 14, 14 },	/* PIO4[7] */
	}, {
		{ 20, 15, 16 },	/* PIO5[0] */
		{ 20, 17, 18 },	/* PIO5[1] */
		{ 20, 19, 19 },	/* PIO5[2] */
		{ 20, 20, 20 },	/* PIO5[3] */
		{ 20, 21, 21 },	/* PIO5[4] */
		{ 20, 22, 23 },	/* PIO5[5] */
		{ 20, 24, 24 },	/* PIO5[6] */
		{ 20, 25, 26 },	/* PIO5[7] */
	}, {
		{ 20, 27, 28 },	/* PIO6[0] */
		{ 20, 29, 30 },	/* PIO6[1] */
		{ 25,  0,  1 },	/* PIO6[2] */
		{ 25,  2,  3 },	/* PIO6[3] */
		{ 25,  4,  5 },	/* PIO6[4] */
		{ 25,  6,  7 },	/* PIO6[5] */
		{ 25,  8,  9 },	/* PIO6[6] */
		{ 25, 10, 11 },	/* PIO6[7] */
	}, {
		{ 25, 12, 13 },	/* PIO7[0] */
		{ 25, 14, 15 },	/* PIO7[1] */
		{ 25, 16, 17 },	/* PIO7[2] */
		{ 25, 18, 19 },	/* PIO7[3] */
		{ 25, 20, 21 },	/* PIO7[4] */
		{ 25, 22, 23 },	/* PIO7[5] */
		{ 25, 24, 25 },	/* PIO7[6] */
		{ 25, 26, 27 },	/* PIO7[7] */
	}, {
		{ 25, 28, 30 },	/* PIO8[0] */
		{ 35,  0,  2 },	/* PIO8[1] */
		{ 35,  3,  5 },	/* PIO8[2] */
		{ 35,  6,  8 },	/* PIO8[3] */
		{ 35,  9, 11 },	/* PIO8[4] */
		{ 35, 12, 14 },	/* PIO8[5] */
		{ 35, 15, 17 },	/* PIO8[6] */
		{ 35, 18, 20 },	/* PIO8[7] */
	}, {
		{ 35, 21, 22 },	/* PIO9[0] */
		{ 35, 23, 24 },	/* PIO9[1] */
		{ 35, 25, 26 },	/* PIO9[2] */
		{ 35, 27, 28 },	/* PIO9[3] */
		{ 35, 29, 30 },	/* PIO9[4] */
		{ 46,  0,  1 },	/* PIO9[5] */
		{ 46,  2,  3 },	/* PIO9[6] */
		{ 46,  4,  5 },	/* PIO9[7] */
	}, {
		{ 46,  6,  7 },	/* PIO10[0] */
		{ 46,  8,  9 },	/* PIO10[1] */
		{ 46, 10, 11 },	/* PIO10[2] */
		{ 46, 12, 13 },	/* PIO10[3] */
		{ 46, 14, 15 },	/* PIO10[4] */
		{ 46, 16, 17 },	/* PIO10[5] */
		{ 46, 18, 19 },	/* PIO10[6] */
		{ 46, 20, 21 },	/* PIO10[7] */
	}, {
		{ 46, 22, 23 },	/* PIO11[0] */
		{ 46, 24, 26 },	/* PIO11[1] */
		{ 46, 27, 29 },	/* PIO11[2] */
		{ 47,  0,  2 },	/* PIO11[3] */
		{ 47,  3,  5 },	/* PIO11[4] */
		{ 47,  6,  8 },	/* PIO11[5] */
		{ 47,  9, 11 },	/* PIO11[6] */
		{ 47, 12, 14 },	/* PIO11[7] */
	}, {
		{ 47, 15, 17 },	/* PIO12[0] */
		{ 47, 18, 20 },	/* PIO12[1] */
		{ 47, 21, 23 },	/* PIO12[2] */
		{ 47, 24, 25 },	/* PIO12[3] */
		{ 47, 26, 27 },	/* PIO12[4] */
		{ 47, 28, 29 },	/* PIO12[5] */
		{ 48,  0,  2 },	/* PIO12[6] */
		{ 48,  3,  5 },	/* PIO12[7] */
	}, {
		{ 48,  6,  8 },	/* PIO13[0] */
		{ 48,  9, 11 },	/* PIO13[1] */
		{ 48, 12, 14 },	/* PIO13[2] */
		{ 48, 15, 17 },	/* PIO13[3] */
		{ 48, 18, 20 },	/* PIO13[4] */
		{ 48, 21, 23 },	/* PIO13[5] */
		{ 48, 24, 25 },	/* PIO13[6] */
		{ 48, 26, 27 },	/* PIO13[7] */
	}, {
		{ 48, 28, 30 },	/* PIO14[0] */
		{ 49,  0,  2 },	/* PIO14[1] */
		{ 49,  3,  5 },	/* PIO14[2] */
		{ 49,  6,  8 },	/* PIO14[3] */
		{ 49,  9, 11 },	/* PIO14[4] */
		{ 49, 12, 14 },	/* PIO14[5] */
		{ 49, 15, 17 },	/* PIO14[6] */
		{ 49, 18, 19 },	/* PIO14[7] */
	}, {
		{ 49, 20, 21 },	/* PIO15[0] */
		{ 49, 22, 23 },	/* PIO15[1] */
		{ 49, 24, 25 },	/* PIO15[2] */
		{ 49, 26, 27 },	/* PIO15[3] */
		{ 49, 28, 28 },	/* PIO15[4] */
		{ 49, 29, 29 },	/* PIO15[5] */
		{ 49, 30, 30 },	/* PIO15[6] */
		{ 50,  0,  1 },	/* PIO15[7] */
	}, {
		{ 50,  2,  3 },	/* PIO16[0] */
		{ 50,  4,  5 },	/* PIO16[1] */
		{ 50,  6,  7 },	/* PIO16[2] */
		{ 50,  8,  8 },	/* PIO16[3] */
		{ 50,  9,  9 },	/* PIO16[4] */
		{ 50, 10, 10 },	/* PIO16[5] */
		{ 50, 11, 11 },	/* PIO16[6] */
		{ 50, 12, 12 },	/* PIO16[7] */
	}
};

static void stx7141_pio_sysconf(int bank, int pin, int alt, const char *name)
{
	struct sysconf_field *sc;

	sc = sysconf_claim(SYS_CFG,
			   pio_sysconf[bank][pin].syscfg,
			   pio_sysconf[bank][pin].lsb,
			   pio_sysconf[bank][pin].msb, name);
	sysconf_write(sc, alt);
}

/* USB resources ----------------------------------------------------------- */

#define AHB2STBUS_WRAPPER_GLUE_OFFSET	0x00000
#define AHB2STBUS_OHCI_OFFSET		0xffc00
#define AHB2STBUS_EHCI_OFFSET		0xffe00
#define AHB2STBUS_PROTOCOL_OFFSET	0xfff00

static struct plat_usb_data usb_wrapper[4] = {
	/* USB2_0 */
	USB_WRAPPER(0, 0xfe100000 + AHB2STBUS_WRAPPER_GLUE_OFFSET,
		    0xfe100000 + AHB2STBUS_PROTOCOL_OFFSET,
		    USB_FLAGS_STRAP_16BIT	|
		    USB_FLAGS_STRAP_PLL		|
		    USB_FLAGS_STBUS_CONFIG_THRESHOLD256),
	/* USB2_1 */
	USB_WRAPPER(1, 0xfea00000 + AHB2STBUS_WRAPPER_GLUE_OFFSET,
		    0xfea00000 + AHB2STBUS_PROTOCOL_OFFSET,
		    USB_FLAGS_STRAP_16BIT	|
		    USB_FLAGS_STRAP_PLL		|
		    USB_FLAGS_STBUS_CONFIG_THRESHOLD256),
	/* USB1_0 */
	USB_WRAPPER(2, 0xfeb00000 + AHB2STBUS_WRAPPER_GLUE_OFFSET,
		    0xfeb00000 + AHB2STBUS_PROTOCOL_OFFSET,
		    USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE),
	/* USB1_1 */
	USB_WRAPPER(3, 0xfec00000 + AHB2STBUS_WRAPPER_GLUE_OFFSET,
		    0xfec00000 + AHB2STBUS_PROTOCOL_OFFSET,
		    USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE),
};

static struct platform_device  st_usb_device[4] = {
USB_DEVICE(0, 0xfe100000 + AHB2STBUS_EHCI_OFFSET, ILC_IRQ(93),
	0xfe100000 + AHB2STBUS_OHCI_OFFSET, ILC_IRQ(94), &usb_wrapper[0]),
USB_DEVICE(1, 0xfea00000 + AHB2STBUS_EHCI_OFFSET, ILC_IRQ(95),
	0xfea00000 + AHB2STBUS_OHCI_OFFSET, ILC_IRQ(96), &usb_wrapper[1]),
USB_DEVICE(2, NULL, NULL, 0xfeb00000 + AHB2STBUS_OHCI_OFFSET,
	ILC_IRQ(97), &usb_wrapper[2]),
USB_DEVICE(3, NULL, NULL, 0xfec00000 + AHB2STBUS_OHCI_OFFSET,
	ILC_IRQ(98), &usb_wrapper[3])
};

void __init stx7141_configure_usb(int port)
{
	static int first = 1;
	struct sysconf_field *sc;
	const struct {
		struct {
			unsigned char port, pin, alt;
		} pwr, oc;
	} usb_pins[4] = {
		{ { 4, 7, 1 }, { 4, 6, 1 } },
		{ { 5, 1, 1 }, { 5, 0, 1 } },
		{ { 4, 3, 1 }, { 4, 2, 1 } },
		{ { 4, 5, 1 }, { 4, 4, 1 } }
	};

	if (first) {
		/* ENABLE_USB48_CLK: Enable 48 MHz clock */
		sc = sysconf_claim(SYS_CFG, 4, 5, 5, "USB");
		sysconf_write(sc, 1);

		first = 0;
	}

	/* Power up USB */
#ifndef CONFIG_PM
	sc = sysconf_claim(SYS_CFG, 32, 7+port, 7+port, "USB");
	sysconf_write(sc, 0);
	sc = sysconf_claim(SYS_STA, 15, 7+port, 7+port, "USB");
	do {
	} while (sysconf_read(sc));
#endif

	stx7141_pio_sysconf(usb_pins[port].pwr.port,
			    usb_pins[port].pwr.pin,
			    usb_pins[port].pwr.alt, "USB");
	stpio_request_pin(usb_pins[port].pwr.port,
			  usb_pins[port].pwr.pin, "USB", STPIO_OUT);

	stx7141_pio_sysconf(usb_pins[port].oc.port,
			    usb_pins[port].oc.pin,
			    usb_pins[port].oc.alt, "USB");
	if (port > 1) {
		/* Overcurrent detection is active high, so force
		 * the pin low. */
		stpio_request_set_pin(usb_pins[port].oc.port,
				      usb_pins[port].oc.pin, "USB",
				      STPIO_OUT, 0);
	} else {
		stpio_request_pin(usb_pins[port].oc.port,
				  usb_pins[port].oc.pin, "USB", STPIO_IN);
	}

	platform_device_register(&st_usb_device[port]);
}

/* FDMA resources ---------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

#include <linux/stm/7200_cut1_fdma2_firmware.h>

static struct fdma_regs stx7141_fdma_regs = {
	.fdma_id = FDMA2_ID,
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
	.fdma_int_clr = FDMA2_INT_CLR_REG,
	.fdma_int_mask = FDMA2_INT_MASK_REG,
	.fdma_sync_reg = FDMA2_SYNCREG,
	.fdma_dmem_region = STX7141_DMEM_OFFSET,
	.fdma_imem_region = STX7141_IMEM_OFFSET,
};

static struct fdma_platform_device_data stx7141_fdma0_plat_data = {
	.registers_ptr = &stx7141_fdma_regs,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
	.fw_device_name = "stb7200_v1.4.bin",
	.fw.data_reg = (unsigned long *)&STB7200_DMEM_REGION,
	.fw.imem_reg = (unsigned long *)&STB7200_IMEM_REGION,
	.fw.imem_fw_sz = STB7200_IMEM_FIRMWARE_SZ,
	.fw.dmem_fw_sz = STB7200_DMEM_FIRMWARE_SZ,
	.fw.dmem_len = STB7200_DMEM_REGION_LENGTH,
	.fw.imem_len = STB7200_IMEM_REGION_LENGTH
};


static struct fdma_platform_device_data stx7141_fdma1_plat_data = {
	.registers_ptr = &stx7141_fdma_regs,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
	.fw_device_name = "stb7200_v1.4.bin",
	.fw.data_reg = (unsigned long *)&STB7200_DMEM_REGION,
	.fw.imem_reg = (unsigned long *)&STB7200_IMEM_REGION,
	.fw.imem_fw_sz = STB7200_IMEM_FIRMWARE_SZ,
	.fw.dmem_fw_sz = STB7200_DMEM_FIRMWARE_SZ,
	.fw.dmem_len = STB7200_DMEM_REGION_LENGTH,
	.fw.imem_len = STB7200_IMEM_REGION_LENGTH
};

#define stx7141_fdma0_plat_data_addr &stx7141_fdma0_plat_data
#define stx7141_fdma1_plat_data_addr &stx7141_fdma1_plat_data
#else
#define stx7141_fdma0_plat_data_addr NULL
#define stx7141_fdma1_plat_data_addr NULL
#endif /* CONFIG_STM_DMA */

static struct platform_device fdma0_device = {
	.name		= "stmfdma",
	.id		= 0,
	.num_resources	= 2,
	.resource = (struct resource[2]) {
		[0] = {
			.start = STX7141_FDMA0_BASE,
			.end   = STX7141_FDMA0_BASE + 0xffff,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = LINUX_FDMA0_STX7141_IRQ_VECT,
			.end   = LINUX_FDMA0_STX7141_IRQ_VECT,
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = stx7141_fdma0_plat_data_addr,
	},
};

static struct platform_device fdma1_device = {
	.name		= "stmfdma",
	.id		= 1,
	.resource = (struct resource[2]) {
		[0] = {
			.start = STX7141_FDMA1_BASE,
			.end   = STX7141_FDMA1_BASE + 0xffff,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = LINUX_FDMA1_STX7141_IRQ_VECT,
			.end   = LINUX_FDMA1_STX7141_IRQ_VECT,
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = stx7141_fdma1_plat_data_addr,
	},
};

static struct platform_device fdma_xbar_device = {
	.name		= "fdma-xbar",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[1]) {
		{
			.start	= STX7141_XBAR_BASE,
			.end	= STX7141_XBAR_BASE+(4*1024)-1,
			.flags	= IORESOURCE_MEM,
		},
	},
};

/* SSC resources ----------------------------------------------------------- */

static char i2c_st[] = "i2c_st";
static char spi_st[] = "spi_st_ssc";

static struct platform_device stssc_devices[] = {
	STSSC_DEVICE(0xfd040000, ILC_IRQ(69), 2, 0, 1, 2),
	STSSC_DEVICE(0xfd041000, ILC_IRQ(70), 2, 3, 4, 5),
	STSSC_DEVICE(0xfd042000, ILC_IRQ(71), 2, 6, 7, 0xff),
	STSSC_DEVICE(0xfd043000, ILC_IRQ(72), 3, 0, 1, 0xff),
	STSSC_DEVICE(0xfd044000, ILC_IRQ(73), 3, 2, 3, 0xff),
	STSSC_DEVICE(0xfd045000, ILC_IRQ(74), 3, 4, 5, 6),
	STSSC_DEVICE(0xfd046000, ILC_IRQ(75), 4, 0, 1, 0xff),
};

void __init stx7141_configure_ssc(struct plat_ssc_data *data)
{
	int num_i2c = 0;
	int num_spi = 0;
	int i;
	int capability = data->capability;
	int pin;

	for (i = 0; i < ARRAY_SIZE(stssc_devices); i++, capability >>= 2) {
		struct ssc_pio_t *ssc_pio = stssc_devices[i].dev.platform_data;

		if (capability & SSC_UNCONFIGURED)
			continue;

		for (pin = 0; pin < 3; pin++) {
			int portno = ssc_pio->pio[pin].pio_port;
			int pinno  = ssc_pio->pio[pin].pio_pin;

			if ((pin == 2) && !(capability & SSC_SPI_CAPABILITY))
				continue;

#ifdef CONFIG_I2C_ST40_PIO
			stx7141_pio_sysconf(portno, pinno, 0, "ssc");
#else
			stx7141_pio_sysconf(portno, pinno, 1, "ssc");
#endif
		}

		if (capability & SSC_SPI_CAPABILITY) {
			stssc_devices[i].name = spi_st;
			stssc_devices[i].id = num_spi++;
			ssc_pio->chipselect = data->spi_chipselects[i];
		} else {
			stssc_devices[i].name = i2c_st;
			stssc_devices[i].id = num_i2c++;
			if (capability & SSC_I2C_CLK_UNIDIR)
				ssc_pio->clk_unidir = 1;
		}

		platform_device_register(&stssc_devices[i]);
	}

	/* I2C buses number reservation (to prevent any hot-plug device
	 * from using it) */
#ifdef CONFIG_I2C_BOARDINFO
	i2c_register_board_info(num_i2c - 1, NULL, 0);
#endif
}

/* SATA resources ---------------------------------------------------------- */

static struct plat_sata_data sata_private_info = {
	.phy_init = 0,
	.pc_glue_logic_init = 0,
	.only_32bit = 0,
};

static struct platform_device sata_device =
	SATA_DEVICE(0, 0xfe209000, ILC_IRQ(89), ILC_IRQ(88),
		    &sata_private_info);

void __init stx7141_configure_sata(void)
{
	if (cpu_data->cut_major >= 2)
		stm_sata_miphy_init();

	platform_device_register(&sata_device);
}

/* PATA resources ---------------------------------------------------------- */

/*
 * EMI A20 = CS1 (active low)
 * EMI A21 = CS0 (active low)
 * EMI A19 = DA2
 * EMI A18 = DA1
 * EMI A17 = DA0
 */

static struct resource pata_resources[] = {
	[0] = {	/* I/O base: CS1=N, CS0=A */
		.start	= (1<<20),
		.end	= (1<<20) + (8<<17)-1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {	/* CTL base: CS1=A, CS0=N, DA2=A, DA1=A, DA0=N */
		.start	= (1<<21) + (6<<17),
		.end	= (1<<21) + (6<<17) + 3,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {	/* IRQ */
		.flags	= IORESOURCE_IRQ,
	}
};

static struct pata_platform_info pata_info = {
	.ioport_shift	= 17,
};

static struct platform_device pata_device = {
	.name		= "pata_platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(pata_resources),
	.resource	= pata_resources,
	.dev = {
		.platform_data = &pata_info,
	}
};

void __init stx7141_configure_pata(int bank, int pc_mode, int irq)
{
	unsigned long bank_base;

	bank_base = emi_bank_base(bank);
	pata_resources[0].start += bank_base;
	pata_resources[0].end   += bank_base;
	pata_resources[1].start += bank_base;
	pata_resources[1].end   += bank_base;
	pata_resources[2].start = irq;
	pata_resources[2].end   = irq;

	emi_config_pata(bank, pc_mode);

	platform_device_register(&pata_device);
}

/* Ethernet MAC resources -------------------------------------------------- */

static void fix_mac_speed(void *priv, unsigned int speed)
{
	struct sysconf_field *sc = priv;

	sysconf_write(sc, (speed == SPEED_100) ? 1 : 0);
}

static struct plat_stmmacenet_data stx7141eth_private_data[2] = {
{
	.bus_id = 0,
	.pbl = 32,
	.has_gmac = 1,
	.fix_mac_speed = fix_mac_speed,
	.bsp_priv = 0,
}, {
	.bus_id = 1,
	.pbl = 32,
	.has_gmac = 1,
	.fix_mac_speed = fix_mac_speed,
	.bsp_priv = 1,
} };

static struct platform_device stx7141eth_devices[2] = {
{
	.name		= "stmmaceth",
	.id		= 0,
	.num_resources	= 2,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd110000,
			.end	= 0xfd117fff,
			.flags	= IORESOURCE_MEM,
		},
		{
			.name	= "macirq",
			.start	= ILC_IRQ(40),
			.end	= ILC_IRQ(40),
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &stx7141eth_private_data[0],
	}
}, {
	.name		= "stmmaceth",
	.id		= 1,
	.num_resources	= 2,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd118000,
			.end	= 0xfd11ffff,
			.flags	= IORESOURCE_MEM,
		},
		{
			.name	= "macirq",
			.start	= ILC_IRQ(47),
			.end	= ILC_IRQ(47),
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.power.can_wakeup = 1,
		.platform_data = &stx7141eth_private_data[1],
	}
} };

void stx7141_configure_ethernet(int port, int reverse_mii, int mode,
				int phy_bus)
{
	struct sysconf_field *sc;
	int i;
	struct {
		struct {
			unsigned char port, pin, alt;
		} pio[2];
		unsigned char dir;
	} mii_pins[] = {
		{ { {  8, 0, 1 }, { 11, 4, 1 } }, STPIO_IN  },	/* TXCLK */
		{ { {  8, 1, 1 }, { 11, 5, 1 } }, STPIO_OUT },	/* TXEN */
		{ { {  8, 2, 1 }, { 11, 6, 1 } }, STPIO_OUT },	/* TXER */
		{ { {  8, 3, 1 }, { 11, 7, 1 } }, STPIO_OUT },	/* TXD[0] */
		{ { {  8, 4, 1 }, { 12, 0, 1 } }, STPIO_OUT },	/* TXD[1] */
		{ { {  8, 5, 1 }, { 12, 1, 1 } }, STPIO_OUT },	/* TXD[2] */
		{ { {  8, 6, 1 }, { 12, 2, 1 } }, STPIO_OUT },	/* TXD[3] */
		{ { {  8, 7, 1 }, { 12, 3, 1 } }, STPIO_OUT },	/* TXD[4] */
		{ { {  9, 0, 1 }, { 12, 4, 1 } }, STPIO_OUT },	/* TXD[5] */
		{ { {  9, 1, 1 }, { 12, 5, 1 } }, STPIO_OUT },	/* TXD[6] */
		{ { {  9, 2, 1 }, { 12, 6, 1 } }, STPIO_OUT },	/* TXD[7] */
		{ { {  9, 3, 1 }, { 12, 7, 1 } }, STPIO_IN  },	/* RXCLK */
		{ { {  9, 4, 1 }, { 13, 0, 1 } }, STPIO_IN  },	/* RXDV */
		{ { {  9, 5, 1 }, { 13, 1, 1 } }, STPIO_IN  },	/* RX_ER */
		{ { {  9, 6, 1 }, { 13, 2, 1 } }, STPIO_IN  },	/* RXD[0] */
		{ { {  9, 7, 1 }, { 13, 3, 1 } }, STPIO_IN  },	/* RXD[1] */
		{ { { 10, 0, 1 }, { 13, 4, 1 } }, STPIO_IN  },	/* RXD[2] */
		{ { { 10, 1, 1 }, { 13, 5, 1 } }, STPIO_IN  },	/* RXD[3] */
		{ { { 10, 2, 1 }, { 13, 6, 1 } }, STPIO_IN  },	/* RXD[4] */
		{ { { 10, 3, 1 }, { 13, 7, 1 } }, STPIO_IN  },	/* RXD[5] */
		{ { { 10, 4, 1 }, { 14, 0, 1 } }, STPIO_IN  },	/* RXD[6] */
		{ { { 10, 5, 1 }, { 14, 1, 1 } }, STPIO_IN  },	/* RXD[7] */
		{ { { 10, 6, 1 }, { 14, 2, 1 } }, STPIO_IN  },	/* CRS */
		{ { { 10, 7, 1 }, { 14, 3, 1 } }, STPIO_IN  },	/* COL */
		{ { { 11, 0, 1 }, { 14, 4, 1 } }, STPIO_OUT },	/* MDC */
		{ { { 11, 1, 1 }, { 14, 5, 1 } }, STPIO_BIDIR },/* MDIO */
		{ { { 11, 2, 1 }, { 14, 6, 1 } }, STPIO_IN  },	/* MDINT */
		{ { { 11, 3, 1 }, { 14, 7, 1 } }, STPIO_OUT },	/* PHYCLK */
	};

	stx7141eth_private_data[port].bus_id = phy_bus;

	/* gmac_en: GMAC Enable */
	sc = sysconf_claim(SYS_CFG, 7, 16+port, 16+port, "stmmac");
	sysconf_write(sc, 1);

	/* GMII clock configuration */
	if (port == 0) {
		sc = sysconf_claim(SYS_CFG, 7, 13, 13, "stmmac");
		sysconf_write(sc, 1);
	} else if (port == 1) {
		sc = sysconf_claim(SYS_CFG, 7, 15, 15, "stmmac");
		sysconf_write(sc, 1);
	}

	/* enmii: Interface type (rev MII/MII) */
	sc = sysconf_claim(SYS_CFG, 7, port ? 31 : 27, port ? 31 : 27,
			   "stmmac");
	sysconf_write(sc, reverse_mii ? 0 : 1);

	/* mac_speed */
	stx7141eth_private_data[port].bsp_priv =
		sysconf_claim(SYS_CFG, 7, 20+port, 20+port, "stmmac");

	/* phy_intf_sel[2;0] : PHY Interface Selection */
	/* Note the that MSB implicitly also set mii_mode */
	sc = sysconf_claim(SYS_CFG, 7,
			   port ? 28 : 24, port ? 30 : 26, "stmmac");
	sysconf_write(sc, mode);

	for (i = 0; i < ARRAY_SIZE(mii_pins); i++) {
		stx7141_pio_sysconf(mii_pins[i].pio[port].port,
				    mii_pins[i].pio[port].pin,
				    mii_pins[i].pio[port].alt, "eth");
		stpio_request_pin(mii_pins[i].pio[port].port,
				  mii_pins[i].pio[port].pin,
				  "eth",
				  mii_pins[i].dir);
	}

	platform_device_register(&stx7141eth_devices[port]);
}

/* Audio output ------------------------------------------------------------ */

void stx7141_configure_audio_pins(int pcmout1, int pcmout2, int spdif,
		int pcmin1, int pcmin2)
{
	/* Claim PIO pins as first PCM player outputs, depending on
	 * how many DATA outputs are to be used... */

	if (pcmout1 > 0) {
		stx7141_pio_sysconf(15, 4, 1, "AUDD1_PCMCLKOUT");
		stpio_request_pin(15, 4, "AUDD1_PCMCLKOUT", STPIO_OUT);
		stx7141_pio_sysconf(15, 5, 1, "AUDD1_LRCLKOUT");
		stpio_request_pin(15, 5, "AUDD1_LRCLKOUT", STPIO_OUT);
		stx7141_pio_sysconf(15, 6, 1, "AUDD1_SCLKOUT");
		stpio_request_pin(15, 6, "AUDD1_SCLKOUT", STPIO_OUT);
		stx7141_pio_sysconf(15, 3, 1, "AUDD1_PCMOUT");
		stpio_request_pin(15, 3, "AUDD1_PCMOUT", STPIO_OUT);
	}
	if (pcmout1 > 1) {
		stx7141_pio_sysconf(15, 7, 2, "AUDD1_PCMOUT[1]");
		stpio_request_pin(15, 7, "AUDD1_PCMOUT", STPIO_OUT);
	}
	if (pcmout1 > 2) {
		stx7141_pio_sysconf(16, 0, 2, "AUDD1_PCMOUT[2]");
		stpio_request_pin(16, 0, "AUDD1_PCMOUT", STPIO_OUT);
	}
	if (pcmout1 > 3) {
		stx7141_pio_sysconf(16, 1, 2, "AUDD1_PCMOUT[3]");
		stpio_request_pin(16, 1, "AUDD1_PCMOUT", STPIO_OUT);
	}
	if (pcmout1 > 4) {
		stx7141_pio_sysconf(16, 2, 2, "AUDD1_PCMOUT[4]");
		stpio_request_pin(16, 2, "AUDD1_PCMOUT", STPIO_OUT);
	}
	if (pcmout1 > 5)
		BUG();

	/* Claim PIO pins for second PCM player outputs, however
	 * they are multiplexed with the first player's ones... */

	if (pcmout2 > 0) {
		if (pcmout1 > 1)
			BUG();

		stx7141_pio_sysconf(16, 0, 1, "AUDD2_PCMCLKOUT");
		stpio_request_pin(16, 0, "AUDD2_PCMCLKOUT", STPIO_OUT);
		stx7141_pio_sysconf(16, 1, 1, "AUDD2_LRCLKOUT");
		stpio_request_pin(16, 1, "AUDD2_LRCLKOUT", STPIO_OUT);
		stx7141_pio_sysconf(16, 2, 1, "AUDD2_SCLKOUT");
		stpio_request_pin(16, 2, "AUDD2_SCLKOUT", STPIO_OUT);
		stx7141_pio_sysconf(15, 7, 1, "AUDD2_PCMOUT");
		stpio_request_pin(15, 7, "AUDD2_PCMOUT", STPIO_OUT);
	}
	if (pcmout2 > 1)
		BUG();

	/* Claim PIO pin as SPDIF output... */

	if (spdif > 0) {
		stx7141_pio_sysconf(16, 3, 1, "AUDD_SPDIFOUT");
		stpio_request_pin(16, 3, "AUDD_SPDIFOUT", STPIO_OUT);
	}
	if (spdif > 1)
		BUG();


	/* Claim PIO for the first PCM reader inputs... */

	if (pcmin1 > 0) {
		stx7141_pio_sysconf(15, 0, 1, "AUDD1_PCMIN");
		stpio_request_pin(15, 0, "AUDD1_PCMIN", STPIO_IN);
		stx7141_pio_sysconf(15, 1, 1, "AUDD1_LRCLKIN");
		stpio_request_pin(15, 1, "AUDD1_LRCLKIN", STPIO_IN);
		stx7141_pio_sysconf(15, 2, 1, "AUDD1_SCLKIN");
		stpio_request_pin(15, 2, "AUDD1_SCLKIN", STPIO_IN);
	}
	if (pcmin1 > 1)
		BUG();

	/* Claim PIO for the second PCM reader inputs... */

	if (pcmin2 > 0) {
		stx7141_pio_sysconf(16, 4, 1, "AUDD2_PCMIN");
		stpio_request_pin(16, 4, "AUDD2_PCMIN", STPIO_IN);
		stx7141_pio_sysconf(16, 5, 1, "AUDD2_LRCLKIN");
		stpio_request_pin(16, 5, "AUDD2_LRCLKIN", STPIO_IN);
		stx7141_pio_sysconf(16, 6, 1, "AUDD2_SCLKIN");
		stpio_request_pin(16, 6, "AUDD2_SCLKIN", STPIO_IN);
	}
	if (pcmin2 > 1)
		BUG();
}

/* PWM resources ----------------------------------------------------------- */

static struct resource stm_pwm_resource[] = {
	[0] = {
		.start	= 0xfd010000,
		.end	= 0xfd010000 + 0x67,
		.flags	= IORESOURCE_MEM
	},
	[1] = {
		.start	= ILC_IRQ(85),
		.end	= ILC_IRQ(85),
		.flags	= IORESOURCE_IRQ
	}
};

static struct platform_device stm_pwm_device = {
	.name		= "stm-pwm",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stm_pwm_resource),
	.resource	= stm_pwm_resource,
};

void stx7141_configure_pwm(struct plat_stm_pwm_data *data)
{
	int pwm;
	const struct {
		unsigned char port, pin, alt;
	} pwm_pios[2] = {
		{ 3, 4, 2 },	/* PWM0 */
		{ 4, 2, 2 },	/* PWM1 */
	};

	stm_pwm_device.dev.platform_data = data;

	for (pwm = 0; pwm < 2; pwm++) {
		if (data->flags & (1<<pwm)) {
			int port = pwm_pios[pwm].port;
			int pin  = pwm_pios[pwm].pin;
			int alt  = pwm_pios[pwm].alt;

			stx7141_pio_sysconf(port, pin, alt, "pwm");
			stpio_request_pin(port, pin, "pwm", STPIO_ALT_OUT);
		}
	}

	platform_device_register(&stm_pwm_device);
}

/* Hardware RNG resources -------------------------------------------------- */

static struct platform_device hwrandom_rng_device = {
	.name	   = "stm_hwrandom",
	.id	     = -1,
	.num_resources  = 1,
	.resource       = (struct resource[]){
		{
			.start  = 0xfe250000,
			.end    = 0xfe250fff,
			.flags  = IORESOURCE_MEM
		},
	}
};

static struct platform_device devrandom_rng_device = {
	.name           = "stm_rng",
	.id             = 0,
	.num_resources  = 1,
	.resource       = (struct resource[]){
		{
			.start  = 0xfe250000,
			.end    = 0xfe250fff,
			.flags  = IORESOURCE_MEM
		},
	}
};

/* ASC resources ----------------------------------------------------------- */

static struct platform_device stm_stasc_devices[] = {
	/* 7141: Checked except pacing */
	STASC_DEVICE(0xfd030000, ILC_IRQ(76), 11, 15,
		     -1, -1, -1, -1, -1,
		STPIO_OUT, STPIO_IN, STPIO_IN, STPIO_OUT),
	STASC_DEVICE(0xfd031000, ILC_IRQ(77), 12, 16,
		     -1, -1, -1, -1, -1,
		STPIO_IN, STPIO_IN, STPIO_IN, STPIO_IN),
	STASC_DEVICE(0xfd032000, ILC_IRQ(78), 13, 17,
		     -1, -1, -1, -1, -1,
		STPIO_IN, STPIO_IN, STPIO_IN, STPIO_IN),
};

/*
 * Note these three variables are global, and shared with the stasc driver
 * for console bring up prior to platform initialisation.
 */

/* the serial console device */
int stasc_console_device __initdata;

/* Platform devices to register */
struct platform_device *stasc_configured_devices[ARRAY_SIZE(stm_stasc_devices)]
	__initdata;
unsigned int stasc_configured_devices_count __initdata = 0;

/* Configure the ASC's for this board.
 * This has to be called before console_init().
 */
void __init stx7141_configure_asc(const int *ascs, int num_ascs, int console)
{
	int i;

	for (i = 0; i < num_ascs; i++) {
		int port;
		unsigned char flags;
		struct platform_device *pdev;
		struct stasc_uart_data *uart_data;
		struct sysconf_field *sc;
		int pio_port = -1;

		port = ascs[i] & 0xff;
		flags = ascs[i] >> 8;
		pdev = &stm_stasc_devices[port];
		uart_data = pdev->dev.platform_data;

		switch (port) {
		case 0:
			/* Mcard routing only */
			BUG();	/* TODO */
			break;
		case 1:
			sc = sysconf_claim(SYS_CFG, 36, 29, 29, "asc");
			if (flags & ASC1_PIO10) {
				pio_port = 10;
				sysconf_write(sc, 0);
			} else {
				BUG();	/* TODO */
			}
			break;
		case 2:
			sc = sysconf_claim(SYS_CFG, 36, 30, 31, "asc");
			if (flags & ASC2_PIO6) {
				pio_port = 6;
				sysconf_write(sc, 3);
			} else {
				pio_port = 1;
				sysconf_write(sc, 0);
			}
			break;
		default:
			BUG();
		}

		uart_data->pio_port = pio_port;

		if (pio_port) {
			/* Tx */
			stx7141_pio_sysconf(pio_port, 0, 3, "asc");
			uart_data->pio_pin[0] = 0;
			/* Rx */
			stx7141_pio_sysconf(pio_port, 1, 3, "asc");
			uart_data->pio_pin[1] = 1;
		}

		if (!(flags & STASC_FLAG_NORTSCTS)) {
			/* CTS */
			stx7141_pio_sysconf(pio_port, 2, 3, "asc");
			uart_data->pio_pin[2] = 2;
			/* RTS */
			stx7141_pio_sysconf(pio_port, 3, 3, "asc");
			uart_data->pio_pin[3] = 3;
		}
		pdev->id = i;
		((struct stasc_uart_data *)(pdev->dev.platform_data))->flags =
			flags;
		stasc_configured_devices[stasc_configured_devices_count] = pdev;
		stasc_configured_devices_count++;
	}

	stasc_console_device = console;
	/* the console will be always a wakeup-able device */
	stasc_configured_devices[console]->dev.power.can_wakeup = 1;
	device_set_wakeup_enable(&stasc_configured_devices[console]->dev, 0x1);
}

/* Add platform device as configured by board specific code */
static int __init stx7141_add_asc(void)
{
	return platform_add_devices(stasc_configured_devices,
				    stasc_configured_devices_count);
}
arch_initcall(stx7141_add_asc);

/* LiRC resources ---------------------------------------------------------- */
static struct lirc_pio lirc_pios[] = {
	[0] = {
		.bank  = 3,
		.pin   = 7,
		.dir   = STPIO_IN,
		.pinof = 0x00 | LIRC_UHF_RX | LIRC_PIO_ON
	},
	[1] = {
		.bank  = 5,
		.pin   = 2,
		.dir   = STPIO_IN,
		.pinof = 0x00 | LIRC_IR_RX | LIRC_PIO_ON //not available
		},
	[2] = {
		.bank  = 5,
		.pin   = 3,
		.dir   = STPIO_ALT_OUT,
		.pinof = 0x00 | LIRC_IR_TX | LIRC_PIO_ON
	},
	[3] = {
		.bank  = 5,
		.pin   = 4,
		.dir   = STPIO_ALT_OUT,
		.pinof = 0x00 | LIRC_IR_TX | LIRC_PIO_ON
	},
};

static struct plat_lirc_data lirc_private_info = {
	/* For the 7141, the clock settings will be calculated by the driver
	 * from the system clock
	 */
	.irbclock	= 0, /* use current_cpu data */
	.irbclkdiv	= 0, /* automatically calculate */
	.irbperiodmult	= 0,
	.irbperioddiv	= 0,
	.irbontimemult	= 0,
	.irbontimediv	= 0,
	.irbrxmaxperiod = 0x5000,
	.sysclkdiv	= 1,
	.rxpolarity	= 1,
	.pio_pin_arr  = lirc_pios,
	.num_pio_pins = ARRAY_SIZE(lirc_pios),
#ifdef CONFIG_PM
	.clk_on_low_power = 1000000,
#endif
};

static struct resource lirc_resource[] = {
	[0] = {
		.start = 0xfd018000,
		.end   = 0xfd018000 + 0xa0,
		.flags = IORESOURCE_MEM
	},
	[1] = {
		.start = ILC_IRQ(81),
		.end   = ILC_IRQ(81),
		.flags = IORESOURCE_IRQ
	},
};

static struct platform_device lirc_device = {
	.name		= "lirc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(lirc_resource),
	.resource	= lirc_resource,
	.dev = {
		   .power.can_wakeup = 1,
		   .platform_data = &lirc_private_info
	}
};

void __init stx7141_configure_lirc(lirc_scd_t *scd)
{
	lirc_private_info.scd_info = scd;
	stx7141_pio_sysconf(5, 2, 1, "lirc");
	stx7141_pio_sysconf(3, 7, 1, "lirc");
	stx7141_pio_sysconf(5, 3, 1, "lirc");
	stx7141_pio_sysconf(5, 4, 1, "lirc");
	platform_device_register(&lirc_device);
}

/* NAND Resources ---------------------------------------------------------- */

static void nand_cmd_ctrl(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct nand_chip *this = mtd->priv;

	if (ctrl & NAND_CTRL_CHANGE) {

		if (ctrl & NAND_CLE)
			this->IO_ADDR_W = (void *)
				((unsigned int)this->IO_ADDR_W |
				 (unsigned int)(1 << 17));
		else
			this->IO_ADDR_W = (void *)
				((unsigned int)this->IO_ADDR_W &
				 ~(unsigned int)(1 << 17));

		if (ctrl & NAND_ALE)
			this->IO_ADDR_W = (void *)
				((unsigned int)this->IO_ADDR_W |
				 (unsigned int)(1 << 18));
		else
			this->IO_ADDR_W = (void *)
				((unsigned int)this->IO_ADDR_W &
				 ~(unsigned int)(1 << 18));
	}

	if (cmd != NAND_CMD_NONE)
		writeb(cmd, this->IO_ADDR_W);
}

static void nand_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;

	/* write buf up to 4-byte boundary */
	while ((unsigned int)buf & 0x3) {
		writeb(*buf++, chip->IO_ADDR_W);
		len--;
	}

	writesl(chip->IO_ADDR_W, buf, len/4);

	/* mop up trailing bytes */
	for (i = (len & ~0x3); i < len; i++)
		writeb(buf[i], chip->IO_ADDR_W);
}

static void nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;

	/* read buf up to 4-byte boundary */
	while ((unsigned int)buf & 0x3) {
		*buf++ = readb(chip->IO_ADDR_R);
		len--;
	}

	readsl(chip->IO_ADDR_R, buf, len/4);

	/* mop up trailing bytes */
	for (i = (len & ~0x3); i < len; i++)
		buf[i] = readb(chip->IO_ADDR_R);
}

static const char *nand_part_probes[] = { "cmdlinepart", NULL };

static struct platform_device nand_flash[] = {
	EMI_NAND_DEVICE(0),
	EMI_NAND_DEVICE(1),
	EMI_NAND_DEVICE(2),
	EMI_NAND_DEVICE(3),
	EMI_NAND_DEVICE(4),
 };


/*
 * stx7141_configure_nand - Configures NAND support for the STx7141
 *
 * Requires generic platform NAND driver (CONFIG_MTD_NAND_PLATFORM).
 * Uses 'gen_nand.x' as ID for specifying MTD partitions on the kernel
 * command line.
 */
void __init stx7141_configure_nand(struct nand_config_data *data)
{
	unsigned int bank_base, bank_end;
	unsigned int emi_bank = data->emi_bank;

	struct platform_nand_data *nand_private_data =
		nand_flash[emi_bank].dev.platform_data;

	bank_base = emi_bank_base(emi_bank) + data->emi_withinbankoffset;
	if (emi_bank == 4)
		bank_end = 0x07ffffff;
	else
		bank_end = emi_bank_base(emi_bank+1) - 1;

	printk(KERN_INFO "Configuring EMI Bank%d for NAND device\n", emi_bank);
	emi_config_nand(data->emi_bank, data->emi_timing_data);

	nand_flash[emi_bank].resource[0].start = bank_base;
	nand_flash[emi_bank].resource[0].end = bank_end;

	nand_private_data->chip.chip_delay = data->chip_delay;
	nand_private_data->chip.partitions = data->mtd_parts;
	nand_private_data->chip.nr_partitions = data->nr_parts;

	platform_device_register(&nand_flash[emi_bank]);
}

/* Early resources (sysconf and PIO) --------------------------------------- */

static struct platform_device sysconf_device = {
	.name		= "sysconf",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfe001000,
			.end	= 0xfe001000 + 0x1df,
			.flags	= IORESOURCE_MEM
		}
	},
	.dev = {
		.platform_data = &(struct plat_sysconf_data) {
			.sys_device_offset = 0,
			.sys_sta_offset = 8,
			.sys_cfg_offset = 0x100,
		}
	}
};

static struct platform_device stpio_devices[] = {
	STPIO_DEVICE(1, 0xfd020000, ILC_IRQ(49)),
	STPIO_DEVICE(2, 0xfd021000, ILC_IRQ(50)),
	STPIO_DEVICE(3, 0xfd022000, ILC_IRQ(51)),
	STPIO_DEVICE(4, 0xfd023000, ILC_IRQ(52)),
	STPIO_DEVICE(5, 0xfd024000, ILC_IRQ(53)),
	STPIO_DEVICE(6, 0xfd025000, ILC_IRQ(54)),
	STPIO_DEVICE(7, 0xfd026000, ILC_IRQ(55)),

	STPIO_DEVICE(8, 0xfe010000, ILC_IRQ(59)),
	STPIO_DEVICE(9, 0xfe011000, ILC_IRQ(60)),
	STPIO_DEVICE(10, 0xfe012000, ILC_IRQ(61)),
	STPIO_DEVICE(11, 0xfe013000, ILC_IRQ(62)),
	STPIO_DEVICE(12, 0xfe014000, ILC_IRQ(63)),
	STPIO_DEVICE(13, 0xfe015000, ILC_IRQ(64)),
	STPIO_DEVICE(14, 0xfe016000, ILC_IRQ(65)),
	STPIO_DEVICE(15, 0xfe017000, ILC_IRQ(66)),
	STPIO_DEVICE(16, 0xfe018000, ILC_IRQ(67)),
};

/* Initialise devices which are required early in the boot process. */
void __init stx7141_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(&sysconf_device);
	stpio_early_init(stpio_devices, ARRAY_SIZE(stpio_devices),
			 ILC_FIRST_IRQ+ILC_NR_IRQS);

	sc = sysconf_claim(SYS_DEV, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx7141 version %ld.x\n", chip_revision);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}

static void __init pio_late_setup(void)
{
	int i;
	struct platform_device *pdev = stpio_devices;

	for (i = 0; i < ARRAY_SIZE(stpio_devices); i++, pdev++)
		platform_device_register(pdev);
}

/* This is the eSTB ILC3 */
static struct platform_device ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd120000,
			.end	= 0xfd120000 + 0x900,
			.flags	= IORESOURCE_MEM
		}
	},
};

/* Pre-arch initialisation ------------------------------------------------- */

static int __init stx7141_postcore_setup(void)
{
	emi_init(0, 0xfe700000);

	return 0;
}
postcore_initcall(stx7141_postcore_setup);

/* Late resources ---------------------------------------------------------- */

static int __init stx7141_subsys_setup(void)
{
	/* we need to do PIO setup before module init, because some
	 * drivers (eg gpio-keys) require that the interrupts
	 * are available. */
	pio_late_setup();

	return 0;
}
subsys_initcall(stx7141_subsys_setup);

static struct platform_device *stx7141_devices[] __initdata = {
	&fdma0_device,
	&fdma1_device,
	&fdma_xbar_device,
	&sysconf_device,
	&ilc3_device,
	&hwrandom_rng_device,
	&devrandom_rng_device,
};

#include "./platform-pm-stx7141.c"

static int __init stx7141_devices_setup(void)
{
	platform_add_pm_devices(stx7141_pm_devices,
		ARRAY_SIZE(stx7141_pm_devices));

	return platform_add_devices(stx7141_devices,
				    ARRAY_SIZE(stx7141_devices));
}
device_initcall(stx7141_devices_setup);

/* Interrupt initialisation ------------------------------------------------ */

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2_TUNI, TMU2_TICPI,
	WDT,
	HUDI,

	/* interrupt groups */
	TMU2, RTC,
};

static struct intc_vect vectors[] = {
	INTC_VECT(TMU0, 0x400), INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2_TUNI, 0x440), INTC_VECT(TMU2_TICPI, 0x460),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),
};

static struct intc_group groups[] = {
	INTC_GROUP(TMU2, TMU2_TUNI, TMU2_TICPI),
};

static struct intc_prio_reg prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,       } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0,    0,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },
};

static DECLARE_INTC_DESC(intc_desc, "stx7111", vectors, groups,
			 NULL, prio_registers, NULL);

void __init plat_irq_setup(void)
{
	unsigned long intc2_base = (unsigned long)ioremap(0xfe001000, 0x400);

	register_intc_controller(&intc_desc);

	ilc_early_init(&ilc3_device);

	/*
	 * Currently we route all ILC3 interrupts to the 0'th output,
	 * which is connected to INTC2: group 0 interrupt 0.
	 */

	/* Enable the INTC2 */
	writel(7, intc2_base + 0x300);	/* INTPRI00 */
	writel(1, intc2_base + 0x360);	/* INTMSKCLR00 */

	/* Set up the demux function */
	set_irq_chip(evt2irq(0xa00), &dummy_irq_chip);
	set_irq_chained_handler(evt2irq(0xa00), ilc_irq_demux);

	ilc_demux_init();
}
