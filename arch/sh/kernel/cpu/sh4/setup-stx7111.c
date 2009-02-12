/*
 * STx7111 Setup
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
#include <linux/stm/fdma-plat.h>
#include <linux/stm/fdma-reqs.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/dma-mapping.h>
#include <asm/irl.h>
#include <asm/irq-ilc.h>

static struct sysconf_field *sc7_3;
static u64 st40_dma_mask = DMA_32BIT_MASK;


/* USB resources ----------------------------------------------------------- */

#define UHOST2C_BASE			0xfe100000
#define AHB2STBUS_WRAPPER_GLUE_BASE	(UHOST2C_BASE)
#define AHB2STBUS_OHCI_BASE		(UHOST2C_BASE + 0x000ffc00)
#define AHB2STBUS_EHCI_BASE		(UHOST2C_BASE + 0x000ffe00)
#define AHB2STBUS_PROTOCOL_BASE		(UHOST2C_BASE + 0x000fff00)

static struct plat_usb_data usb_wrapper =
	USB_WRAPPER(0, AHB2STBUS_WRAPPER_GLUE_BASE, AHB2STBUS_PROTOCOL_BASE,
		    USB_FLAGS_STRAP_16BIT	|
		    USB_FLAGS_STRAP_PLL		|
		    USB_FLAGS_STBUS_CONFIG_THRESHOLD256);

static struct platform_device  st40_ohci_device =
	USB_OHCI_DEVICE(0, AHB2STBUS_OHCI_BASE,
			evt2irq(0x1700), /* 168 */
			&usb_wrapper);

static struct platform_device  st40_ehci_device =
	USB_EHCI_DEVICE(0, AHB2STBUS_EHCI_BASE,
			evt2irq(0x1720), /* 169 */
			&usb_wrapper);

void __init stx7111_configure_usb(int inv_enable)
{
	static struct stpio_pin *pin;
	struct sysconf_field *sc;

	/* Power on USB */
	sc = sysconf_claim(SYS_CFG, 32, 4,4, "USB");
	sysconf_write(sc, 0);

	/* Work around for USB over-current detection chip being
	 * active low, and the 7111 being active high.
	 * Note this is an undocumented bit, which apparently enables
	 * an inverter on the overcurrent signal.
	 */
	sc = sysconf_claim(SYS_CFG, 6, 29,29, "USB");
	sysconf_write(sc, inv_enable);

	pin = stpio_request_pin(5,6, "USBOC", STPIO_IN);
	pin = stpio_request_pin(5,7, "USBPWR", STPIO_ALT_OUT);

	platform_device_register(&st40_ohci_device);
	platform_device_register(&st40_ehci_device);
}

/* FDMA resources ---------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

#include <linux/stm/7200_cut1_fdma2_firmware.h>

static struct fdma_regs stx7111_fdma_regs = {
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
	.fdma_dmem_region = STX7111_DMEM_OFFSET,
	.fdma_imem_region = STX7111_IMEM_OFFSET,
};

static struct fdma_platform_device_data stx7111_fdma0_plat_data = {
	.registers_ptr = &stx7111_fdma_regs,
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


static struct fdma_platform_device_data stx7111_fdma1_plat_data = {
	.registers_ptr = &stx7111_fdma_regs,
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

#define stx7111_fdma0_plat_data_addr &stx7111_fdma0_plat_data
#define stx7111_fdma1_plat_data_addr &stx7111_fdma1_plat_data
#else
#define stx7111_fdma0_plat_data_addr NULL
#define stx7111_fdma1_plat_data_addr NULL
#endif /* CONFIG_STM_DMA */

static struct platform_device fdma0_device = {
	.name		= "stmfdma",
	.id		= 0,
	.num_resources	= 2,
	.resource = (struct resource[2]) {
		[0] = {
			.start = STX7111_FDMA0_BASE,
			.end   = STX7111_FDMA0_BASE + 0xffff,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = LINUX_FDMA0_STX7111_IRQ_VECT,
			.end   = LINUX_FDMA0_STX7111_IRQ_VECT,
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = stx7111_fdma0_plat_data_addr,
	},
};

static struct platform_device fdma1_device = {
	.name		= "stmfdma",
	.id		= 1,
	.resource = (struct resource[2]) {
		[0] = {
			.start = STX7111_FDMA1_BASE,
			.end   = STX7111_FDMA1_BASE + 0xffff,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = LINUX_FDMA1_STX7111_IRQ_VECT,
			.end   = LINUX_FDMA1_STX7111_IRQ_VECT,
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = stx7111_fdma1_plat_data_addr,
	},
};

static struct platform_device fdma_xbar_device = {
	.name		= "fdma-xbar",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[1]) {
		{
			.start	= STX7111_XBAR_BASE,
			.end	= STX7111_XBAR_BASE+(4*1024)-1,
			.flags	= IORESOURCE_MEM,
		},
	},
};

/* SSC resources ----------------------------------------------------------- */

static char i2c_st[] = "i2c_st";
static char spi_st[] = "spi_st_ssc";

static struct platform_device stssc_devices[] = {
	STSSC_DEVICE(0xfd040000, evt2irq(0x10e0), 2, 0, 1, 2),
	STSSC_DEVICE(0xfd041000, evt2irq(0x10c0), 3, 0, 1, 2),
	STSSC_DEVICE(0xfd042000, evt2irq(0x10a0), 4, 0, 1, 0xff),
	STSSC_DEVICE(0xfd043000, evt2irq(0x1080), 0xff, 0xff, 0xff, 0xff),
};

void __init stx7111_configure_ssc(struct plat_ssc_data *data)
{
	int num_i2c=0;
	int num_spi=0;
	int i;
	int capability = data->capability;
	struct sysconf_field* ssc_sc;

	for (i=0; i < ARRAY_SIZE(stssc_devices); i++, capability >>= SSC_BITS_SIZE){
		struct ssc_pio_t *ssc_pio = stssc_devices[i].dev.platform_data;

		if(capability & SSC_UNCONFIGURED)
			continue;

		if (capability & SSC_I2C_CLK_UNIDIR)
			ssc_pio->clk_unidir = 1;

		switch (i) {
		case 0:
			/* spi_boot_not_comm = 0 */
			/* This is a signal from SPI block */
			/* Hope this is set correctly by default */
			break;

		case 1:
			/* dvo_out=0 */
			ssc_sc = sysconf_claim(SYS_CFG, 7, 10, 10, "ssc");
			sysconf_write(ssc_sc, 0);

			/* Select SSC1 instead of PCI interrupts */
			/* Early datasheet version erroneously said 9-11 */
			ssc_sc = sysconf_claim(SYS_CFG, 5, 10, 12, "ssc");
			sysconf_write(ssc_sc, 0);

			break;

		case 2:
			break;
		}

		/* SSCx_mux_sel */
		ssc_sc = sysconf_claim(SYS_CFG, 7, i+1, i+1, "ssc");

		if(capability & SSC_SPI_CAPABILITY){
			stssc_devices[i].name = spi_st;
			sysconf_write(ssc_sc, 1);
			stssc_devices[i].id = num_spi++;
			ssc_pio->chipselect = data->spi_chipselects[i];
		} else {
			stssc_devices[i].name = i2c_st;
			sysconf_write(ssc_sc, 0);
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

/* Ethernet MAC resources -------------------------------------------------- */

static struct sysconf_field *mac_speed_sc;

static void fix_mac_speed(void* priv, unsigned int speed)
{
	sysconf_write(mac_speed_sc, (speed == SPEED_100) ? 1 : 0);
}

/* Hopefully I can remove this now */
static void stx7111eth_hw_setup_null(void)
{
}

static struct plat_stmmacenet_data stx7111eth_private_data = {
	.bus_id = 0,
	.pbl = 32,
	.has_gmac = 1,
	.fix_mac_speed = fix_mac_speed,
	.hw_setup = stx7111eth_hw_setup_null,
};

static struct platform_device stx7111eth_device = {
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
                	.start  = evt2irq(0x12a0), /* 133, */
                	.end    = evt2irq(0x12a0),
                	.flags  = IORESOURCE_IRQ,
        	},
	},
	.dev = {
		.power.can_wakeup    = 1,
		.platform_data = &stx7111eth_private_data,
	}
};


void stx7111_configure_ethernet(int en_mii, int sel, int ext_clk, int phy_bus)
{
	struct sysconf_field *sc;

	stx7111eth_private_data.bus_id = phy_bus;

	/* Ethernet ON */
	sc = sysconf_claim(SYS_CFG, 7, 16, 16, "stmmac");
	sysconf_write(sc, 1);

	/* PHY EXT CLOCK: 0: provided by STX7111S; 1: external */
	sc = sysconf_claim(SYS_CFG, 7, 19, 19, "stmmac");
	sysconf_write(sc, ext_clk ? 1 : 0);

	/* MAC speed*/
	mac_speed_sc = sysconf_claim(SYS_CFG, 7, 20, 20, "stmmac");

	/* Default GMII/MII slection */
	sc = sysconf_claim(SYS_CFG, 7, 24, 26, "stmmac");
	sysconf_write(sc, sel & 0x7);

	/* MII mode */
	sc = sysconf_claim(SYS_CFG, 7, 27, 27, "stmmac");
	sysconf_write(sc, en_mii ? 1 : 0);

	platform_device_register(&stx7111eth_device);
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

void stx7111_configure_pwm(struct plat_stm_pwm_data *data)
{
	stm_pwm_device.dev.platform_data = data;

	if (data->flags & PLAT_STM_PWM_OUT0) {
		/* Route UART2 (in and out) and PWM_OUT0 instead of SCI to pins
		 * ssc2_mux_sel = 0 */
		if (!sc7_3)
			sc7_3 = sysconf_claim(SYS_CFG, 7, 3, 3, "pwm");
		sysconf_write(sc7_3, 0);
		stpio_request_pin(4, 6, "PWM", STPIO_ALT_OUT);
	}

	if (data->flags & PLAT_STM_PWM_OUT1) {
		stpio_request_pin(4, 7, "PWM", STPIO_ALT_OUT);
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
	STASC_DEVICE(0xfd030000, evt2irq(0x1160), 11, 15,
		     0, 0, 1, 4, 7,
		STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT), /* oe pin: 6 */
	STASC_DEVICE(0xfd031000, evt2irq(0x1140), 12, 16,
		     1, 0, 1, 4, 5,
		STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT), /* oe pin: 6 */
	STASC_DEVICE(0xfd032000, evt2irq(0x1120), 13, 17,
		     4, 3, 2, 4, 5,
		STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
	STASC_DEVICE(0xfd033000, evt2irq(0x1100), 14, 18,
		     5, 0, 1, 2, 3,
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
void __init stx7111_configure_asc(const int *ascs, int num_ascs, int console)
{
	int i;

	for (i=0; i<num_ascs; i++) {
		int port;
		unsigned char flags;
		struct platform_device *pdev;
		struct sysconf_field *sc;

		port = ascs[i] & 0xff;
		flags = ascs[i] >> 8;
		pdev = &stm_stasc_devices[port];

		switch (ascs[i]) {
		case 0:
			/* Route UART0 instead of PDES to pins.
			 * pdes_scmux_out = 0 */

			/* According to note against against SYSCFG7[7]
			 * this bit is in the PDES block.
			 * Lets just hope it powers up in UART mode! */

			/* Route CTS instead of emiss_bus_request[2] to pins. */
			if (!(flags & STASC_FLAG_NORTSCTS)) {
				sc = sysconf_claim(SYS_CFG, 5,3,3, "asc");
				sysconf_write(sc, 0);
			}

			break;

		case 1:
			if (!(flags & STASC_FLAG_NORTSCTS)) {
				/* Route CTS instead of emiss_bus_free_accesspend_in to pins */
				sc = sysconf_claim(SYS_CFG, 5, 6, 6, "asc");
				sysconf_write(sc, 0);

				/* Route RTS instead of PCI_PME_OUT to pins */
				sc = sysconf_claim(SYS_CFG, 5, 7, 7, "asc");
				sysconf_write(sc, 0);
			}

			/* What about SYS_CFG5[23]? */

			break;

		case 2:
			/* Route UART2 (in and out) instead of SCI to pins.
			 * ssc2_mux_sel = 0 */
			if (!sc7_3)
				sc7_3 = sysconf_claim(SYS_CFG, 7, 3, 3, "asc");
			sysconf_write(sc7_3, 0);

			break;

		case 3:
			/* Nothing to do! */
			break;
		}

		pdev->id = i;
		((struct stasc_uart_data*)(pdev->dev.platform_data))->flags = flags;
		stasc_configured_devices[stasc_configured_devices_count++] = pdev;
	}

	stasc_console_device = console;
}

/* Add platform device as configured by board specific code */
static int __init stb7111_add_asc(void)
{
	return platform_add_devices(stasc_configured_devices,
				    stasc_configured_devices_count);
}
arch_initcall(stb7111_add_asc);

/* LiRC resources ---------------------------------------------------------- */
static struct lirc_pio lirc_pios[] = {
        [0] = {
		.bank = 3,
		.pin  = 3,
		.dir  = STPIO_IN,
                .pinof= 0x00 | LIRC_IR_RX | LIRC_PIO_ON
	},
	[1] = {
		.bank = 3,
		.pin  = 4,
		.dir  = STPIO_IN,
                .pinof= 0x00 | LIRC_UHF_RX /* | LIRC_PIO_ON not available */
                },
	[2] = {
		.bank = 3,
		.pin  = 5,
		.dir  = STPIO_ALT_OUT,
                .pinof= 0x00 | LIRC_IR_TX | LIRC_PIO_ON
	},
	[3] = {
		.bank = 3,
		.pin  = 6,
		.dir  = STPIO_ALT_OUT,
                .pinof= 0x00 | LIRC_IR_TX | LIRC_PIO_ON
	},
};

static struct plat_lirc_data lirc_private_info = {
	/* For the 7111, the clock settings will be calculated by the driver
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
	.num_pio_pins = ARRAY_SIZE(lirc_pios)
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
	           .platform_data = &lirc_private_info
	}
};

void __init stx7111_configure_lirc(lirc_scd_t *scd)
{
	lirc_private_info.scd_info = scd;

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
 * stx7111_configure_nand - Configures NAND support for the STx7111
 *
 * Requires generic platform NAND driver (CONFIG_MTD_NAND_PLATFORM).
 * Uses 'gen_nand.x' as ID for specifying MTD partitions on the kernel
 * command line.
 */
void __init stx7111_configure_nand(struct nand_config_data *data)
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


/* This function assumes you are using the dedicated pins. Production boards will
 * more likely use the external interrupt pins and save the PIOs
 */
int stx7111_pcibios_map_platform_irq(struct pci_config_data *pci_config, u8 pin)
{
	int irq;
	int pin_type;

	if((pin > 4) || (pin < 1)) return -1;

	pin_type = pci_config->pci_irq[pin - 1];

	switch(pin_type) {
		case PCI_PIN_DEFAULT:
			irq = evt2irq(0xa00 + ( (pin - 1) * 0x20 ));
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

static struct platform_device pci_device =
	PCI_DEVICE(0xfe400000, 0xfe560000, PCI_WINDOW_START, PCI_WINDOW_SIZE, evt2irq(0x1280), evt2irq(0x1200));

void __init stx7111_configure_pci(struct pci_config_data *pci_conf)
{
	int i;
	struct sysconf_field *sc;
	struct stpio_pin *pin;
	static const char *int_name[] = {"PCI INT A","PCI INT B","PCI INT C","PCI INT D"};
	static const char *req_name[] = {"PCI REQ 0 ","PCI REQ 1","PCI REQ 2","PCI REQ 3"};
	static const char *gnt_name[] = {"PCI GNT 0 ","PCI GNT 1","PCI GNT 2","PCI GNT 3"};


#ifndef CONFIG_PCI
	return;
#else

	/* Fill in the default values for the 7111 */
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

	for(i = 0; i < 4; i++) {
                if(pci_conf->pci_irq[i] == PCI_PIN_DEFAULT) {
			/* Set the alternate function correctly in sysconfig */
			pin = stpio_request_pin(3, (i == 0) ? 7 : (3 - i), int_name[i], STPIO_IN);
			sc = sysconf_claim(SYS_CFG, 5, 9 + i, 9 + i, int_name[i]);
			sysconf_write(sc, 1);
                }
        }

	/* REQ/GNT 0 are dedicated pins */
	for( i = 1; i < 4; i++ ) {
		if(pci_conf->req_gnt[i] == PCI_PIN_DEFAULT) {
			pin = stpio_request_pin(0, ((3 - i) * 2) + 2, req_name[i], STPIO_IN);
			pin = stpio_request_pin(2, (3 - i) + 5 , gnt_name[i], STPIO_ALT_OUT);
			sc = sysconf_claim(SYS_CFG, 5, (3 - i) + 2,  (3 - i) + 2, req_name[i]);
			sysconf_write(sc, 1);
		}
	}

	/* Enable the SERR interrupt if wired up */
	if(pci_conf->serr_irq == PCI_PIN_DEFAULT ) {
		/* Use the default */
		pin = stpio_request_pin(5, 5, pci_device.resource[5].name, STPIO_IN);
	} else {
		pci_device.resource[5].start = 	pci_device.resource[5].end = pci_conf->serr_irq;
	}

	platform_device_register(&pci_device);
#endif
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
};

/* Initialise devices which are required early in the boot process. */
void __init stx7111_early_device_init(void)
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

	printk("STx7111 version %ld.x\n", chip_revision);

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

static int __init stx7111_postcore_setup(void)
{
	emi_init(0, 0xfe700000);

	return 0;
}
postcore_initcall(stx7111_postcore_setup);

/* Late resources ---------------------------------------------------------- */

static int __init stx7111_subsys_setup(void)
{
	/* we need to do PIO setup before module init, because some
	 * drivers (eg gpio-keys) require that the interrupts
	 * are available. */
	pio_late_setup();

	return 0;
}
subsys_initcall(stx7111_subsys_setup);

static struct platform_device *stx7111_devices[] __initdata = {
	&fdma0_device,
	//&fdma1_device,
	&fdma_xbar_device,
	&sysconf_device,
	&ilc3_device,
	&hwrandom_rng_device,
	&devrandom_rng_device,
};

static int __init stx7111_devices_setup(void)
{
	return platform_add_devices(stx7111_devices,
				    ARRAY_SIZE(stx7111_devices));
}
device_initcall(stx7111_devices_setup);

/* Interrupt initialisation ------------------------------------------------ */

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2_TUNI, TMU2_TICPI,
	WDT,
	HUDI,

	PCI_DEV0, PCI_DEV1, PCI_DEV2, PCI_DEV3,		/* Group 0 */
	I2S2SPDIF1, I2S2SPDIF2, I2S2SPDIF3,
	AUX_VDP_END_PROC, AUX_VDP_FIFO_EMPTY,
	COMPO_CAP_BF, COMPO_CAP_TF,
	STANDALONE_PIO,
	PIO0, PIO1, PIO2,
	PIO6, PIO5, PIO4, PIO3,				/* Group 1 */
	SSC3, SSC2, SSC1, SSC0,				/* Group 2 */
	UART3, UART2, UART1, UART0,			/* Group 3 */
	IRB_WAKEUP, IRB, PWM, MAFE,			/* Group 4 */
	PCI_ERROR, FE900, DAA, TTXT,			/* Group 5 */
	EMPI_PCI_DMA, GMAC, TS_MERGER, SBAG_OR_CEC,	/* Group 6 */
	LX_DELTAMU, LX_AUD, DCXO, PMT,			/* Group 7 */
	FDMA0, FDMA1, I2S2SPDIF, HDMI_CEC,		/* Group 8 */
	PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR,		/* Group 9 */
	DCS0, DELPHI_PRE0, NAND, DELPHI_MBE,		/* Group 10 */
	MAIN_VDP_FIFO_EMPTY, MAIN_VDP_END_PROCESSING,	/* Group 11 */
	MAIN_VTG, AUX_VTG,
	BDISP_AQ, DVP, HDMI, HDCP,			/* Group 12 */
	PTI, PDES_ESA0_SEL, PDES, PDES_READ_CW,		/* Group 13 */
	TKDMA_TKD, TKDMA_DMA, CRIPTO_SIG_DMA,		/* Group 14 */
	CRIPTO_SIG_CHK,
	OHCI, EHCI, DCS1, BDISP_CQ,			/* Group 15 */
	ICAM3_KTE, ICAM3, KEY_SCANNER, MES_LMI_SYS,	/* Group 16 */

	/* interrupt groups */
	TMU2, RTC,
	GROUP0_1, GROUP0_2, GROUP1, GROUP2, GROUP3,
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

	INTC_VECT(PCI_DEV0, 0xa00), INTC_VECT(PCI_DEV1, 0xa20),
	INTC_VECT(PCI_DEV2, 0xa40), INTC_VECT(PCI_DEV3, 0xa60),
	INTC_VECT(I2S2SPDIF1, 0xa80), INTC_VECT(I2S2SPDIF2, 0xb00),
	INTC_VECT(I2S2SPDIF3, 0xb20),
	INTC_VECT(AUX_VDP_END_PROC, 0xb40), INTC_VECT(AUX_VDP_FIFO_EMPTY, 0xb60),
	INTC_VECT(COMPO_CAP_BF, 0xb80), INTC_VECT(COMPO_CAP_TF, 0xba0),
	INTC_VECT(STANDALONE_PIO, 0xbc0),
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
	INTC_VECT(PCI_ERROR, 0x1200), INTC_VECT(FE900, 0x1220),
	INTC_VECT(DAA, 0x1240), INTC_VECT(TTXT, 0x1260),
	INTC_VECT(EMPI_PCI_DMA, 0x1280), INTC_VECT(GMAC, 0x12a0),
	INTC_VECT(TS_MERGER, 0x12c0), INTC_VECT(SBAG_OR_CEC, 0x12e0),
	INTC_VECT(LX_DELTAMU, 0x1300), INTC_VECT(LX_AUD, 0x1320),
	INTC_VECT(DCXO, 0x1340), INTC_VECT(PMT, 0x1360),
	INTC_VECT(FDMA0, 0x1380), INTC_VECT(FDMA1, 0x13a0),
	INTC_VECT(I2S2SPDIF, 0x13c0), INTC_VECT(HDMI_CEC, 0x13e0),
	INTC_VECT(PCMPLYR0, 0x1400), INTC_VECT(PCMPLYR1, 0x1420),
	INTC_VECT(PCMRDR, 0x1440), INTC_VECT(SPDIFPLYR, 0x1460),
	INTC_VECT(DCS0, 0x1480), INTC_VECT(DELPHI_PRE0, 0x14a0),
	INTC_VECT(NAND, 0x14c0), INTC_VECT(DELPHI_MBE, 0x14e0),
	INTC_VECT(MAIN_VDP_FIFO_EMPTY, 0x1500), INTC_VECT(MAIN_VDP_END_PROCESSING, 0x1520),
	INTC_VECT(MAIN_VTG, 0x1540), INTC_VECT(AUX_VTG, 0x1560),
	INTC_VECT(BDISP_AQ, 0x1580), INTC_VECT(DVP, 0x15a0),
	INTC_VECT(HDMI, 0x15c0), INTC_VECT(HDCP, 0x15e0),
	INTC_VECT(PTI, 0x1600), INTC_VECT(PDES_ESA0_SEL, 0x1620),
	INTC_VECT(PDES, 0x1640), INTC_VECT(PDES_READ_CW, 0x1660),
	INTC_VECT(TKDMA_TKD, 0x1680), INTC_VECT(TKDMA_DMA, 0x16a0),
	INTC_VECT(CRIPTO_SIG_DMA, 0x16c0), INTC_VECT(CRIPTO_SIG_CHK, 0x16e0),
	INTC_VECT(OHCI, 0x1700), INTC_VECT(EHCI, 0x1720),
	INTC_VECT(DCS1, 0x1740), INTC_VECT(BDISP_CQ, 0x1760),
	INTC_VECT(ICAM3_KTE, 0x1780), INTC_VECT(ICAM3, 0x17a0),
	INTC_VECT(KEY_SCANNER, 0x17c0), INTC_VECT(MES_LMI_SYS, 0x17e0)
};

static struct intc_group groups[] = {
	INTC_GROUP(TMU2, TMU2_TUNI, TMU2_TICPI),

	/* PCI_DEV0 is not grouped */
	INTC_GROUP(GROUP0_1, PCI_DEV1, PCI_DEV2, PCI_DEV3,
		   I2S2SPDIF1),
	INTC_GROUP(GROUP0_2, I2S2SPDIF2, I2S2SPDIF3,
		   AUX_VDP_END_PROC, AUX_VDP_FIFO_EMPTY,
		   COMPO_CAP_BF, COMPO_CAP_TF, STANDALONE_PIO),
	/* PIO0, PIO1, PIO2 are not part of any group */
	INTC_GROUP(GROUP1, PIO6, PIO5, PIO4, PIO3),
	INTC_GROUP(GROUP2, SSC3, SSC2, SSC1, SSC0),
	INTC_GROUP(GROUP3, UART3, UART2, UART1, UART0),
	INTC_GROUP(GROUP4, IRB_WAKEUP, IRB, PWM, MAFE),
	INTC_GROUP(GROUP5, PCI_ERROR, FE900, DAA, TTXT),
	INTC_GROUP(GROUP6, EMPI_PCI_DMA, GMAC, TS_MERGER, SBAG_OR_CEC),
	INTC_GROUP(GROUP7, LX_DELTAMU, LX_AUD, DCXO, PMT),
	INTC_GROUP(GROUP8, FDMA0, FDMA1, I2S2SPDIF, HDMI_CEC),
	INTC_GROUP(GROUP9, PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR),
	INTC_GROUP(GROUP10, DCS0, DELPHI_PRE0, NAND, DELPHI_MBE),
	INTC_GROUP(GROUP11, MAIN_VDP_FIFO_EMPTY, MAIN_VDP_END_PROCESSING,
		   MAIN_VTG, AUX_VTG),
	INTC_GROUP(GROUP12, BDISP_AQ, DVP, HDMI, HDCP),
	INTC_GROUP(GROUP13, PTI, PDES_ESA0_SEL, PDES, PDES_READ_CW),
	INTC_GROUP(GROUP14, TKDMA_TKD, TKDMA_DMA, CRIPTO_SIG_DMA,
		   CRIPTO_SIG_CHK),
	INTC_GROUP(GROUP15, OHCI, EHCI, DCS1, BDISP_CQ),
	INTC_GROUP(GROUP16, ICAM3_KTE, ICAM3, KEY_SCANNER, MES_LMI_SYS),
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
						    PIO0, GROUP0_2, GROUP0_1, PCI_DEV0 } },
	{ 0x00000304, 0, 32, 4, /* INTPRI04 */ {  GROUP8,  GROUP7,  GROUP6,  GROUP5,
						  GROUP4,  GROUP3,  GROUP2,  GROUP1 } },
	{ 0x00000308, 0, 32, 4, /* INTPRI08 */ { GROUP16, GROUP15, GROUP14, GROUP13,
						 GROUP12, GROUP11, GROUP10,  GROUP9 } },
};

static struct intc_mask_reg mask_registers[] = {
	{ 0x00000340, 0x00000360, 32, /* INTMSK00 / INTMSKCLR00 */
	  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 31..16 */
	    0, PIO2, PIO1, PIO0,				/* 15..12 */
	    STANDALONE_PIO, COMPO_CAP_TF, COMPO_CAP_BF,	AUX_VDP_FIFO_EMPTY, /* 11...8 */
	    AUX_VDP_END_PROC, I2S2SPDIF3, I2S2SPDIF2, I2S2SPDIF1, /*  7...4 */
	    PCI_DEV3, PCI_DEV2, PCI_DEV1, PCI_DEV0 } },		/*  3...0 */
	{ 0x00000344, 0x00000364, 32, /* INTMSK04 / INTMSKCLR04 */
	  { HDMI_CEC, I2S2SPDIF, FDMA1, FDMA0,			/* 31..28 */
	    PMT, DCXO, LX_AUD, LX_DELTAMU,			/* 27..24 */
	    SBAG_OR_CEC, TS_MERGER, GMAC, EMPI_PCI_DMA,		/* 23..20 */
	    TTXT, DAA, FE900, PCI_ERROR,			/* 19..16 */
	    MAFE, PWM, IRB, IRB_WAKEUP,				/* 15..12 */
	    UART0, UART1, UART2, UART3,				/* 11...8 */
	    SSC0, SSC1, SSC2, SSC3,				/*  7...4 */
	    PIO3, PIO4, PIO5, PIO6 } },				/*  3...0 */
	{ 0x00000348, 0x00000368, 32, /* INTMSK08 / INTMSKCLR08 */
	  { MES_LMI_SYS, KEY_SCANNER, ICAM3, ICAM3_KTE,		/* 31..28 */
	    BDISP_CQ, DCS1, EHCI, OHCI,				/* 27..24 */
	    CRIPTO_SIG_CHK, CRIPTO_SIG_DMA, TKDMA_DMA, TKDMA_TKD, /* 23..20 */
	    PDES_READ_CW, PDES, PDES_ESA0_SEL, PTI,		/* 19..16 */
	    HDCP, HDMI, DVP, BDISP_AQ,				/* 15..12 */
	    AUX_VTG, MAIN_VTG, MAIN_VDP_END_PROCESSING, MAIN_VDP_FIFO_EMPTY, /* 11...8 */
	    DELPHI_MBE, NAND, DELPHI_PRE0, DCS0,		/*  7...4 */
	    SPDIFPLYR, PCMRDR, PCMPLYR1, PCMPLYR0 } }		/*  3...0 */
};

static DECLARE_INTC_DESC(intc_desc, "stx7111", vectors, groups,
			 mask_registers, prio_registers, NULL);

#define INTC_ICR	0xffd00000UL
#define INTC_ICR_IRLM   (1<<7)

void __init plat_irq_setup(void)
{
	struct sysconf_field *sc;
	unsigned long intc2_base = (unsigned long)ioremap(0xfe001000, 0x400);
	int i;
	static const int irl_irqs[4] = {
		IRL0_IRQ, IRL1_IRQ, IRL2_IRQ, IRL3_IRQ
	};

	for (i=4; i<=6; i++)
		prio_registers[i].set_reg += intc2_base;
	for (i=0; i<=2; i++) {
		mask_registers[i].set_reg += intc2_base;
		mask_registers[i].clr_reg += intc2_base;
	}

	register_intc_controller(&intc_desc);

	/* Configure the external interrupt pins as inputs */
	sc = sysconf_claim(SYS_CFG, 10, 0, 3, "irq");
	sysconf_write(sc, 0xf);

	/* Disable encoded interrupts */
	ctrl_outw(ctrl_inw(INTC_ICR) | INTC_ICR_IRLM, INTC_ICR);

	/* Don't change the default priority assignments, so we get a
	 * range of priorities for the ILC3 interrupts by picking the
	 * correct output. */

	for (i=0; i<4; i++) {
		int irq = irl_irqs[i];
		set_irq_chip(irq, &dummy_irq_chip);
		set_irq_chained_handler(irq, ilc_irq_demux);
	}

	ilc_early_init(&ilc3_device);
	ilc_demux_init();
}
