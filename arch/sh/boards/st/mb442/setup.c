/*
 * arch/sh/boards/st/mb442/setup.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STb7100 Reference board support.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <asm/io.h>

#define SYSCONF_BASE 0xb9001000
#define SYSCONF_DEVICEID	(SYSCONF_BASE + 0x000)
#define SYSCONF_SYS_STA(n)	(SYSCONF_BASE + 0x008 + ((n) * 4))
#define SYSCONF_SYS_CFG(n)	(SYSCONF_BASE + 0x100 + ((n) * 4))

/*
 * Initialize the board
 */
void __init mb442_setup(char** cmdline_p)
{
	unsigned long sysconf;
	unsigned long chip_revision, chip_7109;
	static struct stpio_pin *ethreset;

	printk("STMicroelectronics STb7100 Reference board initialisation\n");

	sysconf = ctrl_inl(SYSCONF_DEVICEID);
	chip_7109 = (((sysconf >> 12) & 0x3ff) == 0x02c);
	chip_revision = (sysconf >> 28) +1;

	if (chip_7109)
		printk("STb7109 version %ld.x\n", chip_revision);
	else
		printk("STb7100 version %ld.x\n", chip_revision);

	/* Route UART2 instead of SCI to PIO4 */
	/* Set ssc2_mux_sel = 0 */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(7));
	sysconf &= ~(1<<3);
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(7));

	/* Reset the SMSC 91C111 Ethernet chip */
	ethreset = stpio_request_pin(2, 6, "SMSC_RST", STPIO_OUT);
	stpio_set_pin(ethreset, 0);
	udelay(1);
	stpio_set_pin(ethreset, 1);
	udelay(1);
	stpio_set_pin(ethreset, 0);

	/* Currently all STB1 chips have problems with the sleep instruction,
	 * so disable it here.
	 */
	disable_hlt();
}

static struct resource smc91x_resources[] = {
	[0] = {
		.start	= 0xa2000300,
		.end	= 0xa2000300 + 0xff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRL0_IRQ,
		.end	= IRL0_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(smc91x_resources),
	.resource	= smc91x_resources,
};

static struct platform_device *mb442_devices[] __initdata = {
	&smc91x_device,
};

static int __init device_init(void)
{
	int ret =0;
	ret = platform_add_devices(mb442_devices, ARRAY_SIZE(mb442_devices));
	return ret;
}

subsys_initcall(device_init);
