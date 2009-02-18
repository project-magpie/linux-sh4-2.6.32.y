/*
 * STx7105 Setup
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
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <asm/irq-ilc.h>

static u64 st40_dma_mask = DMA_32BIT_MASK;

static struct {
	unsigned char syscfg;
	unsigned char max_alt;
	unsigned char bits;
} pio_sysconf[] = {
	[0] = { 19, 5, 2},
	[1] = { 20, 4, 2},
	[2] = { 21, 3, 2},
	[3] = { 25, 4, 2},
	[4] = { 34, 4, 2},
	[5] = { 35, 4, 2},
	[6] = { 36, 6, 2},
	[7] = { 37, 5, 2},
	[8] = { 46, 3, 2},
	[9] = { 47, 3, 2},
	[10] = { 0, 1, 0 },
	[11] = { 0 /* 5[29] */ , 2, 0 },
	[12] = { 48, 4, 3},
	[13] = { 49, 4, 3},
	[14] = { 0, 2/5, 0},
	[15] = { 50, 4, 2},
	[16] = { 0, 2/5, 0},
};

static void stx7105_pio_sysconf(int bank, int pin, int alt, const char* name)
{
	int cfg = pio_sysconf[bank].syscfg;
	struct sysconf_field *sc[3];

	sc[0] = sysconf_claim(SYS_CFG, cfg, pin, pin, name);
	sc[1] = sysconf_claim(SYS_CFG, cfg, pin+8, pin+8, name);
	sc[2] = sysconf_claim(SYS_CFG, cfg, pin+16, pin+16, name);
	sysconf_write(sc[0], (alt-1) & 1);
	sysconf_write(sc[1], ((alt-1) >> 1) & 1);
	sysconf_write(sc[2], ((alt-1) >> 2) & 1);
}

/* Most of the PIO pins are sane in that alternative 1 is 0 alternative2 is 1,
 * etc. However, there are some where the alternatives are not fully specified
 * so alternative 3 is 0 etc. This function just wraps the above to add one
 * so you can pass in what the datasheet says.
 */
static inline void stx7105_pio_sysconf_alt(int bank, int pin, int alt_val, const char *name)
{
	stx7105_pio_sysconf(bank, pin, alt_val + 1, name);
}




/* USB resources ----------------------------------------------------------- */

#define UHOST2C_BASE(N)			(0xfe100000 + ((N)*0x00900000))
#define AHB2STBUS_WRAPPER_GLUE_BASE(N)  (UHOST2C_BASE(N))
#define AHB2STBUS_OHCI_BASE(N)          (UHOST2C_BASE(N) + 0x000ffc00)
#define AHB2STBUS_EHCI_BASE(N)          (UHOST2C_BASE(N) + 0x000ffe00)
#define AHB2STBUS_PROTOCOL_BASE(N)      (UHOST2C_BASE(N) + 0x000fff00)

static struct plat_usb_data usb_wrapper[2] = {
	USB_WRAPPER(0, AHB2STBUS_WRAPPER_GLUE_BASE(0),
		    AHB2STBUS_PROTOCOL_BASE(0),
		    USB_FLAGS_STRAP_8BIT	|
		    USB_FLAGS_STBUS_CONFIG_THRESHOLD128),
	USB_WRAPPER(1, AHB2STBUS_WRAPPER_GLUE_BASE(1),
		    AHB2STBUS_PROTOCOL_BASE(1),
		    USB_FLAGS_STRAP_8BIT	|
		    USB_FLAGS_STBUS_CONFIG_THRESHOLD128),
};

static struct platform_device usb_device[2] = {
	USB_DEVICE(0, AHB2STBUS_EHCI_BASE(0), evt2irq(0x1720),
		      AHB2STBUS_OHCI_BASE(0), evt2irq(0x1700),
		      &usb_wrapper[0]),
	USB_DEVICE(1, AHB2STBUS_EHCI_BASE(1), evt2irq(0x13e0),
		      AHB2STBUS_OHCI_BASE(1), evt2irq(0x13c0),
		      &usb_wrapper[1]),
};
/**
 * stx7105_configure_usb - Configure a USB port
 * @port: USB port number (0 or 1)
 * @init_data: details of how to configure port
 *
 * Configure a USB port. Pins:
 *		  PORT 0	PORT 1
 *		+-------------------------
 * OC	normal	|  4[4]		 4[6]
 *	alt	| 12[5]		14[6]
 * PWR	normal	|  4[5]		 4[7]
 *	alt	| 12[6]		14[7]
 */
void __init stx7105_configure_usb(int port, struct usb_init_data *data)
{
	static struct stpio_pin *pin;
	struct sysconf_field *sc;

	/* USB PHY clock from alternate pad? */
	/* sysconf_claim(SYS_CFG, 40, 2,2, "USB"); */

#ifndef CONFIG_PM
	/* Power up USB PHY */
	sc = sysconf_claim(SYS_CFG, 32, 6+port,6+port, "USB");
	sysconf_write(sc, 0);

	/* Power up USB host */
	sc = sysconf_claim(SYS_CFG, 32, 4+port,4+port, "USB");
	sysconf_write(sc, 0);
#endif

	/* USB overcurrent enable */
	sc = sysconf_claim(SYS_CFG, 4, 11+port,11+port, "USBOC");
	sysconf_write(sc, data->oc_en);

	if (data->oc_en) {
		const struct {
			int portno;
			int pinno;
			int alt;
		} oc_pio[2][2] = {
			{ { 4, 4, 4 }, { 12, 5, 3 } },
			{ { 4, 6, 4 }, { 14, 6, 2 } }
		};

		int oc_portno = oc_pio[port][data->oc_pinsel].portno;
		int oc_pinno  = oc_pio[port][data->oc_pinsel].pinno;
		int oc_alt = oc_pio[port][data->oc_pinsel].alt;

		sc = sysconf_claim(SYS_CFG, 4, 5+port,5+port, "USBOC");
		sysconf_write(sc, data->oc_pinsel);

		stx7105_pio_sysconf(oc_portno, oc_pinno, oc_alt, "USBOC");
		pin = stpio_request_pin(oc_portno, oc_pinno, "USBOC", STPIO_IN);

		sc = sysconf_claim(SYS_CFG, 4, 3+port,3+port, "USBOC");
		sysconf_write(sc, data->oc_actlow);
	}

	if (data->pwr_en) {
		const struct {
			int portno;
			int pinno;
			int alt;
		} pwr_pio[2][2] = {
			{ { 4, 5, 4 }, { 12, 6, 3 } },
			{ { 4, 7, 4 }, { 14, 7, 2 } }
		};

		int pwr_portno = pwr_pio[port][data->pwr_pinsel].portno;
		int pwr_pinno  = pwr_pio[port][data->pwr_pinsel].pinno;
		int pwr_alt = pwr_pio[port][data->pwr_pinsel].alt;

		stx7105_pio_sysconf(pwr_portno, pwr_pinno, pwr_alt, "USBPWR");
		pin = stpio_request_pin(pwr_portno, pwr_pinno, "USBPWR", STPIO_ALT_OUT);
	}

	platform_device_register(&usb_device[port]);

}

/* FDMA resources ---------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

#include <linux/stm/7200_cut1_fdma2_firmware.h>

static struct fdma_regs stx7105_fdma_regs = {
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
	.fdma_dmem_region = STX7105_DMEM_OFFSET,
	.fdma_imem_region = STX7105_IMEM_OFFSET,
};

static struct fdma_platform_device_data stx7105_fdma0_plat_data = {
	.registers_ptr = &stx7105_fdma_regs,
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


static struct fdma_platform_device_data stx7105_fdma1_plat_data = {
	.registers_ptr = &stx7105_fdma_regs,
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

#define stx7105_fdma0_plat_data_addr &stx7105_fdma0_plat_data
#define stx7105_fdma1_plat_data_addr &stx7105_fdma1_plat_data
#else
#define stx7105_fdma0_plat_data_addr NULL
#define stx7105_fdma1_plat_data_addr NULL
#endif /* CONFIG_STM_DMA */

static struct platform_device fdma0_device = {
	.name		= "stmfdma",
	.id		= 0,
	.num_resources	= 2,
	.resource = (struct resource[2]) {
		[0] = {
			.start = STX7105_FDMA0_BASE,
			.end   = STX7105_FDMA0_BASE + 0xffff,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = LINUX_FDMA0_STX7105_IRQ_VECT,
			.end   = LINUX_FDMA0_STX7105_IRQ_VECT,
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = stx7105_fdma0_plat_data_addr,
	},
};

static struct platform_device fdma1_device = {
	.name		= "stmfdma",
	.id		= 1,
	.resource = (struct resource[2]) {
		[0] = {
			.start = STX7105_FDMA1_BASE,
			.end   = STX7105_FDMA1_BASE + 0xffff,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = LINUX_FDMA1_STX7105_IRQ_VECT,
			.end   = LINUX_FDMA1_STX7105_IRQ_VECT,
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = stx7105_fdma1_plat_data_addr,
	},
};

static struct platform_device fdma_xbar_device = {
	.name		= "fdma-xbar",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[1]) {
		{
			.start	= STX7105_XBAR_BASE,
			.end	= STX7105_XBAR_BASE+(4*1024)-1,
			.flags	= IORESOURCE_MEM,
		},
	},
};

/* SSC resources ----------------------------------------------------------- */

static char i2c_st[] = "i2c_st";
static char spi_st[] = "spi_st_ssc";

static struct platform_device stssc_devices[] = {
	STSSC_DEVICE(0xfd040000, evt2irq(0x10e0), 2, 2, 3, 4),
	STSSC_DEVICE(0xfd041000, evt2irq(0x10c0), 2, 5, 6, 7),
	STSSC_DEVICE(0xfd042000, evt2irq(0x10a0), 0xff, 0xff, 0xff, 0xff),
	STSSC_DEVICE(0xfd043000, evt2irq(0x1080), 0xff, 0xff, 0xff, 0xff),
};

void __init stx7105_configure_ssc(struct plat_ssc_data *data)
{
	int num_i2c=0;
	int num_spi=0;
	int i;
	int capability = data->capability;
	int routing = data->routing;
	int pin;
	struct sysconf_field* sc;

	const struct {
		unsigned char port, pin, alt;
	} ssc_pios[2][3][4] = { {
		/* SSC2 */
		{ { 3, 4, 3 }, {  3, 4, 3 }, { 12, 0, 3 }, { 13, 4, 2 } },
		{ { 3, 5, 3 }, { 12, 1, 3 }, { 13, 5, 2 }, { 13, 5, 2 } },
		{ { 2, 0, 3 }, {  3, 5, 3 }, { 12, 1, 3 }, { 13, 5, 2 } }
	}, {
		/* SSC3 */
		{ { 3, 6, 3 }, { 13, 2, 3 }, { 13, 6, 2 }, { 13, 6, 2 } },
		{ { 3, 7, 3 }, { 13, 3, 3 }, { 13, 7, 2 }, { 13, 7, 2 } },
		{ { 2, 1, 3 }, {  3, 7, 3 }, { 13, 3, 3 }, { 13, 7, 2 } }
	} };

	for (i=0; i < ARRAY_SIZE(stssc_devices);
	     i++, capability >>= SSC_BITS_SIZE, routing >>= 6) {
		struct ssc_pio_t *ssc_pio = stssc_devices[i].dev.platform_data;

		if(capability & SSC_UNCONFIGURED)
			continue;

		if (capability & SSC_I2C_CLK_UNIDIR)
			ssc_pio->clk_unidir = 1;

		switch (i) {
		case 0:
		case 1:
			/* These have fixed routing */
			for (pin = 0; pin < 3; pin++) {
				int portno = ssc_pio->pio[pin].pio_port;
				int pinno  = ssc_pio->pio[pin].pio_pin;

				if ((pin==2) && !(capability & SSC_SPI_CAPABILITY))
					continue;

				stx7105_pio_sysconf(portno, pinno, 3, "ssc");
			}

			sc = sysconf_claim(SYS_CFG, 16, 3*i, 3*i, "ssc");
			sysconf_write(sc,
				      (capability & SSC_SPI_CAPABILITY) ? 1 : 0);
			break;
		case 2:
		case 3:
			/* Complex routing */
			for (pin = 0; pin < 3; pin++) {
				int bit = ((i==2) ? 11 : 18) - (pin * 2);
				int r = (routing >> (pin*2)) & 3;
				int portno = ssc_pios[i-2][pin][r].port;
				int pinno  = ssc_pios[i-2][pin][r].pin;
				int alt    = ssc_pios[i-2][pin][r].alt;

				sc = sysconf_claim(SYS_CFG, 16,
						   bit, bit+1, "ssc");
				sysconf_write(sc, r);
				ssc_pio->pio[pin].pio_port = portno;
				ssc_pio->pio[pin].pio_pin  = pinno;

				if ((pin==2) && !(capability & SSC_SPI_CAPABILITY))
					continue;

				stx7105_pio_sysconf(portno, pinno, alt, "ssc");
			}
			break;
		}

		if(capability & SSC_SPI_CAPABILITY){
			stssc_devices[i].name = spi_st;
			stssc_devices[i].id = num_spi++;
			ssc_pio->chipselect = data->spi_chipselects[i];
		} else {
			stssc_devices[i].name = i2c_st;
			stssc_devices[i].id = num_i2c++;
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

/* Ok to have same private data for both controllers */
static struct plat_sata_data sata_private_info = {
	.phy_init = 0,
	.pc_glue_logic_init = 0,
	.only_32bit = 0,
};

static struct platform_device sata_device[1] = {
	SATA_DEVICE(0, 0xfe209000, evt2irq(0xb00), evt2irq(0xa80),
		    &sata_private_info),
};

void __init stx7105_configure_sata(void)
{
	struct sysconf_field *sc;

	/* Power up SATA phy */
	sc = sysconf_claim(SYS_CFG, 32, 9, 9, "SATA");
	sysconf_write(sc, 0);

	if ((cpu_data->cut_major >= 3)) {
		stm_sata_miphy_init();
	}

	/* Power up SATA host */
	sc = sysconf_claim(SYS_CFG, 32, 11 , 11, "SATA");
	sysconf_write(sc, 0 );

	platform_device_register(sata_device);
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

void __init stx7105_configure_pata(int bank, int pc_mode, int irq)
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

static struct sysconf_field *mac_speed_sc;

static void fix_mac_speed(void* priv, unsigned int speed)
{
	sysconf_write(mac_speed_sc, (speed == SPEED_100) ? 1 : 0);
}

static struct plat_stmmacenet_data stx7105eth_private_data = {
	.bus_id = 0,
	.pbl = 32,
	.has_gmac = 1,
	.fix_mac_speed = fix_mac_speed,
};

static struct platform_device stx7105eth_device = {
        .name           = "stmmaceth",
        .id             = 0,
        .num_resources  = 2,
        .resource       = (struct resource[]) {
        	{
	                .start = 0xfd110000,
        	        .end   = 0xfd117fff,
                	.flags  = IORESOURCE_MEM,
        	},
        	{
			.name   = "macirq",
                	.start  = evt2irq(0x12c0),
                	.end    = evt2irq(0x12c0),
                	.flags  = IORESOURCE_IRQ,
        	},
	},
	.dev = {
		.power.can_wakeup = 1,
		.platform_data = &stx7105eth_private_data,
	}
};

void stx7105_configure_ethernet(int reverse_mii, int rmii_mode, int mode,
				int ext_mdio, int ext_clk, int phy_bus)
{
	static struct stpio_pin *pin;
	struct sysconf_field *sc;

	stx7105eth_private_data.bus_id = phy_bus;

	/* Ethernet ON */
	sc = sysconf_claim(SYS_CFG, 7, 16, 16, "stmmac");
	sysconf_write(sc, 1);

	/* MII M-DIO select: 1: miim_dio from external input, 0: from GMAC */
	sc = sysconf_claim(SYS_CFG, 7, 17, 17, "stmmac");
	sysconf_write(sc, ext_mdio ? 1 : 0);

	/* RMII pin multiplexing: 0: MII interface active, 1: RMII interface */
	/* cut 1: This register wasn't connected, so only MII available */
	sc = sysconf_claim(SYS_CFG, 7, 18, 18, "stmmac");
	sysconf_write(sc, rmii_mode ? 1 : 0);

	/*
	 * PHY EXT CLOCK: 0: provided by STx7105; 1: external
	 * cut 1: sysconf7[19], however this wasn't connected, so only
	 * input supported.
	 * cut 2: direction now based on PIO direction, so this code removed.
	 */

	/* MAC speed*/
	mac_speed_sc = sysconf_claim(SYS_CFG, 7, 20, 20, "stmmac");

	/* Default GMII/MII selection */
	sc = sysconf_claim(SYS_CFG, 7, 25, 26, "stmmac");
	sysconf_write(sc, mode & 0x3);

	/* MII mode */
	sc = sysconf_claim(SYS_CFG, 7, 27, 27, "stmmac");
	sysconf_write(sc, reverse_mii ? 0 : 1);

	/* Pin configuration... */

	/* MIIRX_DV/MII_EXCRS */
	stx7105_pio_sysconf(7, 4, 1, "eth");
	pin = stpio_request_pin(7, 4, "eth", STPIO_IN);

	/* MIIRX_ER/MII_EXCOL */
	stx7105_pio_sysconf(7, 5, 1, "eth");
	pin = stpio_request_pin(7, 5, "eth", STPIO_IN);

	/* MIITXD[0] */
	stx7105_pio_sysconf(7, 6, 1, "eth");
	pin = stpio_request_pin(7, 6, "eth", STPIO_ALT_OUT);

	/* MIITXD[1] */
	stx7105_pio_sysconf(7, 7, 1, "eth");
	pin = stpio_request_pin(7, 7, "eth", STPIO_ALT_OUT);

	/* MIITXD[2] */
	stx7105_pio_sysconf(8, 0, 1, "eth");
	pin = stpio_request_pin(8, 0, "eth", STPIO_ALT_OUT);

	/* MIITXD[3] */
	stx7105_pio_sysconf(8, 1, 1, "eth");
	pin = stpio_request_pin(8, 1, "eth", STPIO_ALT_OUT);

	/* MIITX_EN */
	stx7105_pio_sysconf(8, 2, 1, "eth");
	pin = stpio_request_pin(8, 2, "eth", STPIO_ALT_OUT);

	/* MIIMDIO */
	stx7105_pio_sysconf(8, 3, 1, "eth");
	pin = stpio_request_pin(8, 3, "eth", STPIO_ALT_BIDIR);

	/* MIIMDC */
	stx7105_pio_sysconf(8, 4, 1, "eth");
	pin = stpio_request_pin(8, 4, "eth", STPIO_ALT_OUT);

	/* MIIRXCLK */
	stx7105_pio_sysconf(8, 5, 1, "eth");
	pin = stpio_request_pin(8, 5, "eth", STPIO_IN);

	/* MIIRXD[0] */
	stx7105_pio_sysconf(8, 6, 1, "eth");
	pin = stpio_request_pin(8, 6, "eth", STPIO_IN);

	/* MIIRXD[1] */
	stx7105_pio_sysconf(8, 7, 1, "eth");
	pin = stpio_request_pin(8, 7, "eth", STPIO_IN);

	/* MIIRXD[2] */
	stx7105_pio_sysconf(9, 0, 1, "eth");
	pin = stpio_request_pin(9, 0, "eth", STPIO_IN);

	/* MIIRXD[3] */
	stx7105_pio_sysconf(9, 1, 1, "eth");
	pin = stpio_request_pin(9, 1, "eth", STPIO_IN);

	/* MIITXCLK */
	stx7105_pio_sysconf(9, 2, 1, "eth");
	pin = stpio_request_pin(9, 2, "eth", STPIO_IN);

	/* MIICOL */
	stx7105_pio_sysconf(9, 3, 1, "eth");
	pin = stpio_request_pin(9, 3, "eth", STPIO_IN);

	/* MIICRS */
	stx7105_pio_sysconf(9, 4, 1, "eth");
	pin = stpio_request_pin(9, 4, "eth", STPIO_IN);

	/* MIIPHYCLK */
	/* Not implemented in cut 1 (DDTS GNBvd69906) - clock never output */
	/* In cut 2 PIO direction used to control input or output. */
	stx7105_pio_sysconf(9, 5, 1, "eth");
	pin = stpio_request_pin(9, 5, "eth",
				ext_clk ? STPIO_IN : STPIO_ALT_OUT);

	/* MIIMDINT */
	stx7105_pio_sysconf(9, 6, 1, "eth");
	pin = stpio_request_pin(9, 6, "eth", STPIO_IN);

	platform_device_register(&stx7105eth_device);
}

/* Audio output ------------------------------------------------------------ */

void stx7105_configure_audio_pins(int pcmout, int spdif, int pcmin)
{
	/* Claim PIO pins as digital audio outputs, depending
	 * on how many DATA outputs are to be used... */

	if (pcmout > 0) {
		stx7105_pio_sysconf(10, 3, 1, "AUD0PCMOUT_CLKIN/OUT");
		stpio_request_pin(10, 3, "AUD0PCMOUT_CLKIN/OUT",
				STPIO_ALT_OUT);
		stx7105_pio_sysconf(10, 4, 1, "AUD0PCMOUT_LRCLK");
		stpio_request_pin(10, 4, "AUD0PCMOUT_LRCLK", STPIO_ALT_OUT);
		stx7105_pio_sysconf(10, 5, 1, "AUD0PCMOUT_SCLK");
		stpio_request_pin(10, 5, "AUD0PCMOUT_SCLK", STPIO_ALT_OUT);
		stx7105_pio_sysconf(10, 0, 1, "AUD0PCMOUT_DATA0");
		stpio_request_pin(10, 0, "AUD0PCMOUT_DATA0", STPIO_ALT_OUT);
	}
	if (pcmout > 1) {
		stx7105_pio_sysconf(10, 1, 1, "AUD0PCMOUT_DATA1");
		stpio_request_pin(10, 1, "AUD0PCMOUT_DATA1", STPIO_ALT_OUT);
	}
	if (pcmout > 2) {
		stx7105_pio_sysconf(10, 2, 1, "AUD0PCMOUT_DATA2");
		stpio_request_pin(10, 2, "AUD0PCMOUT_DATA2", STPIO_ALT_OUT);
	}
	if (pcmout > 3)
		BUG();

	/* Claim PIO pin as SPDIF output... */

	if (spdif > 0) {
		stx7105_pio_sysconf(10, 6, 1, "AUDSPDIFOUT");
		stpio_request_pin(10, 6, "AUDSPDIFOUT", STPIO_ALT_OUT);
	}
	if (spdif > 1)
		BUG();

	/* Claim PIO pins as digital audio inputs... */

	if (pcmin > 0) {
		stx7105_pio_sysconf(10, 7, 1, "AUD0PCMIN_DATA");
		stpio_request_pin(10, 7, "AUD0PCMIN_DATA", STPIO_ALT_BIDIR);
		stx7105_pio_sysconf(11, 0, 1, "AUD0PCMIN_SCLK");
		stpio_request_pin(11, 0, "AUD0PCMIN_SCLK", STPIO_ALT_BIDIR);
		stx7105_pio_sysconf(11, 1, 1, "AUD0PCMIN_LRCLK");
		stpio_request_pin(11, 1, "AUD0PCMIN_LRCLK", STPIO_ALT_BIDIR);
	}
	if (pcmin > 1)
		BUG();
}

/* PWM resources ----------------------------------------------------------- */

static struct resource stm_pwm_resource[]= {
	[0] = {
		.start	= 0xfd010000,
		.end	= 0xfd010000 + 0x67,
		.flags	= IORESOURCE_MEM
	},
	[1] = {
		.start	= evt2irq(0x11c0),
		.end	= evt2irq(0x11c0),
		.flags	= IORESOURCE_IRQ
	}
};

static struct platform_device stm_pwm_device = {
	.name		= "stm-pwm",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stm_pwm_resource),
	.resource	= stm_pwm_resource,
};

void stx7105_configure_pwm(struct plat_stm_pwm_data *data)
{
	int pwm;
	const struct {
		unsigned char port, pin, alt;
	} pwm_pios[2][2] = {
		{ { 4, 4, 3 }, { 13, 0, 3 } }, 	/* PWM0 */
		{ { 4, 5, 3 }, { 13, 1, 3 } },	/* PWM1 */
	};

	stm_pwm_device.dev.platform_data = data;

	for (pwm = 0; pwm < 2; pwm++) {
		if (data->flags & (1<<pwm)) {
			int r = (data->routing >> pwm) & 1;
			int port = pwm_pios[pwm][r].port;
			int pin  = pwm_pios[pwm][r].pin;
			int alt  = pwm_pios[pwm][r].alt;

			stx7105_pio_sysconf(port, pin, alt, "pwm");
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
	/* 7105: Checked except pacing */
	STASC_DEVICE(0xfd030000, evt2irq(0x1160), 11, 15, 0, 0, 1, 4, 3,
		STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT), /* oe pin: 2 */
	STASC_DEVICE(0xfd031000, evt2irq(0x1140), 12, 16, 1, 0, 1, 4, 3,
		STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
	STASC_DEVICE(0xfd032000, evt2irq(0x1120), 13, 17, 4, 0, 1, 2, 3,
		STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT), /* or 12, 0, 1, 2, 3 */
	STASC_DEVICE(0xfd033000, evt2irq(0x1100), 14, 18, 5, 0, 1, 3, 2,
		STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
};

/*
 * Note these three variables are global, and shared with the stasc driver
 * for console bring up prior to platform initialisation.
 */

/* the serial console device */
int stasc_console_device __initdata;

/* Platform devices to register */
struct platform_device *stasc_configured_devices[ARRAY_SIZE(stm_stasc_devices)] __initdata;
unsigned int stasc_configured_devices_count __initdata = 0;

/* Configure the ASC's for this board.
 * This has to be called before console_init().
 */
void __init stx7105_configure_asc(const int *ascs, int num_ascs, int console)
{
	int i;
	static const int alt_conf[4] = { 4, 4, 3, 2 };

	for (i=0; i<num_ascs; i++) {
		int port;
		unsigned char flags;
		struct platform_device *pdev;
		struct stasc_uart_data *uart_data;

		port = ascs[i] & 0xff;
		flags = ascs[i] >> 8;
		pdev = &stm_stasc_devices[port];
		uart_data = pdev->dev.platform_data;

		/* Tx */
		stx7105_pio_sysconf(uart_data->pio_port, uart_data->pio_pin[0],
				    alt_conf[port], "asc");
		/* Rx */
		stx7105_pio_sysconf(uart_data->pio_port, uart_data->pio_pin[1],
				    alt_conf[port], "asc");

		if (! (flags & STASC_FLAG_NORTSCTS)) {
			/* CTS */
			stx7105_pio_sysconf(uart_data->pio_port, uart_data->pio_pin[2],
					    alt_conf[port], "asc");
			/* RTS */
			stx7105_pio_sysconf(uart_data->pio_port, uart_data->pio_pin[3],
					    alt_conf[port], "asc");
		}
		pdev->id = i;
		((struct stasc_uart_data*)(pdev->dev.platform_data))->flags = flags;
		stasc_configured_devices[stasc_configured_devices_count++] = pdev;
	}

	stasc_console_device = console;
	/* the console will be always a wakeup-able device */
	stasc_configured_devices[console]->dev.power.can_wakeup = 1;
	device_set_wakeup_enable(&stasc_configured_devices[console]->dev, 0x1);
}

/* Add platform device as configured by board specific code */
static int __init stx7105_add_asc(void)
{
	return platform_add_devices(stasc_configured_devices,
				    stasc_configured_devices_count);
}
arch_initcall(stx7105_add_asc);

/* LiRC resources ---------------------------------------------------------- */
static struct lirc_pio lirc_pios[] = {
        [0] = {
		.bank = 3,
		.pin  = 0,
		.dir  = STPIO_IN,
		.pinof = 0x00 | LIRC_IR_RX | LIRC_PIO_ON
	},
	[1] = {
		.bank = 3,
		.pin  = 1,
		.dir  = STPIO_IN,
		.pinof = 0x00 | LIRC_UHF_RX | LIRC_PIO_ON
        },
	[2] = {
		.bank = 3,
		.pin  = 2,
		.dir  = STPIO_ALT_OUT,
        .pinof= 0x00 | LIRC_IR_TX | LIRC_PIO_ON
	},
	[3] = {
		.bank = 3,
		.pin  = 3,
		.dir  = STPIO_ALT_OUT,
		.pinof= 0x00 | LIRC_IR_TX | LIRC_PIO_ON
	},
};

static struct plat_lirc_data lirc_private_info = {
	/* For the 7105, the clock settings will be calculated by the driver
	 * from the system clock
	 */
	.irbclock	= 0, /* use current_cpu data */
	.irbclkdiv      = 0, /* automatically calculate */
	.irbperiodmult  = 0,
	.irbperioddiv   = 0,
	.irbontimemult  = 0,
	.irbontimediv   = 0,
	.irbrxmaxperiod = 0x5000,
	.sysclkdiv	= 1,
	.rxpolarity	= 1,
	.pio_pin_arr  = lirc_pios,
	.num_pio_pins = ARRAY_SIZE(lirc_pios),
#ifdef CONFIG_PM
	.clk_on_low_power = 1000000,
#endif
};

static struct resource lirc_resource[]= {
        [0] = {
		.start = 0xfd018000,
		.end   = 0xfd018000 + 0xa0,
	        .flags = IORESOURCE_MEM
	},
	[1] = {
		.start = evt2irq(0x11a0),
		.end   = evt2irq(0x11a0),
	        .flags = IORESOURCE_IRQ
	},
};

static struct platform_device lirc_device = {
	.name           = "lirc",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(lirc_resource),
	.resource       = lirc_resource,
	.dev = {
		   .power.can_wakeup = 1,
	           .platform_data = &lirc_private_info
	}
};

void __init stx7105_configure_lirc(lirc_scd_t *scd)
{
	lirc_private_info.scd_info = scd;
	stx7105_pio_sysconf(3, 0, 3, "lirc");
	stx7105_pio_sysconf(3, 1, 3, "lirc");
	stx7105_pio_sysconf(3, 2, 3, "lirc");
	stx7105_pio_sysconf(3, 3, 3, "lirc");
	platform_device_register(&lirc_device);
}

/* NAND Resources ---------------------------------------------------------- */

static void nand_cmd_ctrl(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct nand_chip *this = mtd->priv;

	if (ctrl & NAND_CTRL_CHANGE) {

		if (ctrl & NAND_CLE) {
			this->IO_ADDR_W = (void *)((unsigned int)this->IO_ADDR_W |
						   (unsigned int)(1 << 17));
		}
		else {
			this->IO_ADDR_W = (void *)((unsigned int)this->IO_ADDR_W &
						   ~(unsigned int)(1 << 17));
		}

		if (ctrl & NAND_ALE) {
			this->IO_ADDR_W = (void *)((unsigned int)this->IO_ADDR_W |
						   (unsigned int)(1 << 18));
		}
		else {
			this->IO_ADDR_W = (void *)((unsigned int)this->IO_ADDR_W &
						   ~(unsigned int)(1 << 18));
		}
	}

	if (cmd != NAND_CMD_NONE) {
		writeb(cmd, this->IO_ADDR_W);
	}
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
	for (i = (len & ~0x3); i < len; i++) {
		writeb(buf[i], chip->IO_ADDR_W);
	}
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
	for (i = (len & ~0x3); i < len; i++) {
		buf[i] = readb(chip->IO_ADDR_R);
	}
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
 * stx7105_configure_nand - Configures NAND support for the STx7105
 *
 * Requires generic platform NAND driver (CONFIG_MTD_NAND_PLATFORM).
 * Uses 'gen_nand.x' as ID for specifying MTD partitions on the kernel
 * command line.
 */
void __init stx7105_configure_nand(struct nand_config_data *data)
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

	printk("Configuring EMI Bank%d for NAND device\n", emi_bank);
	emi_config_nand(data->emi_bank, data->emi_timing_data);

	nand_flash[emi_bank].resource[0].start = bank_base;
	nand_flash[emi_bank].resource[0].end = bank_end;

	nand_private_data->chip.chip_delay = data->chip_delay;
	nand_private_data->chip.partitions = data->mtd_parts;
	nand_private_data->chip.nr_partitions = data->nr_parts;

	platform_device_register(&nand_flash[emi_bank]);
}

/*
 * PCI Bus initialisation
 *
 */


/*
 * This function assumes you are using the dedicated pins. Production boards will
 * more likely use the external interrupt pins and save the PIOs
 */

int stx7105_pcibios_map_platform_irq(struct pci_config_data *pci_config, u8 pin)
{
	int irq;
	int pin_type;

	if((pin > 4) || (pin < 1)) return -1;

	pin_type = pci_config->pci_irq[pin - 1];

	switch(pin_type) {
		case PCI_PIN_DEFAULT:
		case PCI_PIN_ALTERNATIVE:
			irq = ILC_EXT_IRQ(pin + 25);
			break;
		case PCI_PIN_UNUSED:
			irq = -1; /* Not used */
			break;
		default:
			irq = pin_type; /* Take whatever interrupt you are told */
			break;
	}

	return irq;
}

#ifdef CONFIG_32BIT
#define PCI_WINDOW_START 0xc0000000
#define PCI_WINDOW_SIZE  0x20000000 /* 512 Megs */
#else
#define PCI_WINDOW_START 0x08000000
#define PCI_WINDOW_SIZE  0x04000000 /* 64 Megs */
#endif

/* Various controls bits in sysconfig 5 */
/* Relative to the start of the PCI block, so they
 * can be plugged into sysconf(read/write) calls
 */

#define PCI_DEVICE_NOT_HOST_ENABLE 	(1<<13)
#define PCI_CLOCK_MASTER_NOT_SLAVE 	(1<<12)
#define PCI_INT0_SRC_SEL		(1<<11)
#define PCI_LOCK_IN_SEL			(1<<9)
#define PCI_SYS_ERROR_ENABLE		(1<<8)
#define PCI_RESETN_ENABLE		(1<<7)
#define PCI_INT_TO_HOST_ENABLE		(1<<6)
#define PCI_INT_FROM_DEVICE(n)		(1 << (5 - (n)))
#define PCI_LOCK_IN_ENABLE		(1<<1)
#define PCI_PME_IN_ENABLE		(1<<0)


static struct platform_device pci_device =
	PCI_DEVICE(0xfe400000, 0xfe560000, PCI_WINDOW_START, PCI_WINDOW_SIZE, evt2irq(0x1280), ILC_IRQ(25));

void __init stx7105_configure_pci(struct pci_config_data *pci_conf)
{
#ifndef CONFIG_PCI
	return;
#else
	int i;
	struct sysconf_field *sc;
	struct stpio_pin *pin;
	static const char *int_name[] = {"PCI INT A","PCI INT B","PCI INT C","PCI INT D"};
	static const char *req_name[] = {"PCI REQ 0 ","PCI REQ 1","PCI REQ 2","PCI REQ 3"};
	static const char *gnt_name[] = {"PCI GNT 0 ","PCI GNT 1","PCI GNT 2","PCI GNT 3"};
	int use_alt_for_int0;
	int sys5_int_enables = 0;

	/* Fill in the default values for the 7105 */
	if(!pci_conf->ad_override_default) {
		pci_conf->ad_threshold = 5;pci_conf->ad_read_ahead = 1;
		pci_conf->ad_chunks_in_msg = 0; pci_conf->ad_pcks_in_chunk = 0;
		pci_conf->ad_trigger_mode = 1; pci_conf->ad_max_opcode = 5;
		pci_conf->ad_posted = 1;
	}

	/* Copy over platform specific data to driver */
	pci_device.dev.platform_data = pci_conf;

	/* Claim and power up the PCI cell */
	sc = sysconf_claim(SYS_CFG, 32, 2, 2, "PCI Power");
	sysconf_write(sc, 0); // We will need to stash this somewhere for power management.

	sc = sysconf_claim(SYS_STA, 15, 2, 2, "PCI Power status");
	while(sysconf_read(sc)); // Loop until powered up

	/* Claim and set pads into PCI mode */
	sc = sysconf_claim(SYS_CFG, 31, 20, 20, "PCI");
	sysconf_write(sc, 1);

	/* SERR is only an output on Cut2, designed for device mode. So no point in enabling it.
	 * LOCK is totally pointless, the SOCs do not support any form of coherency
	 */
	sc = sysconf_claim(SYS_CFG, 5, 16, 29, "PCI Config");
	use_alt_for_int0 = (pci_conf->pci_irq[0] == PCI_PIN_ALTERNATIVE);
	sysconf_write(sc, PCI_CLOCK_MASTER_NOT_SLAVE | ( (use_alt_for_int0) ? PCI_INT0_SRC_SEL : 0 ) );

	if(use_alt_for_int0 ) {
		set_irq_type(ILC_EXT_IRQ(26), IRQ_TYPE_LEVEL_LOW);
		pin = stpio_request_pin(15, 3, int_name[0], STPIO_IN);
		if(!pin) printk(KERN_ERR "Unable to configure PIO for %s\n",int_name[0]);
		sys5_int_enables |= PCI_INT_FROM_DEVICE(0);
	}

	for(i = 0; i < 4; i++) {
                if(pci_conf->pci_irq[i] == PCI_PIN_DEFAULT) {
			set_irq_type(ILC_EXT_IRQ(26 + i), IRQ_TYPE_LEVEL_LOW);
			pin = stpio_request_pin(6, i, int_name[i], STPIO_IN);
			if(!pin) printk(KERN_ERR "Unable to configure PIO for %s\n",int_name[i]);
			/* Set the alternate function correctly in sysconfig */
			sys5_int_enables |= PCI_INT_FROM_DEVICE(i);
                }
        }

	/* Set the approprate enabled interrupts */
	sysconf_write(sc, sysconf_read(sc) | sys5_int_enables);

	/* REQ/GNT 0 are dedicated pins, so we start  from 1 */
	for(i = 1; i < 4; i++ ) {
		if(pci_conf->req_gnt[i] == PCI_PIN_DEFAULT) {
			pin = stpio_request_pin(6, 4 + i, req_name[i], STPIO_IN);
			if(!pin) printk(KERN_ERR "Unable to configure PIO for %s\n",req_name[i]);
			pin = stpio_request_pin(7, i, gnt_name[i], STPIO_ALT_OUT);
			if(!pin) printk(KERN_ERR "Unable to configure PIO for %s\n",gnt_name[i]);
			stx7105_pio_sysconf_alt(7, i, 0x11, gnt_name[i]);
		}
	}

	/* Enable the SERR interrupt if wired up */
	if(pci_conf->serr_irq == PCI_PIN_DEFAULT ) {
		/* Use the default */
		pin = stpio_request_pin(6, 4, pci_device.resource[5].name, STPIO_IN);
	} else {
		pci_device.resource[5].start = 	pci_device.resource[5].end = pci_conf->serr_irq;
	}

	platform_device_register(&pci_device);

#endif /* CONFIG_PCI */
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
	STPIO_DEVICE(0, 0xfd020000, evt2irq(0xc00)),
	STPIO_DEVICE(1, 0xfd021000, evt2irq(0xc80)),
	STPIO_DEVICE(2, 0xfd022000, evt2irq(0xd00)),
	STPIO_DEVICE(3, 0xfd023000, evt2irq(0x1060)),
	STPIO_DEVICE(4, 0xfd024000, evt2irq(0x1040)),
	STPIO_DEVICE(5, 0xfd025000, evt2irq(0x1020)),
	STPIO_DEVICE(6, 0xfd026000, evt2irq(0x1000)),

	STPIO_DEVICE(7, 0xfe010000, -1),
	STPIO_DEVICE(8, 0xfe011000, -1),
	STPIO_DEVICE(9, 0xfe012000, -1),
	STPIO_DEVICE(10, 0xfe013000, -1),
	STPIO_DEVICE(11, 0xfe014000, -1),
	STPIO_DEVICE(12, 0xfe015000, -1),
	STPIO_DEVICE(13, 0xfe016000, -1),
	STPIO_DEVICE(14, 0xfe017000, -1),
	STPIO_DEVICE(15, 0xfe018000, -1),
	STPIO_DEVICE(16, 0xfe019000, -1),
};

/* Standalone PIO at fe01 - fe01ffff */
/* Int evt2irq(0xb40)) */

/* Initialise devices which are required early in the boot process. */
void __init stx7105_early_device_init(void)
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
	chip_revision = (devid >> 28) +1;
	boot_cpu_data.cut_major = chip_revision;

	printk("STx7105 version %ld.x\n", chip_revision);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}

static void __init pio_late_setup(void)
{
	int i;
	struct platform_device *pdev = stpio_devices;

	for (i=0; i<ARRAY_SIZE(stpio_devices); i++,pdev++) {
		platform_device_register(pdev);
	}
}

static struct platform_device ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd000000,
			.end	= 0xfd000000 + 0x900,
			.flags	= IORESOURCE_MEM
		}
	},
};

/* Pre-arch initialisation ------------------------------------------------- */

static int __init stx7105_postcore_setup(void)
{
	emi_init(0, 0xfe700000);

	return 0;
}
postcore_initcall(stx7105_postcore_setup);

/* Late resources ---------------------------------------------------------- */

static int __init stx7105_subsys_setup(void)
{
	/* we need to do PIO setup before module init, because some
	 * drivers (eg gpio-keys) require that the interrupts
	 * are available. */
	pio_late_setup();

	return 0;
}
subsys_initcall(stx7105_subsys_setup);

static struct platform_device *stx7105_devices[] __initdata = {
	&fdma0_device,
	&fdma1_device,
	&fdma_xbar_device,
	&sysconf_device,
	&ilc3_device,
	&hwrandom_rng_device,
	&devrandom_rng_device,
};

#include "./platform-pm-stx7105.c"

static int __init stx7105_devices_setup(void)
{
	platform_add_pm_devices(stx7105_pm_devices,
				    ARRAY_SIZE(stx7105_pm_devices));

	return platform_add_devices(stx7105_devices,
				    ARRAY_SIZE(stx7105_devices));
}
device_initcall(stx7105_devices_setup);

/* Interrupt initialisation ------------------------------------------------ */

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2_TUNI, TMU2_TICPI,
	WDT,
	HUDI,

	I2S2SPDIF0, I2S2SPDIF1, I2S2SPDIF2, I2S2SPDIF3,	/* Group 0 */
	SATA_DMAC,
	SATA_HOSTC, DVP, STANDALONE_PIO, AUX_VDP_END_PROC,
	AUX_VDP_FIFO_EMPTY, COMPO_CAP_BF, COMPO_CAP_TF,
	PIO0, PIO1, PIO2,

	PIO6, PIO5, PIO4, PIO3,				/* Group 1 */
	SSC3, SSC2, SSC1, SSC0,				/* Group 2 */
	UART3, UART2, UART1, UART0,			/* Group 3 */
	IRB_WAKEUP, IRB, PWM, MAFE,			/* Group 4 */
	SBAG, BDISP_AQ, DAA, TTXT,			/* Group 5 */
	EMPI_PCI, GMAC_PMT, GMAC, TS_MERGER,		/* Group 6 */
	LX_DELTAMU, LX_AUD, DCXO, PTI1,			/* Group 7 */
	FDMA0, FDMA1, OHCI1, EHCI1,			/* Group 8 */
	PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR,		/* Group 9 */
	TVO_DCS0, NAND, DELMU_PP, DELMU_MBE,		/* Group 10 */
	MAIN_VDP_FIFO_EMPTY, MAIN_VDP_END_PROCESSING,	/* Group 11 */
	MAIN_VTG, AUX_VTG,
	HDMI_CEC_WAKEUP, HDMI_CEC, HDMI, HDCP,		/* Group 12 */
	PTI0, PDES_ESA, PDES, PDES_READ_CW,		/* Group 13 */
	TKDMA_TKD, TKDMA_DMA, CRIPTO_SIGDMA,		/* Group 14 */
	CRIPTO_SIG_CHK,
	OHCI0, EHCI0, TVO_DCS1, BDISP_CQ,		/* Group 15 */
	ICAM3_KTE, ICAM3, KEY_SCANNER, MES,		/* Group 16 */

	/* interrupt groups */
	TMU2, RTC,
	GROUP0_0, GROUP0_1, GROUP0_2,
	GROUP1, GROUP2, GROUP3,
	GROUP4, GROUP5, GROUP6, GROUP7,
	GROUP8, GROUP9, GROUP10, GROUP11,
	GROUP12, GROUP13, GROUP14, GROUP15,
	GROUP16
};

static struct intc_vect vectors[] = {
	INTC_VECT(TMU0, 0x400), INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2_TUNI, 0x440), INTC_VECT(TMU2_TICPI, 0x460),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),

	INTC_VECT(I2S2SPDIF0, 0xa00), INTC_VECT(I2S2SPDIF1, 0xa20),
	INTC_VECT(I2S2SPDIF2, 0xa40), INTC_VECT(I2S2SPDIF3, 0xa60),
	INTC_VECT(SATA_DMAC, 0xa80),
	INTC_VECT(SATA_HOSTC, 0xb00), INTC_VECT(DVP, 0xb20),
	INTC_VECT(STANDALONE_PIO, 0xb40), INTC_VECT(AUX_VDP_END_PROC, 0xb60),
	INTC_VECT(AUX_VDP_FIFO_EMPTY, 0xb80), INTC_VECT(COMPO_CAP_BF, 0xba0),
	INTC_VECT(COMPO_CAP_TF, 0xbc0),
	INTC_VECT(PIO0, 0xc00), INTC_VECT(PIO1, 0xc80),
	INTC_VECT(PIO2, 0xd00),

	INTC_VECT(PIO6, 0x1000), INTC_VECT(PIO5, 0x1020),
	INTC_VECT(PIO4, 0x1040), INTC_VECT(PIO3, 0x1060),
	INTC_VECT(SSC3, 0x1080), INTC_VECT(SSC2, 0x10a0),
	INTC_VECT(SSC1, 0x10c0), INTC_VECT(SSC0, 0x10e0),
	INTC_VECT(UART3, 0x1100), INTC_VECT(UART2, 0x1120),
	INTC_VECT(UART1, 0x1140), INTC_VECT(UART0, 0x1160),
	INTC_VECT(IRB_WAKEUP, 0x1180), INTC_VECT(IRB, 0x11a0),
	INTC_VECT(PWM, 0x11c0), INTC_VECT(MAFE, 0x11e0),
	INTC_VECT(SBAG, 0x1200), INTC_VECT(BDISP_AQ, 0x1220),
	INTC_VECT(DAA, 0x1240), INTC_VECT(TTXT, 0x1260),
	INTC_VECT(EMPI_PCI, 0x1280), INTC_VECT(GMAC_PMT, 0x12a0),
	INTC_VECT(GMAC, 0x12c0), INTC_VECT(TS_MERGER, 0x12e0),
	INTC_VECT(LX_DELTAMU, 0x1300), INTC_VECT(LX_AUD, 0x1320),
	INTC_VECT(DCXO, 0x1340), INTC_VECT(PTI1, 0x1360),
	INTC_VECT(FDMA0, 0x1380), INTC_VECT(FDMA1, 0x13a0),
	INTC_VECT(OHCI1, 0x13c0), INTC_VECT(EHCI1, 0x13e0),
	INTC_VECT(PCMPLYR0, 0x1400), INTC_VECT(PCMPLYR1, 0x1420),
	INTC_VECT(PCMRDR, 0x1440), INTC_VECT(SPDIFPLYR, 0x1460),
	INTC_VECT(TVO_DCS0, 0x1480), INTC_VECT(NAND, 0x14a0),
	INTC_VECT(DELMU_PP, 0x14c0), INTC_VECT(DELMU_MBE, 0x14e0),
	INTC_VECT(MAIN_VDP_FIFO_EMPTY, 0x1500), INTC_VECT(MAIN_VDP_END_PROCESSING, 0x1520),
	INTC_VECT(MAIN_VTG, 0x1540), INTC_VECT(AUX_VTG, 0x1560),
	INTC_VECT(HDMI_CEC_WAKEUP, 0x1580), INTC_VECT(HDMI_CEC, 0x15a0),
	INTC_VECT(HDMI, 0x15c0), INTC_VECT(HDCP, 0x15e0),
	INTC_VECT(PTI0, 0x1600), INTC_VECT(PDES_ESA, 0x1620),
	INTC_VECT(PDES, 0x1640), INTC_VECT(PDES_READ_CW, 0x1660),
	INTC_VECT(TKDMA_TKD, 0x1680), INTC_VECT(TKDMA_DMA, 0x16a0),
	INTC_VECT(CRIPTO_SIGDMA, 0x16c0), INTC_VECT(CRIPTO_SIG_CHK, 0x16e0),
	INTC_VECT(OHCI0, 0x1700), INTC_VECT(EHCI0, 0x1720),
	INTC_VECT(TVO_DCS1, 0x1740), INTC_VECT(BDISP_CQ, 0x1760),
	INTC_VECT(ICAM3_KTE, 0x1780), INTC_VECT(ICAM3, 0x17a0),
	INTC_VECT(KEY_SCANNER, 0x17c0), INTC_VECT(MES, 0x17e0),
};

static struct intc_group groups[] = {
	INTC_GROUP(TMU2, TMU2_TUNI, TMU2_TICPI),

	INTC_GROUP(GROUP0_0, I2S2SPDIF0, I2S2SPDIF1, I2S2SPDIF2, I2S2SPDIF3),
	INTC_GROUP(GROUP0_1, SATA_DMAC, SATA_HOSTC, DVP, STANDALONE_PIO),
	INTC_GROUP(GROUP0_2, AUX_VDP_END_PROC, AUX_VDP_FIFO_EMPTY,
		   COMPO_CAP_BF, COMPO_CAP_TF),
	/* PIO0, PIO1, PIO2 are not part of any group */

	INTC_GROUP(GROUP1, PIO6, PIO5, PIO4, PIO3),
	INTC_GROUP(GROUP2, SSC3, SSC2, SSC1, SSC0),
	INTC_GROUP(GROUP3, UART3, UART2, UART1, UART0),
	INTC_GROUP(GROUP4, IRB_WAKEUP, IRB, PWM, MAFE),
	INTC_GROUP(GROUP5, SBAG, BDISP_AQ, DAA, TTXT),
	INTC_GROUP(GROUP6, EMPI_PCI, GMAC_PMT, GMAC, TS_MERGER),
	INTC_GROUP(GROUP7, LX_DELTAMU, LX_AUD, DCXO, PTI1),
	INTC_GROUP(GROUP8, FDMA0, FDMA1, OHCI1, EHCI1),
	INTC_GROUP(GROUP9, PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR),
	INTC_GROUP(GROUP10, TVO_DCS0, NAND, DELMU_PP, DELMU_MBE),
	INTC_GROUP(GROUP11, MAIN_VDP_FIFO_EMPTY, MAIN_VDP_END_PROCESSING,
		   MAIN_VTG, AUX_VTG),
	INTC_GROUP(GROUP12, HDMI_CEC_WAKEUP, HDMI_CEC, HDMI, HDCP),
	INTC_GROUP(GROUP13, PTI0, PDES_ESA, PDES, PDES_READ_CW),
	INTC_GROUP(GROUP14, TKDMA_TKD, TKDMA_DMA, CRIPTO_SIGDMA,
		   CRIPTO_SIG_CHK),
	INTC_GROUP(GROUP15, OHCI0, EHCI0, TVO_DCS1, BDISP_CQ),
	INTC_GROUP(GROUP16, ICAM3_KTE, ICAM3, KEY_SCANNER, MES),
};

static struct intc_prio_reg prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,       } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0,    0,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },
						/* 31-28,   27-24,   23-20,   19-16 */
						/* 15-12,    11-8,     7-4,     3-0 */
	{ 0x00000300, 0, 32, 4, /* INTPRI00 */ {       0,       0,    PIO2,    PIO1,
						    PIO0, GROUP0_2, GROUP0_1, GROUP0_0 } },
	{ 0x00000304, 0, 32, 4, /* INTPRI04 */ {  GROUP8,  GROUP7,  GROUP6,  GROUP5,
						  GROUP4,  GROUP3,  GROUP2,  GROUP1 } },
	{ 0x00000308, 0, 32, 4, /* INTPRI08 */ { GROUP16, GROUP15, GROUP14, GROUP13,
						 GROUP12, GROUP11, GROUP10,  GROUP9 } },
};

static struct intc_mask_reg mask_registers[] = {
	{ 0x00000340, 0x00000360, 32, /* INTMSK00 / INTMSKCLR00 */
	  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 31..16 */
	    0, PIO2, PIO1, PIO0,				/* 15..12 */
	    COMPO_CAP_TF, COMPO_CAP_BF, AUX_VDP_FIFO_EMPTY,	/* 11...8 */
		AUX_VDP_END_PROC,
	    STANDALONE_PIO, DVP, SATA_HOSTC, SATA_DMAC,		/*  7...4 */
	    I2S2SPDIF3, I2S2SPDIF2, I2S2SPDIF1, I2S2SPDIF0 } },	/*  3...0 */
	{ 0x00000344, 0x00000364, 32, /* INTMSK04 / INTMSKCLR04 */
	  { EHCI1, OHCI1, FDMA1, FDMA0,				/* 31..28 */
	    PTI1, DCXO, LX_AUD, LX_DELTAMU,			/* 27..24 */
	    TS_MERGER, GMAC, GMAC_PMT, EMPI_PCI,		/* 23..20 */
	    TTXT, DAA, BDISP_AQ, SBAG,				/* 19..16 */
	    MAFE, PWM, IRB, IRB_WAKEUP,				/* 15..12 */
	    UART0, UART1, UART2, UART3,				/* 11...8 */
	    SSC0, SSC1, SSC2, SSC3, 				/*  7...4 */
	    PIO3, PIO4, PIO5,  PIO6  } },			/*  3...0 */
	{ 0x00000348, 0x00000368, 32, /* INTMSK08 / INTMSKCLR08 */
	  { MES, KEY_SCANNER, ICAM3, ICAM3_KTE,			/* 31..28 */
	    BDISP_CQ, TVO_DCS1, EHCI0, OHCI0,			/* 27..24 */
	    CRIPTO_SIG_CHK, CRIPTO_SIGDMA, TKDMA_DMA, TKDMA_TKD,/* 23..20 */
	    PDES_READ_CW, PDES, PDES_ESA, PTI0,			/* 19..16 */
	    HDCP, HDMI, HDMI_CEC, HDMI_CEC_WAKEUP,		/* 15..12 */
	    AUX_VTG, MAIN_VTG, MAIN_VDP_END_PROCESSING,		/* 11...8 */
		 MAIN_VDP_FIFO_EMPTY,
	    DELMU_MBE, DELMU_PP, NAND, TVO_DCS0,		/*  7...4 */
	    SPDIFPLYR, PCMRDR, PCMPLYR1, PCMPLYR0 } }		/*  3...0 */
};

static DECLARE_INTC_DESC(intc_desc, "stx7105", vectors, groups,
			 mask_registers, prio_registers, NULL);

void __init plat_irq_setup(void)
{
	struct sysconf_field *sc;
	unsigned long intc2_base = (unsigned long)ioremap(0xfe001000, 0x400);
	int i;

	ilc_early_init(&ilc3_device);

	for (i=4; i<=6; i++)
		prio_registers[i].set_reg += intc2_base;
	for (i=0; i<=2; i++) {
		mask_registers[i].set_reg += intc2_base;
		mask_registers[i].clr_reg += intc2_base;
	}

	/* Configure the external interrupt pins as inputs */
	sc = sysconf_claim(SYS_CFG, 10, 0, 3, "irq");
	sysconf_write(sc, 0xf);

	register_intc_controller(&intc_desc);

	for (i = 0; i < 16; i++) {
		set_irq_chip(i, &dummy_irq_chip);
		set_irq_chained_handler(i, ilc_irq_demux);
	}

	ilc_demux_init();
}
