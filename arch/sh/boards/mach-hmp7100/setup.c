/*
 * arch/sh/boards/mach-hmp7100/setup.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STb7100 HMP board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/phy.h>
#include <linux/lirc.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7100.h>
#include <asm/irl.h>



#define HMP7100_PIO_FE_RESET stm_gpio(2, 6)
#define HMP7100_PIO_SMC91X_RESET stm_gpio(2, 7)
#define HMP7100_PIO_PHY_RESET stm_gpio(5, 3)



void __init hmp7100_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STb7100 HMP "
			"board initialisation\n");

	stx7100_early_device_init();

	stx7100_configure_asc(2, &(struct stx7100_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1, });
	stx7100_configure_asc(3, &(struct stx7100_asc_config) {
			.hw_flow_control = 0,
			.is_console = 0, });
}



static struct resource hmp7100_smc91x_resources[] = {
	[0] = {
		.start	= 0x02000000,
		.end	= 0x02000000 + 0xff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRL0_IRQ,
		.end	= IRL0_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.name  = "polarity",
		.start = 0,
		.end   = 0,
		.flags = IORESOURCE_IRQ,
	},
	[3] = {
		.name  = "type",
		.start = 1,
		.end   = 1,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device hmp7100_smc91x_device = {
	.name		= "smc911x",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(hmp7100_smc91x_resources),
	.resource	= hmp7100_smc91x_resources,
};

static struct mtd_partition hmp7100_mtd_parts_table[3] = {
	{
		.name = "Boot firmware",
		.size = 0x00040000,
		.offset = 0x00000000,
	}, {
		.name = "Kernel",
		.size = 0x00100000,
		.offset = 0x00040000,
	}, {
		.name = "Root FS",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00140000,
	}
};

static struct physmap_flash_data hmp7100_physmap_flash_data = {
	.width		= 2,
	.set_vpp	= NULL,
	.nr_parts	= ARRAY_SIZE(hmp7100_mtd_parts_table),
	.parts		= hmp7100_mtd_parts_table
};

static struct resource hmp7100_physmap_flash_resource = {
	.start		= 0x00000000,
	.end		= 0x00800000 - 1,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device hmp7100_physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.dev		= {
		.platform_data	= &hmp7100_physmap_flash_data,
	},
	.num_resources	= 1,
	.resource	= &hmp7100_physmap_flash_resource,
};



static int hmp7100_phy_reset(void *bus)
{
	gpio_set_value(HMP7100_PIO_PHY_RESET, 1);
	udelay(1);
	gpio_set_value(HMP7100_PIO_PHY_RESET, 0);
	udelay(1);
	gpio_set_value(HMP7100_PIO_PHY_RESET, 1);

	return 1;
}

static struct stm_plat_stmmacphy_data hmp7100_phy_private_data = {
	.bus_id = 0,
	.phy_addr = 2,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &hmp7100_phy_reset,
};

static struct platform_device hmp7100_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			.start	= IRL3_IRQ,
			.end	= IRL3_IRQ,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &hmp7100_phy_private_data,
	 }
};

static struct platform_device *hmp7100_devices[] __initdata = {
	&hmp7100_smc91x_device,
	&hmp7100_physmap_flash,
	&hmp7100_phy_device,
};

static int __init hmp7100_device_init(void)
{
	stx7100_configure_sata();

	stx7100_configure_pwm(&(struct stx7100_pwm_config) {
			.out0_enabled = 0,
			.out1_enabled = 1, });

	stx7100_configure_ssc_i2c(0);
	stx7100_configure_ssc_spi(1, NULL);
	stx7100_configure_ssc_i2c(2);

	stx7100_configure_usb();
	stx7100_configure_lirc(&(struct stx7100_lirc_config) {
			.rx_mode = stx7100_lirc_rx_mode_ir,
			.tx_enabled = 1,
			.tx_od_enabled = 0, });

	stx7100_configure_pata(&(struct stx7100_pata_config) {
			.emi_bank = 3,
			.pc_mode = 1,
			.irq = IRL1_IRQ, });

	gpio_request(HMP7100_PIO_PHY_RESET, "ste100p_reset");
	gpio_direction_output(HMP7100_PIO_PHY_RESET, 1);
	stx7100_configure_ethernet(&(struct stx7100_ethernet_config) {
			.mode = stx7100_ethernet_mode_mii,
			.ext_clk = 0,
			.phy_bus = 0 });

	/* Reset the FE chip */
	gpio_request(HMP7100_PIO_FE_RESET, "fe_reset");
	gpio_direction_output(HMP7100_PIO_FE_RESET, 1);
	udelay(1);
	gpio_set_value(HMP7100_PIO_FE_RESET, 0);
	udelay(1);
	gpio_set_value(HMP7100_PIO_FE_RESET, 1);

	/* Reset the SMSC 9118 Ethernet chip */
	gpio_request(HMP7100_PIO_SMC91X_RESET, "smc91x_reset");
	gpio_direction_output(HMP7100_PIO_SMC91X_RESET, 1);
	udelay(1);
	gpio_set_value(HMP7100_PIO_SMC91X_RESET, 0);
	udelay(1);
	gpio_set_value(HMP7100_PIO_SMC91X_RESET, 1);

	return platform_add_devices(hmp7100_devices,
				    ARRAY_SIZE(hmp7100_devices));
}
device_initcall(hmp7100_device_init);
