#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/dma-mapping.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7100.h>
#include <asm/irq-ilc.h>



/* Ethernet MAC resources ------------------------------------------------- */

static struct stm_pad_config stx7100_ethernet_pad_configs[] = {
	[stx7100_ethernet_mode_mii] = {
		.labels_num = 2,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_STRINGS("VIDDIGOUT", "HSYNCH", "VSYNCH"),
			STM_PAD_LABEL_RANGE("VIDDIGOUT.YC", 0, 15),
		},
		.sysconf_values_num = 3,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* DVO_ETH_PAD_DISABLE and ETH_IF_ON */
			STM_PAD_SYS_CFG(7, 16, 17, 3),
			/* RMII_MODE */
			STM_PAD_SYS_CFG(7, 18, 18, 0),
			/*
			 * PHY_CLK_EXT: PHY external clock
			 * 0: PHY clock is provided by STx7109
			 * 1: PHY clock is provided by an external source
			 * Value assigned in stx7100_configure_ethernet()
			 */
			STM_PAD_SYS_CFG(7, 19, 19, -1),
		},
		.gpio_values_num = 1, /* see stx7100_configure_ethernet() */
		.gpio_values = (struct stm_pad_gpio_value []) {
			/* Claimed only when config->ext_clk == 0 */
			STM_PAD_PIO_ALT_OUT(3, 7), /* PHYCLK */
		},
	},
	[stx7100_ethernet_mode_rmii] = {
		.labels_num = 2,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL("VIDDIGOUT.HSYNCH"),
			STM_PAD_LABEL_LIST("VIDDIGOUT.YC",
					0, 1, 4, 5, 6, 8, 9, 15),
		},
		.sysconf_values_num = 3,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* DVO_ETH_PAD_DISABLE and ETH_IF_ON */
			STM_PAD_SYS_CFG(7, 16, 17, 3),
			/* RMII_MODE */
			STM_PAD_SYS_CFG(7, 18, 18, 1),
			/* PHY_CLK_EXT */
			STM_PAD_SYS_CFG(7, 19, 19, -1),
		},
		.gpio_values_num = 1,
		.gpio_values = (struct stm_pad_gpio_value []) {
			/* Claimed only when config->ext_clk == 0 */
			STM_PAD_PIO_ALT_OUT(3, 7), /* REF_CLK */
		},
	},
};

static void stx7100_ethernet_fix_mac_speed(void *bsp_priv, unsigned int speed)
{
	struct sysconf_field *mac_speed_sel = bsp_priv;

	sysconf_write(mac_speed_sel, (speed == SPEED_100) ? 1 : 0);
}

static struct stm_plat_stmmacenet_data stx7100_ethernet_platform_data = {
	/* .pbl & .pad_config are set in stx7100_configure_ethernet() */
	.has_gmac = 0,
	.fix_mac_speed = stx7100_ethernet_fix_mac_speed,
};

static struct platform_device stx7100_ethernet_device = {
	.name = "stmmaceth",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x18110000, 0x10000),
		STM_PLAT_RESOURCE_IRQ_NAMED("macirq", 133, -1),
	},
	.dev.platform_data = &stx7100_ethernet_platform_data,
};

void __init stx7100_configure_ethernet(struct stx7100_ethernet_config *config)
{
	static int configured;
	struct stx7100_ethernet_config default_config;
	struct stm_pad_config *pad_config;

	/* 7100 doesn't have a MAC */
	if (cpu_data->type == CPU_STX7100)
		return;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	pad_config = &stx7100_ethernet_pad_configs[config->mode];

	/* PIO3[7]: RMII: REF_CLK (in or out) MII: PHYCLK (out) */
	if ((config->mode == stx7100_ethernet_mode_mii) &&
	    (config->ext_clk)) {
		/* Do not claim PHYCLK pin */
		pad_config->gpio_values_num--;
	}
	pad_config->gpio_values[0].direction =
		config->ext_clk ? STM_GPIO_DIRECTION_IN :
		STM_GPIO_DIRECTION_ALT_OUT;
	pad_config->sysconf_values[2].value = (config->ext_clk ? 1 : 0);

	stx7100_ethernet_platform_data.pad_config = pad_config;
	stx7100_ethernet_platform_data.bus_id = config->phy_bus;

	/* MAC_SPEED_SEL */
	stx7100_ethernet_platform_data.bsp_priv =
			sysconf_claim(SYS_CFG, 7, 20, 20, "stmmac");

	/* Configure the ethernet MAC PBL depending on the cut of the chip */
	stx7100_ethernet_platform_data.pbl =
			(cpu_data->cut_major == 1) ? 1 : 32;

	platform_device_register(&stx7100_ethernet_device);
}

/* USB resources ---------------------------------------------------------- */

static int stx7100_usb_pad_claim(struct stm_pad_config *config, void *priv)
{
	struct sysconf_field *sc;
	u32 reg;
	int gpio;

	/* Work around for USB over-current detection chip being
	 * active low, and the 710x being active high.
	 *
	 * This test is wrong for 7100 cut 3.0 (which needs the work
	 * around), but as we can't reliably determine the minor
	 * revision number, hard luck, this works for most people.
	 */
	gpio = stm_pad_gpio(config, "USB_OVRCUR");
	if ((cpu_data->type == CPU_STX7109 && cpu_data->cut_major < 2) ||
			(cpu_data->type == CPU_STX7100 &&
			cpu_data->cut_major < 3))
		gpio_direction_output(gpio, 0);
	else
		gpio_direction_input(gpio);

	/*
	 * There have been two changes to the USB power enable signal:
	 *
	 * - 7100 upto and including cut 3.0 and 7109 1.0 generated an
	 *   active high enables signal. From 7100 cut 3.1 and 7109 cut 2.0
	 *   the signal changed to active low.
	 *
	 * - The 710x ref board (mb442) has always used power distribution
	 *   chips which have active high enables signals (on rev A and B
	 *   this was a TI TPS2052, rev C used the ST equivalent a ST2052).
	 *   However rev A and B had a pull up on the enables signal, while
	 *   rev C changed this to a pull down.
	 *
	 * The net effect of all this is that the easiest way to drive
	 * this signal is ignore the USB hardware and drive it as a PIO
	 * pin.
	 *
	 * (Note the USB over current input on the 710x changed from active
	 * high to low at the same cuts, but board revs A and B had a resistor
	 * option to select an inverted output from the TPS2052, so no
	 * software work around is required.)
	 */
	gpio = stm_pad_gpio(config, "USB_PWR");
	gpio_direction_output(gpio, 1);

	sc = sysconf_claim(SYS_CFG, 2, 1, 1, "stm-usb");
	BUG_ON(!sc);
	reg = sysconf_read(sc);
	if (reg) {
		sysconf_write(sc, 0);
		mdelay(30);
	}

	return 0;
}

static struct stm_plat_usb_data stx7100_usb_platform_data = {
	.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
		STM_PLAT_USB_FLAGS_STRAP_PLL |
		STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE,
	.pad_config = &(struct stm_pad_config) {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_STRINGS("USB", "DM", "DP", "REF"),
		},
		.gpio_values_num = 2,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_UNKNOWN_NAME(5, 6, "USB_OVERCUR"),
			STM_PAD_PIO_UNKNOWN_NAME(5, 7, "USB_PWR"),
		},
		.custom_claim = stx7100_usb_pad_claim,
	},
};

static u64 stx7100_usb_dma_mask = DMA_32BIT_MASK;

static struct platform_device stx7100_usb_device = {
	.name = "stm-usb",
	.id = 0,
	.dev = {
		.dma_mask = &stx7100_usb_dma_mask,
		.coherent_dma_mask = DMA_32BIT_MASK,
		.platform_data = &stx7100_usb_platform_data,
	},
	.num_resources = 6,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0x191ffe00, 0x100),
		STM_PLAT_RESOURCE_IRQ_NAMED("ehci", 169, -1),
		STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0x191ffc00, 0x100),
		STM_PLAT_RESOURCE_IRQ_NAMED("ohci", 168, -1),
		STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0x19100000, 0x100),
		STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0x191fff00, 0x100),
	},
};

void __init stx7100_configure_usb(void)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	platform_device_register(&stx7100_usb_device);
}



/* SATA resources --------------------------------------------------------- */

static struct stm_plat_sata_data stx7100_sata_platform_data = {
	/* filled in stx7100_configure_sata() */
};

static struct platform_device stx7100_sata_device = {
	.name = "sata-stm",
	.id = -1,
	.dev.platform_data = &stx7100_sata_platform_data,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x19209000, 0x1000),
		STM_PLAT_RESOURCE_IRQ(170, -1),
	},
};

void __init stx7100_configure_sata(void)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (cpu_data->type == CPU_STX7100 && cpu_data->cut_major == 1) {
		/* 7100 cut 1.x */
		stx7100_sata_platform_data.phy_init = 0x0013704A;
	} else {
		/* 7100 cut 2.x and cut 3.x and 7109 */
		stx7100_sata_platform_data.phy_init = 0x388fc;
	}

	if ((cpu_data->type == CPU_STX7109 && cpu_data->cut_major == 1) ||
			cpu_data->type == CPU_STX7100) {
		stx7100_sata_platform_data.only_32bit = 1;
		stx7100_sata_platform_data.pc_glue_logic_init = 0x1ff;
	} else {
		stx7100_sata_platform_data.only_32bit = 0;
		stx7100_sata_platform_data.pc_glue_logic_init = 0x100ff;
	}

	platform_device_register(&stx7100_sata_device);
}
