/*
 * arch/sh/boards/st/hms1/setup.c
 *
 * Copyright (C) 2006 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * HMS1 board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/lirc.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7100.h>
#include <asm/irl.h>



#define HMS1_PIO_FLASH_VPP stm_gpio(2, 5)



void __init hms1_setup(char** cmdline_p)
{
	printk("HMS1 board initialisation\n");

	stx7100_early_device_init();

	stx7100_configure_asc(2, &(struct stx7100_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1, });
	stx7100_configure_asc(3, &(struct stx7100_asc_config) {
			.hw_flow_control = 0,
			.is_console = 0, });
}



static void hms1_set_vpp(struct map_info *info, int enable)
{
	gpio_set_value(HMS1_PIO_FLASH_VPP, enable);
}

static struct mtd_partition hms1_mtd_parts_table[3] = {
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

static struct physmap_flash_data hms1_physmap_flash_data = {
	.width		= 2,
	.set_vpp	= hms1_set_vpp,
	.nr_parts	= ARRAY_SIZE(hms1_mtd_parts_table),
	.parts		= hms1_mtd_parts_table
};

static struct resource hms1_physmap_flash_resource = {
	.start		= 0x00000000,
	.end		= 0x00800000 - 1,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device hms1_physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.dev		= {
		.platform_data	= &hms1_physmap_flash_data,
	},
	.num_resources	= 1,
	.resource	= &hms1_physmap_flash_resource,
};

static struct platform_device hms1_smsc_lan9117 = {
	.name		= "smc911x",
	.id		= -1,
	.num_resources	= 4,
	.resource	= (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x01000000,
			.end   = 0x010000ff,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = IRL0_IRQ,
			.end   = IRL0_IRQ,
		},
		/* See end of "drivers/net/smsc_911x/smsc9118.c" file
		 * for description of two following resources. */
		{
			.flags = IORESOURCE_IRQ,
			.name  = "polarity",
			.start = 1,
			.end   = 1,
		},
		{
			.flags = IORESOURCE_IRQ,
			.name  = "type",
			.start = 1,
			.end   = 1,
		},
	},
};

static struct platform_device *hms1_devices[] __initdata = {
	&hms1_physmap_flash,
	&hms1_smsc_lan9117,
};

static int __init hms1_device_init(void)
{
	stx7100_configure_sata();

	stx7100_configure_pwm(&(struct stx7100_pwm_config) {
			.out0_enabled = 0,
			.out1_enabled = 1, });

	stx7100_configure_ssc_i2c(0);
	stx7100_configure_ssc_i2c(1);
	stx7100_configure_ssc_i2c(2);

	stx7100_configure_usb();

	stx7100_configure_pata(&(struct stx7100_pata_config) {
			.emi_bank = 3,
			.pc_mode = 1,
			.irq = IRL1_IRQ, });

	/* HMS1 has no on-board IR receivers/transmitters. Anyway we still
	 * configure them because receiver can be present on a front-panel
	 * add-on and transmitter can be plugged to J9 mini-jack connector */
	stx7100_configure_lirc(&(struct stx7100_lirc_config) {
			.rx_mode = stx7100_lirc_rx_mode_ir,
			.tx_enabled = 0,
			.tx_od_enabled = 1, });

	gpio_request(HMS1_PIO_FLASH_VPP, "Flash VPP");
	gpio_direction_output(HMS1_PIO_FLASH_VPP, 0);

	return platform_add_devices(hms1_devices, ARRAY_SIZE(hms1_devices));
}
arch_initcall(hms1_device_init);
