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
#include <linux/delay.h>
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
void __init mb442_setup(char** cmdline_p)
{
	unsigned long sysconf;
	unsigned long chip_revision, chip_7109;
	static struct stpio_pin *ethreset,*usbpower;

	printk("STMicroelectronics STb7100 Reference board initialisation\n");

	sysconf = ctrl_inl(SYSCONF_DEVICEID);
	chip_7109 = (((sysconf >> 12) & 0x3ff) == 0x02c);
	chip_revision = (sysconf >> 28) +1;

	if (chip_7109)
		printk("STb7109 version %ld.x\n", chip_revision);
	else
		printk("STb7100 version %ld.x\n", chip_revision);

	sysconf = ctrl_inl(SYSCONF_SYS_CFG(7));

	/* SCIF_PIO_OUT_EN=0 */
	/* Route UART2 and PWM to PIO4 instead of SCIF */
	sysconf &= ~(1<<0);

	/* Set SSC2_MUX_SEL = 0 */
	/* Treat SSC2 as I2C instead of SSC */
	sysconf &= ~(1<<3);

	ctrl_outl(sysconf, SYSCONF_SYS_CFG(7));

	/* Reset the SMSC 91C111 Ethernet chip */
	ethreset = stpio_request_pin(2, 6, "SMSC_RST", STPIO_OUT);
	stpio_set_pin(ethreset, 0);
	udelay(1);
	stpio_set_pin(ethreset, 1);
	udelay(1);
	stpio_set_pin(ethreset, 0);

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
	usbpower=stpio_request_pin(5,7, "USBPWR", STPIO_OUT);
	stpio_set_pin(usbpower, 1);

	/* Currently all STB1 chips have problems with the sleep instruction,
	 * so disable it here.
	 */
	disable_hlt();

#ifdef CONFIG_STMMAC_ETH
	/* Reset the PHY */
	ethreset = stpio_request_pin(2, 4, "STE100P_RST", STPIO_OUT);
	stpio_set_pin(ethreset, 1);
	udelay(1);
	stpio_set_pin(ethreset, 0);
	udelay(1000);
	stpio_set_pin(ethreset, 1);
#endif

#ifdef CONFIG_STM_PWM
	stpio_request_pin(4, 7, "PWM", STPIO_ALT_OUT);
#endif
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

static void phy_reset(void* bus)
{
	static struct stpio_pin *phyreset;

	if (phyreset == NULL) {
		phyreset = stpio_request_pin(2, 4, "ste100p_reset", STPIO_OUT);
	}

	stpio_set_pin(phyreset, 1);
	udelay(1);
	stpio_set_pin(phyreset, 0);
	udelay(1000);
	stpio_set_pin(phyreset, 1);
}

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

static struct platform_device *mb442_devices[] __initdata = {
	&smc91x_device,
	&physmap_flash,
};

static int __init device_init(void)
{
	int ret =0;
	ret = platform_add_devices(mb442_devices, ARRAY_SIZE(mb442_devices));
	return ret;
}

subsys_initcall(device_init);
