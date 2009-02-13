/*
 * STx5197 Setup
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

/*
 * Config control A and B and config monitor A and B are in the High
 * Speed (HS) config regiter block (in STBus Group 1). We don't currently
 * map these.
 *
 * The remaining config control and config monitor registers are in the
 * High Density (HD) config register block (in STBus Group 3). These are
 * mapped using the sysconf driver as it does the right thing, as long
 * as we disregard the distinction between SYS_STA and SYS_CFG because
 * monitor and control registers are intermixed.
 *
 * Note registers are documented as offsets, but the sysconf driver
 * always multiples by 4, hence the divide below.
 */

#define CFG_CONTROL_C	(0x00 / 4)
#define CFG_CONTROL_D	(0x04 / 4)
#define CFG_CONTROL_E	(0x08 / 4)
#define CFG_CONTROL_F	(0x0c / 4)
#define CFG_CONTROL_G	(0x10 / 4)
#define CFG_CONTROL_H	(0x14 / 4)
#define CFG_CONTROL_I	(0x18 / 4)
#define CFG_CONTROL_J	(0x1c / 4)

#define CFG_CONTROL_K	(0x40 / 4)
#define CFG_CONTROL_L	(0x44 / 4)
#define CFG_CONTROL_M	(0x48 / 4)
#define CFG_CONTROL_N	(0x4c / 4)
#define CFG_CONTROL_O	(0x50 / 4)
#define CFG_CONTROL_P	(0x54 / 4)
#define CFG_CONTROL_Q	(0x58 / 4)
#define CFG_CONTROL_R	(0x5c / 4)

#define CFG_MONITOR_C	(0x20 / 4)
#define CFG_MONITOR_D	(0x24 / 4)
#define CFG_MONITOR_E	(0x28 / 4)
#define CFG_MONITOR_F	(0x2c / 4)
#define CFG_MONITOR_G	(0x30 / 4)
#define CFG_MONITOR_H	(0x34 / 4)
#define CFG_MONITOR_I	(0x38 / 4)
#define CFG_MONITOR_J	(0x3c / 4)

#define CFG_MONITOR_K	(0x60 / 4)
#define CFG_MONITOR_L	(0x64 / 4)
#define CFG_MONITOR_M	(0x68 / 4)
#define CFG_MONITOR_N	(0x6c / 4)
#define CFG_MONITOR_O	(0x70 / 4)
#define CFG_MONITOR_P	(0x74 / 4)
#define CFG_MONITOR_Q	(0x78 / 4)
#define CFG_MONITOR_R	(0x7c / 4)

struct {
	unsigned char cfg;
	unsigned char off[2];
} const pio_conf[5] = {
	{ CFG_CONTROL_F, {  0,  8} },
	{ CFG_CONTROL_F, { 16, 24} },
	{ CFG_CONTROL_G, {  0,  8} },
	{ CFG_CONTROL_G, { 16, 24} },
	{ CFG_CONTROL_O, {  0,  8} }
};

static void stx5197_pio_conf(int bank, int pin, int alt, const char *name)
{
	int cfg = pio_conf[bank].cfg;
	int bit[2] = {
		 pio_conf[bank].off[0] + pin,
		 pio_conf[bank].off[1] + pin
	};
	struct sysconf_field *sc[2];

	sc[0] = sysconf_claim(SYS_CFG, cfg, bit[0], bit[0], name);
	sc[1] = sysconf_claim(SYS_CFG, cfg, bit[1], bit[1], name);
	sysconf_write(sc[0], (alt >> 0) & 1);
	sysconf_write(sc[1], (alt >> 1) & 1);
}

static u64 st40_dma_mask = DMA_32BIT_MASK;

/* USB resources ----------------------------------------------------------- */

#define UHOST2C_BASE			0xfdd00000
#define AHB2STBUS_WRAPPER_GLUE_BASE	(UHOST2C_BASE)
#define AHB2STBUS_OHCI_BASE		(UHOST2C_BASE + 0x000ffc00)
#define AHB2STBUS_EHCI_BASE		(UHOST2C_BASE + 0x000ffe00)
#define AHB2STBUS_PROTOCOL_BASE		(UHOST2C_BASE + 0x000fff00)

static struct plat_usb_data usb_wrapper =
	USB_WRAPPER(0, AHB2STBUS_WRAPPER_GLUE_BASE, AHB2STBUS_PROTOCOL_BASE,
		    USB_FLAGS_STRAP_16BIT	|
		    USB_FLAGS_STRAP_PLL		|
		    USB_FLAGS_STBUS_CONFIG_THRESHOLD256);

static struct platform_device st_usb =
	USB_DEVICE(0, AHB2STBUS_EHCI_BASE, ILC_IRQ(29),
		      AHB2STBUS_OHCI_BASE, ILC_IRQ(28),
		      &usb_wrapper);

void __init stx5197_configure_usb(void)
{
	struct sysconf_field *sc;

	/* USB power down */
	sc = sysconf_claim(SYS_CFG, CFG_CONTROL_H, 8, 8, "USB");
	sysconf_write(sc, 0);

	/* DDR enable for ULPI. 0=8 bit SDR ULPI, 1=4 bit DDR ULPI */
	sc = sysconf_claim(SYS_CFG, CFG_CONTROL_M, 12, 12, "USB");
	sysconf_write(sc, 0);

	platform_device_register(&st_usb);

}

/* FDMA resources ---------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

#include <linux/stm/7200_cut1_fdma2_firmware.h>

static struct fdma_regs stx5197_fdma_regs = {
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
	.fdma_dmem_region = STX7111_DMEM_OFFSET,
	.fdma_imem_region = STX7111_IMEM_OFFSET,
};

static struct fdma_platform_device_data stx5197_fdma_plat_data = {
	.registers_ptr = &stx5197_fdma_regs,
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

#define stx5197_fdma_plat_data_addr &stx5197_fdma_plat_data
#else
#define stx5197_fdma_plat_data_addr NULL
#endif /* CONFIG_STM_DMA */

static struct platform_device fdma_device = {
	.name		= "stmfdma",
	.id		= -1,
	.num_resources	= 2,
	.resource = (struct resource[2]) {
		[0] = {
			.start = 0xfdb00000,
			.end   = 0xfdb0ffff,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = ILC_IRQ(34),
			.end   = ILC_IRQ(34),
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = stx5197_fdma_plat_data_addr,
	},
};

/* SSC resources ----------------------------------------------------------- */

static char i2c_st[] = "i2c_st";
static char spi_st[] = "spi_st_ssc";

static struct platform_device stssc_devices[] = {
	STSSC_DEVICE(0xfd140000, ILC_IRQ(5),  1, 6, 7, 0xff),
	STSSC_DEVICE(0xfd141000, ILC_IRQ(6),  SSC_NO_PIO, 0,0,0),
	STSSC_DEVICE(0xfd142000, ILC_IRQ(17), 3, 3, 2, 0xff),
};

static struct sysconf_field *spi_cs;
static void stx5197_ssc0_cs(void *spi, int is_on)
{
	sysconf_write(spi_cs, is_on ? 0 : 1);
}

void __init stx5197_configure_ssc(struct plat_ssc_data *data)
{
	int num_i2c = 0;
	int num_spi = 0;
	int i;
	int capability = data->capability;
	int routing = data->routing;
	struct sysconf_field *sc;
	const unsigned char alt_ssc[3] = { 2, 0xff, 1 };
	const unsigned char alt_pio[3] = { 1, 0xff, 0 };

	for (i = 0; i < ARRAY_SIZE(stssc_devices);
	     i++, capability >>= SSC_BITS_SIZE) {
		struct ssc_pio_t *ssc_pio = stssc_devices[i].dev.platform_data;

		if (capability & SSC_UNCONFIGURED)
			continue;

		if (capability & SSC_I2C_CLK_UNIDIR)
			ssc_pio->clk_unidir = 1;

		switch (i) {
		case 0:
			/* SSC0 can either drive the SPI pins (in which
			 * case it is SPI) or PIO1[7:6] (I2C).
			 */

			/* spi_bootnotcomms
			 * 0: SSC0 -> PIO1[7:6], 1: SSC0 -> SPI */
			sc = sysconf_claim(SYS_CFG, CFG_CONTROL_M, 14, 14,
					   "ssc");

			if (capability & SSC_SPI_CAPABILITY) {
				sysconf_write(sc, 1);
				ssc_pio->pio[0].pio_port = SSC_NO_PIO;

				spi_cs = sysconf_claim(SYS_CFG, CFG_CONTROL_M,
						       13, 13, "ssc");
				sysconf_write(spi_cs, 1);
				ssc_pio->chipselect = stx5197_ssc0_cs;
			} else {
				sysconf_write(sc, 0);
			}

			/* pio_functionality_on_pio1_7.
			 * 0: QAM validation, 1: Normal PIO */
			sc = sysconf_claim(SYS_CFG, CFG_CONTROL_I, 2, 2, "ssc");
			sysconf_write(sc, 1);

			break;

		case 1:
			BUG_ON(capability & SSC_SPI_CAPABILITY);

			if (routing & SSC1_QPSK) {
				/* qpsk_debug_config
				 *  0 IP289 I2C input from PIO1[0:1]
				 *  1 IP289 input from BE COMMS SSC1
				 */
				sc = sysconf_claim(SYS_CFG, CFG_CONTROL_C,
						   1, 1, "ssc");
				sysconf_write(sc, 1);
			} else {
				  /* 0: QPSK repeater interface is routed to
				   *    QAM_SCLT/SDAT.
				   * 1: SSC1 is routed to QAM_SCLT/SDAT.
				   */
				  sc = sysconf_claim(SYS_CFG, CFG_CONTROL_K,
						     27, 27, "ssc");
				  sysconf_write(sc, 1);
			}
			break;

		case 2:
			/* SSC2 always drives PIO3[3:2] */
			BUG_ON(capability & SSC_SPI_CAPABILITY);
			break;
		}

		if (ssc_pio->pio[0].pio_port != SSC_NO_PIO) {
			int pin;

			for (pin = 0; pin < 2; pin++) {
				int portno = ssc_pio->pio[pin].pio_port;
				int pinno  = ssc_pio->pio[pin].pio_pin;
				int alt;

				if (capability & SSC_SPI_CAPABILITY)
#ifdef CONFIG_SPI_STM_PIO
					alt = alt_pio[i];
#else
					alt = alt_ssc[i];
#endif
				else
#ifdef CONFIG_I2C_ST40_PIO
					alt = alt_pio[i];
#else
					alt = alt_ssc[i];
#endif

				stx5197_pio_conf(portno, pinno, alt, "ssc");
			}
		}

		if (capability & SSC_SPI_CAPABILITY) {
			stssc_devices[i].name = spi_st;
			stssc_devices[i].id = num_spi++;
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

/* Ethernet MAC resources -------------------------------------------------- */

static struct sysconf_field *mac_speed_sc;

static void fix_mac_speed(void *priv, unsigned int speed)
{
	sysconf_write(mac_speed_sc, (speed == SPEED_100) ? 1 : 0);
}

static struct plat_stmmacenet_data stx5197eth_private_data = {
	.bus_id = 0,
	.pbl = 32,
	.fix_mac_speed = fix_mac_speed,
};

static struct platform_device stx5197eth_device = {
	.name		= "stmmaceth",
	.id		= 0,
	.num_resources	= 2,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfde00000,
			.end	= 0xfde0ffff,
			.flags	= IORESOURCE_MEM,
		},
		{
			.name	= "macirq",
			.start	= ILC_IRQ(24),
			.end	= ILC_IRQ(24),
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.power.can_wakeup    = 1,
		.platform_data = &stx5197eth_private_data,
	}
};

void stx5197_configure_ethernet(int rmii, int ext_clk, int phy_bus)
{
	struct sysconf_field *sc;

	stx5197eth_private_data.bus_id = phy_bus;

	/* Ethernet interface on */
	sc = sysconf_claim(SYS_CFG, CFG_CONTROL_E, 0, 0, "stmmac");
	sysconf_write(sc, 1);

	/* MII plyclk out enable: 0=output, 1=input */
	sc = sysconf_claim(SYS_CFG, CFG_CONTROL_E, 6, 6, "stmmac");
	sysconf_write(sc, ext_clk);

	/* MAC speed*/
	mac_speed_sc = sysconf_claim(SYS_CFG, CFG_CONTROL_E, 1, 1, "stmmac");

	/* RMII/MII pin mode */
	sc = sysconf_claim(SYS_CFG, CFG_CONTROL_E, 7, 8, "stmmac");
	sysconf_write(sc, rmii ? 2 : 3);

	/* MII mode */
	sc = sysconf_claim(SYS_CFG, CFG_CONTROL_E, 2, 2, "stmmac");
	sysconf_write(sc, rmii ? 0 : 1);

	platform_device_register(&stx5197eth_device);
}

/* PWM resources ----------------------------------------------------------- */

static struct resource stm_pwm_resource[] = {
	[0] = {
		.start	= 0xfd110000,
		.end	= 0xfd110000 + 0x67,
		.flags	= IORESOURCE_MEM
	},
	[1] = {
		.start	= ILC_IRQ(43),
		.end	= ILC_IRQ(43),
		.flags	= IORESOURCE_IRQ
	}
};

static struct platform_device stm_pwm_device = {
	.name		= "stm-pwm",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stm_pwm_resource),
	.resource	= stm_pwm_resource,
};

void stx5197_configure_pwm(struct plat_stm_pwm_data *data)
{
	stm_pwm_device.dev.platform_data = data;

	if (data->flags & PLAT_STM_PWM_OUT0) {
		stx5197_pio_conf(2, 4, 1, "pwm");
		stpio_request_pin(2, 4, "pwm", STPIO_ALT_OUT);
	}

	platform_device_register(&stm_pwm_device);
}

/* LiRC resources ---------------------------------------------------------- */
static struct lirc_pio lirc_pios[] = {
	[0] = {
		.bank  = 2,
		.pin   = 5,
		.dir   = STPIO_IN,
		.pinof = 0x00 | LIRC_IR_RX  | LIRC_PIO_ON
	},
	[1] = {
		.bank  = 2,
		.pin   = 6,
		.dir   = STPIO_IN,
		.pinof = 0x00 | LIRC_UHF_RX | LIRC_PIO_ON
		/* To have UHF available on :
		   MB704: need one wire from J14-C to J14-E
		   MB676: need one wire from  J6-E to J15-A */
	},
	[2] = {
		.bank  = 2,
		.pin   = 7,
		.dir   = STPIO_ALT_OUT,
		.pinof = 0x00 | LIRC_IR_TX | LIRC_PIO_ON
	}
};

static struct plat_lirc_data lirc_private_info = {
	/* The clock settings will be calculated by the driver
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

static struct resource lirc_resource[] = {
	[0] = {
		.start = 0xfd118000,
		.end   = 0xfd118000 + 0xa8,
		.flags = IORESOURCE_MEM
	},
	[1] = {
		.start = ILC_IRQ(19),
		.end   = ILC_IRQ(19),
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

void __init stx5197_configure_lirc(lirc_scd_t *scd)
{
	lirc_private_info.scd_info = scd;

	platform_device_register(&lirc_device);
}

/* ASC resources ----------------------------------------------------------- */

static struct platform_device stm_stasc_devices[] = {
	STASC_DEVICE(0xfd130000, ILC_IRQ(7), 8, 10,
		     0, 0, 1, 5, 4,
		     STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
	STASC_DEVICE(0xfd131000, ILC_IRQ(8), 9, 11,
		     4, 0, 1, 3, 2,
		     STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
	STASC_DEVICE(0xfd132000, ILC_IRQ(12), 3, 5,
		     1, 2, 3, 5, 4,
		     STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
	STASC_DEVICE(0xfd133000, ILC_IRQ(13), 4, 6,
		     2, 0, 1, 2, 5,
		     STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
};

static const unsigned char asc_alt[4][4] = {
	{ 0, 0, 2, 2 },
	{ 2, 2, 3, 2 },
	{ 1, 1, 1, 1 },
	{ 1, 1, 1, 1 },
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
void __init stx5197_configure_asc(const int *ascs, int num_ascs, int console)
{
	int i;

	for (i = 0; i < num_ascs; i++) {
		int port;
		unsigned char flags;
		struct platform_device *pdev;
		struct stasc_uart_data *uart_data;

		port = ascs[i] & 0xff;
		flags = ascs[i] >> 8;
		pdev = &stm_stasc_devices[port];
		uart_data = pdev->dev.platform_data;

		/* Tx */
		stx5197_pio_conf(uart_data->pio_port, uart_data->pio_pin[0],
				 asc_alt[port][0], "asc");
		/* Rx */
		stx5197_pio_conf(uart_data->pio_port, uart_data->pio_pin[1],
				 asc_alt[port][1], "asc");

		if (!(flags & STASC_FLAG_NORTSCTS)) {
			/* CTS */
			stx5197_pio_conf(uart_data->pio_port,
					 uart_data->pio_pin[2],
					 asc_alt[port][2], "asc");
			/* RTS */
			stx5197_pio_conf(uart_data->pio_port,
					 uart_data->pio_pin[3],
					 asc_alt[port][3], "asc");
		}
		pdev->id = i;
		((struct stasc_uart_data *)(pdev->dev.platform_data))->flags =
			flags;
		stasc_configured_devices[stasc_configured_devices_count++] =
			pdev;
	}

	stasc_console_device = console;
}

/* Add platform device as configured by board specific code */
static int __init stx5197_add_asc(void)
{
	return platform_add_devices(stasc_configured_devices,
				    stasc_configured_devices_count);
}
arch_initcall(stx5197_add_asc);

/* Early resources (sysconf and PIO) --------------------------------------- */

static struct platform_device sysconf_device = {
	.name		= "sysconf",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd901000,
			.end	= 0xfd901000 + 4095,
			.flags	= IORESOURCE_MEM
		}
	},
	.dev = {
		.platform_data = &(struct plat_sysconf_data) {
			.sys_device_offset = 0,
			.sys_sta_offset = 0,
			.sys_cfg_offset = 0,
		}
	}
};

static struct platform_device stpio_devices[] = {
	STPIO_DEVICE(0, 0xfd120000, ILC_IRQ(0)),
	STPIO_DEVICE(1, 0xfd121000, ILC_IRQ(1)),
	STPIO_DEVICE(2, 0xfd122000, ILC_IRQ(2)),
	STPIO_DEVICE(3, 0xfd123000, ILC_IRQ(3)),
	STPIO_DEVICE(4, 0xfd124000, ILC_IRQ(4)),
};

/* Initialise devices which are required early in the boot process. */
void __init stx5197_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(&sysconf_device);
	stpio_early_init(stpio_devices, ARRAY_SIZE(stpio_devices),
			 ILC_FIRST_IRQ+ILC_NR_IRQS);

	sc = sysconf_claim(SYS_DEV, CFG_MONITOR_H, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx5197 version %ld.x\n", chip_revision);

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

static struct platform_device ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd100000,
			.end	= 0xfd100000 + 0x900,
			.flags	= IORESOURCE_MEM
		}
	},
};

/* Pre-arch initialisation ------------------------------------------------- */

static int __init stx5197_postcore_setup(void)
{
	emi_init(0, 0xfde30000);

	return 0;
}
postcore_initcall(stx5197_postcore_setup);

/* Late resources ---------------------------------------------------------- */

static int __init stx5197_subsys_setup(void)
{
	/*
	 * We need to do PIO setup before module init, because some
	 * drivers (eg gpio-keys) require that the interrupts are
	 * available.
	 */
	pio_late_setup();

	return 0;
}
subsys_initcall(stx5197_subsys_setup);

static struct platform_device *stx5197_devices[] __initdata = {
	&fdma_device,
	&sysconf_device,
	&ilc3_device,
};

static int __init stx5197_devices_setup(void)
{
	return platform_add_devices(stx5197_devices,
				    ARRAY_SIZE(stx5197_devices));
}
device_initcall(stx5197_devices_setup);

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

static struct intc_prio priorities[] = {
};

static struct intc_prio_reg prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,       } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0,    0,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },
};

static DECLARE_INTC_DESC(intc_desc, "stx5197", vectors, groups,
			 priorities, NULL, prio_registers, NULL);

void __init plat_irq_setup(void)
{
	int i;

	register_intc_controller(&intc_desc);

	for (i = 0; i < 16; i++) {
		/*
		 * This is a hack to allow for the fact that we don't
		 * register a chip type for the IRL lines. Without
		 * this the interrupt type is "no_irq_chip" which
		 * causes problems when trying to register the chained
		 * handler.
		 */
		set_irq_chip(i, &dummy_irq_chip);

		set_irq_chained_handler(i, ilc_irq_demux);
	}

	ilc_early_init(&ilc3_device);
	ilc_demux_init();
}
