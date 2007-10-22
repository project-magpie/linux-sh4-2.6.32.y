/*
 * arch/sh/boards/st/mb448/setup.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STb7109E Reference board support.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/platform_device.h>
#include <linux/mtd/physmap.h>
#include <asm/io.h>

#define SYSCONF_BASE 0xb9001000
#define SYSCONF_DEVICEID	(SYSCONF_BASE + 0x000)
#define SYSCONF_SYS_STA(n)	(SYSCONF_BASE + 0x008 + ((n) * 4))
#define SYSCONF_SYS_CFG(n)	(SYSCONF_BASE + 0x100 + ((n) * 4))

/*
 * Initialize the board
 */
void __init platform_setup(void)
{
	unsigned long sysconf;
	unsigned long chip_revision;
	static struct stpio_pin *ethreset;

	printk("STMicroelectronics STb7109E Reference board initialisation\n");

	sysconf = ctrl_inl(SYSCONF_DEVICEID);
	chip_revision = (sysconf >> 28) + 1;
	printk("STb7109 version %ld.x\n", chip_revision);

	/* Route UART2 instead of SCI to PIO4 */
	/* Set ssc2_mux_sel = 0 */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(7));
	sysconf &= ~(1<<3);
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(7));

	/* Permanently enable Flash VPP */
	{
		static struct stpio_pin *pin;
		pin = stpio_request_pin(2,7, "VPP", STPIO_OUT);
		stpio_set_pin(pin, 1);
	}

	/* Reset the SMSC 91C111 Ethernet chip */
	ethreset = stpio_request_pin(2, 6, "SMSC_RST", STPIO_OUT);
	stpio_set_pin(ethreset, 0);
	udelay(1);
	stpio_set_pin(ethreset, 1);
	udelay(1);
	stpio_set_pin(ethreset, 0);

	/* Work around for USB over-current detection chip being
	 * active low, and the 7109 being active high */
	if (chip_revision < 2) {
		static struct stpio_pin *pin;
		pin = stpio_request_pin(5,6, "USBOC", STPIO_OUT);
		stpio_set_pin(pin, 0);
	}

	/* Currently all STB1 chips have problems with the sleep instruction,
	 * so disable it here.
	 */
	disable_hlt();
}

const char *get_system_type(void)
{
	return "STb7109E Reference board";
}

static struct resource smc91x_resources[] = {
	[0] = {
		.start	= 0xa2000300,
		.end	= 0xa2000300 + 0xff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
	  .start	= IRL3_IRQ,
		.end	= IRL3_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(smc91x_resources),
	.resource	= smc91x_resources,
};

#ifdef CONFIG_MTD_PHYSMAP
static struct mtd_partition mtd_parts_table[3] = {
	{
	 .name = "Boot firmware",
	 .size = 0x00040000,
	 .offset = 0x00000000,
	 },
	{
	 .name = "Kernel",
	 .size = 0x00100000,
	 .offset = 0x00040000,

	 },
	{
	 .name = "Root FS",
	 .size = MTDPART_SIZ_FULL,	/* will expand to the end of the flash */
	 .offset = 0x00140000,
	 }
};

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.set_vpp	= NULL,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};
#define physmap_flash_data_addr &physmap_flash_data
#else
#define physmap_flash_data_addr NULL
#endif

static struct resource physmap_flash_resource = {
	.start		= 0x00000000,
	.end		= 0x00800000 - 1,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.dev		= {
		.platform_data	= physmap_flash_data_addr,
	},
	.num_resources	= 1,
	.resource	= &physmap_flash_resource,
};

static struct platform_device *mb448_devices[] __initdata = {
	&smc91x_device,
	&physmap_flash,
};

static int __init device_init(void)
{
	int ret =0;
	ret = platform_add_devices(mb448_devices, ARRAY_SIZE(mb448_devices));
	return ret;
}

device_initcall(device_init);
