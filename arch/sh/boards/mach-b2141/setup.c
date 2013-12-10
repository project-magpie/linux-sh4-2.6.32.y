/*
 * arch/sh/boards/mach-b2141/setup.c
 *
 * Copyright (C) 2013 STMicroelectronics Limited
 * Author: Patrice Chotard (patrice.chotard@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/leds.h>
#include <linux/tm1668.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/stm/platform.h>
#include <linux/stm/stxh205.h>
#include <linux/stm/sysconf.h>
#include <asm/irq-ilc.h>

#define B2141_GPIO_POWER_ON_ETH		stm_gpio(3, 3)

static void __init b2141_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics B2141 board initialisation\n");

	stxh205_early_device_init();

	/*
	 * UART_SW: JD8 connector (no flow control)
	 */
	stxh205_configure_asc(STXH205_ASC(10), &(struct stxh205_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1, });

	/*
	 * JD7 connector (no flow control)
	 */
	stxh205_configure_asc(STXH205_ASC(1), &(struct stxh205_asc_config) {
			.hw_flow_control = 0,
		});
}

static struct platform_device b2141_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "RED",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(3, 1),
			},
		},
	},
};

/* Serial Flash */
static struct stm_plat_spifsm_data b2141_serial_flash =  {
	.name		= "n25q256",
	.nr_parts	= 2,
	.parts = (struct mtd_partition []) {
		{
			.name = "Serial Flash 1",
			.size = 0x00080000,
			.offset = 0,
		}, {
			.name = "Serial Flash 2",
			.size = MTDPART_SIZ_FULL,
			.offset = MTDPART_OFS_NXTBLK,
		},
	},
	.capabilities = {
		/* Capabilities may be overriden by SoC configuration */
		.dual_mode = 1,
		.quad_mode = 1,
		.reset_signal = 0,
	},
};

/* NAND Flash */
static struct stm_nand_bank_data b2141_nand_flash = {
	.csn		= 0, /* EMI_CS0 hardwired to NAND_CS */
	.options	= NAND_NO_AUTOINCR | NAND_USE_FLASH_BBT,
	.nr_partitions	= 2,
	.partitions	= (struct mtd_partition []) {
		{
			.name	= "NAND Flash 1",
			.offset	= 0,
			.size	= 0x00800000
		}, {
			.name	= "NAND Flash 2",
			.offset = MTDPART_OFS_NXTBLK,
			.size	= MTDPART_SIZ_FULL
		},
	},
	.timing_data	=  &(struct stm_nand_timing_data) {
		.sig_setup	= 50,		/* times in ns */
		.sig_hold	= 50,
		.CE_deassert	= 0,
		.WE_to_RBn	= 100,
		.wr_on		= 10,
		.wr_off		= 40,
		.rd_on		= 10,
		.rd_off		= 40,
		.chip_delay	= 30,		/* in us */
	},
};

static int b2141_phy_reset(void *bus)
{
	/*
	 * IC+ IP101 datasheet specifies 10mS low period and device usable
	 * 2.5mS after rising edge. However experimentally it appear
	 * 10mS is required for reliable functioning.
	 */
	gpio_set_value(B2141_GPIO_POWER_ON_ETH, 0);
	mdelay(10);
	gpio_set_value(B2141_GPIO_POWER_ON_ETH, 1);
	mdelay(10);

	return 1;
}

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = b2141_phy_reset,
	.phy_mask = 0,
	.probed_phy_irq = ILC_IRQ(25), /* MDINT */
};

static struct platform_device *b2141_devices[] __initdata = {
	&b2141_leds,
};

static int __init device_init(void)
{
	/* The "POWER_ON_ETH" line should be rather called "PHY_RESET",
	 * but it isn't... ;-) */
	gpio_request(B2141_GPIO_POWER_ON_ETH, "POWER_ON_ETH");
	gpio_direction_output(B2141_GPIO_POWER_ON_ETH, 0);

	stxh205_configure_ethernet(&(struct stxh205_ethernet_config) {
			.mode = stxh205_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	/* phy irq has to be triggered low */
	set_irq_type(ILC_IRQ(25), IRQ_TYPE_LEVEL_LOW);

	stxh205_configure_usb(0);

	stxh205_configure_usb(1);

	/*
	 * ssc1: fe/demo
	 * STiH270: half NIM JD9, JF4
	 */
	stxh205_configure_ssc_i2c(STXH205_SSC(1), &(struct stxh205_ssc_config) {
			.routing.ssc1.sclk = stxh205_ssc1_sclk_pio12_0,
			.routing.ssc1.mtsr = stxh205_ssc1_mtsr_pio12_1, });
	/*
	 * SSC3: SYS
	 * UD22: M24256 (EEPROM)
	 */
	stxh205_configure_ssc_i2c(STXH205_SSC(3), &(struct stxh205_ssc_config) {
			.routing.ssc3.sclk = stxh205_ssc3_sclk_pio15_5,
			.routing.ssc3.mtsr = stxh205_ssc3_mtsr_pio15_6, });

	/*
	 * SSC11: HDMI
	 * UA4: HDMI2C1 -> HDMI
	 */
	stxh205_configure_ssc_i2c(STXH205_SSC(11), NULL);

	stxh205_configure_lirc(&(struct stxh205_lirc_config) {
#ifdef CONFIG_LIRC_STM_UHF
			.rx_mode = stxh205_lirc_rx_mode_uhf, });
#else
			.rx_mode = stxh205_lirc_rx_mode_ir, });
#endif

	stxh205_configure_pwm(&(struct stxh205_pwm_config) {
			/*
			 * PWM10 is connected to 12V->1.2V power supply
			 * for "debug purposes". Enable at your own risk!
			 */
			.out10_enabled = 0 });

	stxh205_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &b2141_nand_flash,
			.rbn.flex_connected = 1,});

	stxh205_configure_spifsm(&b2141_serial_flash);

	return platform_add_devices(b2141_devices,
			ARRAY_SIZE(b2141_devices));
}
arch_initcall(device_init);

static void __iomem *b2141_ioport_map(unsigned long port, unsigned int size)
{
	/* If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called. */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_b2141 __initmv = {
	.mv_name = "b2141",
	.mv_setup = b2141_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = b2141_ioport_map,
};

#if defined(CONFIG_HIBERNATION_ON_MEMORY)

#include "../../kernel/cpu/sh4/stm_hom.h"

static int b2141_board_freeze(struct stm_wakeup_devices *wkd)
{
	if (!wkd->stm_mac0_can_wakeup) 
		gpio_set_value(B2141_GPIO_POWER_ON_ETH, 0);
	return 0;
}

static int b2141_board_defrost(struct stm_wakeup_devices *wkd)
{
	b2141_phy_reset(NULL);
	return 0;
}

static struct stm_hom_board b2141_hom = {
	.freeze = b2141_board_freeze,
	.restore = b2141_board_defrost,
};

static int __init b2141_hom_register(void)
{
	return stm_hom_board_register(&b2141_hom);
}

module_init(b2141_hom_register);
#endif
