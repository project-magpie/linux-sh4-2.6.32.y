/*
 * arch/sh/boards/st/mb671/setup.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx7200 Mboard support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/stm/emi.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7200.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/irq.h>
#include <asm/irq-ilc.h>
#include <mach/epld.h>
#include <mach/common.h>

static void __init mb671_setup(char **cmdline_p)
{
	printk(KERN_NOTICE "STMicroelectronics STx7200 Mboard "
			"initialisation\n");

	stx7200_early_device_init();

	stx7200_configure_asc(2, &(struct stx7200_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1 });
	stx7200_configure_asc(3, &(struct stx7200_asc_config) {
			.hw_flow_control = 1,
			.is_console = 0 });
}

static struct mtd_partition mb671_mtd_parts_table[3] = {
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

static void mb671_mtd_set_vpp(struct map_info *map, int vpp)
{
	/* Bit 0: VPP enable
	 * Bit 1: Reset (not used in later EPLD versions)
	 */

	if (vpp) {
		epld_write(3, EPLD_FLASH);
	} else {
		epld_write(2, EPLD_FLASH);
	}
}

static struct physmap_flash_data mb671_physmap_flash_data = {
	.width		= 2,
	.set_vpp	= mb671_mtd_set_vpp,
	.nr_parts	= ARRAY_SIZE(mb671_mtd_parts_table),
	.parts		= mb671_mtd_parts_table
};

static struct platform_device mb671_physmap_flash = {
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
		.platform_data	= &mb671_physmap_flash_data,
	},
};

static struct stm_plat_stmmacphy_data mb671_phy_private_data[2] = {
	{
		/* MII0: SMSC LAN8700 */
		.bus_id = 0,
		.phy_addr = -1,
		.phy_mask = 0,
		.interface = PHY_INTERFACE_MODE_RMII,
	}, {
		/* MII1: MB539B connected to J2 */
		.bus_id = 1,
		.phy_addr = -1,
		.phy_mask = 0,
		.interface = PHY_INTERFACE_MODE_MII,
	}
};

static struct platform_device mb671_phy_devices[2] = {
	{
		.name		= "stmmacphy",
		.id		= 0,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			{
				.name	= "phyirq",
				/* This should be:
				 * .start = ILC_IRQ(93),
				 * .end = ILC_IRQ(93),
				 * but mode pins setup (MII0_RXD[3] pulled
				 * down) disables nINT pin of LAN8700, so
				 * we are unable to use it... */
				.start	= -1,
				.end	= -1,
				.flags	= IORESOURCE_IRQ,
			},
		},
		.dev = {
			.platform_data = &mb671_phy_private_data[0],
		}
	}, {
		.name		= "stmmacphy",
		.id		= 1,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			{
				.name	= "phyirq",
				.start	= ILC_IRQ(95),
				.end	= ILC_IRQ(95),
				.flags	= IORESOURCE_IRQ,
			},
		},
		.dev.platform_data = &mb671_phy_private_data[1],
	}
};

static struct platform_device mb671_epld_device = {
	.name		= "epld",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= EPLD_BASE,
			.end	= EPLD_BASE + EPLD_SIZE - 1,
			.flags	= IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct plat_epld_data) {
		.opsize = 16,
	},
};

static struct platform_device *mb671_devices[] __initdata = {
	&mb671_epld_device,
	&mb671_physmap_flash,
	&mb671_phy_devices[0],
	&mb671_phy_devices[1],
};

static int __init mb671_devices_init(void)
{
	unsigned int epld_rev;
	unsigned int pcb_rev;

	epld_rev = epld_read(EPLD_EPLDVER);
	pcb_rev = epld_read(EPLD_PCBVER);
	printk(KERN_NOTICE "mb671 PCB rev %X EPLD rev %dr%d\n",
			pcb_rev, epld_rev >> 4, epld_rev & 0xf);

	stx7200_configure_ssc_i2c(0); /* HDMI */
	/* Usage of the remaining SSC is defined by the peripheral
	 * board (eg. MB520) */

	stx7200_configure_usb(0);
	stx7200_configure_usb(1);
	stx7200_configure_usb(2);

	stx7200_configure_sata(0);

#if 1 /* On-board PHY (MII0) in RMII mode, using MII_CLK */
	stx7200_configure_ethernet(0, &(struct stx7200_ethernet_config) {
			.mode = stx7200_ethernet_mode_rmii,
			.ext_clk = 0,
			.phy_bus = 0, });
#else /* External PHY board (MB539B) on MII1 in MII mode, using its own clock */
	stx7200_configure_ethernet(1, &(struct stx7200_ethernet_config) {
			.mode = stx7200_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 1, });
#endif

	return platform_add_devices(mb671_devices, ARRAY_SIZE(mb671_devices));
}
arch_initcall(mb671_devices_init);

static void __iomem *mb671_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor
	 * core version register, which is guaranted to be available, and
	 * non-writable. */
	return (void __iomem *)CCN_PVR;
}

static void __init mb671_init_irq(void)
{
	epld_early_init(&mb671_epld_device);

#if defined(CONFIG_SH_ST_STEM)
	/* The off chip interrupts on the mb671 are a mess. The external
	 * EPLD priority encodes them, but because they pass through the ILC3
	 * there is no way to decode them.
	 *
	 * So here we bodge it as well. Only enable the STEM INTR0 signal,
	 * and hope nothing else goes active. This will result in
	 * SYS_ITRQ[3..0] = 0100.
	 *
	 * BTW. According to EPLD code author - "masking" interrupts
	 * means "enabling" them... Just to let you know... ;-)
	 */
	epld_write(0xff, EPLD_INTMASK0CLR);
	epld_write(0xff, EPLD_INTMASK1CLR);
	/* IntPriority(4) <= not STEM_notINTR0 */
	epld_write(1 << 4, EPLD_INTMASK0SET);
#endif
}

struct sh_machine_vector mv_mb671 __initmv = {
	.mv_name		= "mb671",
	.mv_setup		= mb671_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= mb671_init_irq,
	.mv_ioport_map		= mb671_ioport_map,
};
