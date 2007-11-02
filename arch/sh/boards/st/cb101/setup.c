/*
 * arch/sh/boards/st/cb101/setup.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics cb101 board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/phy.h>
#include <asm/irq-ilc.h>

static int ascs[2] __initdata = { 2, 3 };

void __init cb101_setup(char** cmdline_p)
{
	stx7200_early_device_init();
	stx7200_configure_asc(ascs, 2, 1);
}

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		((SSC_I2C_CAPABILITY                     ) << (0*2)) |
		((SSC_I2C_CAPABILITY | SSC_SPI_CAPABILITY) << (1*2)) |
		((SSC_I2C_CAPABILITY                     ) << (2*2)) |
		((SSC_I2C_CAPABILITY | SSC_SPI_CAPABILITY) << (3*2)) |
		((SSC_I2C_CAPABILITY                     ) << (4*2)),
};

static struct mtd_partition mtd_parts_table[3] = {
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

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct platform_device physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 32*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &physmap_flash_data,
	},
};

static struct plat_stmmacphy_data phy_private_data = {
	/* MAC0: STE101P */
	.bus_id = 0,
	.phy_addr = 0,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
};

static struct platform_device cb101_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			/* See mb519 for why we disable interrupts here */
			.start	= -1,
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data,
	 }
};

static struct platform_device *cb101_devices[] __initdata = {
	&physmap_flash,
	&cb101_phy_device,
};

static int __init device_init(void)
{
	stx7200_configure_ssc(&ssc_private_info);
	stx7200_configure_usb();
	stx7200_configure_ethernet(0, 0, 1, 0);
        stx7200_configure_lirc();

	return platform_add_devices(cb101_devices, ARRAY_SIZE(cb101_devices));
}
arch_initcall(device_init);

static void __iomem *cb101_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init cb101_init_irq(void)
{
	/* enable individual interrupt mode for externals */
	plat_irq_setup_pins(IRQ_MODE_IRQ);

}

struct sh_machine_vector mv_cb101 __initmv = {
	.mv_name		= "cb101",
	.mv_setup		= cb101_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= cb101_init_irq,
	.mv_ioport_map		= cb101_ioport_map,
};
