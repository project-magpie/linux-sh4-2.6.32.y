/*
 * arch/sh/boards/st/mb411/setup.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STb7100 MBoard board support.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stm/pio.h>
#include <linux/platform_device.h>
#include <linux/mtd/physmap.h>
#include <asm/io.h>
#include <asm/mb411/harp.h>

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
	unsigned char devid;
	unsigned char epldver;

	printk("STMicroelectronics STb7100 MBoard board initialisation\n");

	sysconf = ctrl_inl(SYSCONF_DEVICEID);
	chip_revision = (sysconf >> 28) +1;
	if ( ((sysconf >> 12) & 0x3ff) == 0x02c )
		printk("STb7109 version %ld.x\n", chip_revision);
	else
		printk("STb7100 version %ld.x\n", chip_revision);

	epldver = ctrl_inb(EPLD_EPLDVER),
	printk("EPLD v%dr%d, PCB ver %X\n",
	       epldver >> 4, epldver & 0xf,
	       ctrl_inb(EPLD_PCBVER));

	devid = ctrl_inb(EPLD_POD_DEVID);
	printk("POD EPLD version: %d, DevID: MB411(%d) Rev.%c\n",
	       ctrl_inb(EPLD_POD_REVID),
	       devid >> 4, 'A'-1+(devid & 0xf));

	/* Route UART2 instead of SCI to PIO4 */
	/* Set ssc2_mux_sel = 0 */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(7));
	sysconf &= ~(1<<3);
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(7));

        /* The ST40RTC sources its clock from clock */
        /* generator B */
        sysconf = ctrl_inl(SYSCONF_SYS_CFG(8));
        ctrl_outl(sysconf | 0x2, SYSCONF_SYS_CFG(8));

	/* Work around for USB over-current detection chip being
	 * active low, and the 710x being active high.
	 *
	 * This test is wrong for 7100 cut 3.0 (which needs the work
	 * around), but as we can't reliably determine the minor
	 * revision number, hard luck, this works for most people.
	 */
	if ( ( chip_7109 && (chip_revision < 2)) ||
	     (!chip_7109 && (chip_revision < 3)) ) {
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
	return "STb7100 MBoard board";
}

static struct resource smc91x_resources[] = {
	[0] = {
		.start	= 0xa3e00300,
		.end	= 0xa3e00300 + 0xff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 7,
		.end	= 7,
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

static void stb7100_mtd_set_vpp(struct map_info *map, int vpp)
{
	if (vpp) {
		harp_set_vpp_on();
	} else {
		harp_set_vpp_off();
	}
}

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.set_vpp	= stb7100_mtd_set_vpp,
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

static struct platform_device *mb411_devices[] __initdata = {
	&smc91x_device,
	&physmap_flash,
};

static int __init device_init(void)
{
        return platform_device_register(&smc91x_device);
}

subsys_initcall(device_init);
