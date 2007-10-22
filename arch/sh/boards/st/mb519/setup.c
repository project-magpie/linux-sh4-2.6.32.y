/*
 * arch/sh/boards/st/mb519/setup.c
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
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/phy.h>
#include <asm/irq-ilc.h>
#include <asm/io.h>

#define EPLD_BASE 0xa5000000
#define EPLD_ver		(EPLD_BASE + 0x000000)
#define EPLD_cpcbver		(EPLD_BASE + 0x020000)
#define EPLD_stem		(EPLD_BASE + 0x040000)
#define EPLD_driver		(EPLD_BASE + 0x060000)
#define EPLD_reset		(EPLD_BASE + 0x080000)
#define EPLD_IntStat0		(EPLD_BASE + 0x0A0000)
#define EPLD_IntStat1		(EPLD_BASE + 0x0C0000)
#define EPLD_IntMask0		(EPLD_BASE + 0x0E0000)
#define EPLD_IntMask0Set	(EPLD_BASE + 0x100000)
#define EPLD_IntMask0Clear	(EPLD_BASE + 0x120000)
#define EPLD_IntMask1		(EPLD_BASE + 0x140000)
#define EPLD_IntMask1Set	(EPLD_BASE + 0x160000)
#define EPLD_IntMask1Clear	(EPLD_BASE + 0x180000)
#define EPLD_LedStdAddr		(EPLD_BASE + 0x1A0000)

#define EPLD_Flash		(EPLD_BASE + 0x400000)
#define EPLD_Stem		(EPLD_BASE + 0x500000)
#define EPLD_StemSet		(EPLD_BASE + 0x600000)
#define EPLD_StemClr		(EPLD_BASE + 0x700000)
#define EPLD_DACSPMux		(EPLD_BASE + 0xD00000)

static int ascs[2] __initdata = { 2, 3 };

void __init mb519_setup(char** cmdline_p)
{
	unsigned short epld_rev = ctrl_inw(EPLD_ver);
	unsigned short pcb_rev = ctrl_inw(EPLD_cpcbver);

	printk("STMicroelectronics STx7200 Mboard initialisation\n");
	printk("mb519 PCB rev %X EPLD rev %dr%d\n",
	       pcb_rev,
	       epld_rev >> 4, epld_rev & 0xf);

	stx7200_early_device_init();
	stx7200_configure_asc(ascs, 2, 0);
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT1,
};

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

static void mtd_set_vpp(struct map_info *map, int vpp)
{
	/* Bit 0: VPP enable
	 * Bit 1: Reset (not used in later EPLD versions)
	 */

	if (vpp) {
		ctrl_outw(3, EPLD_Flash);
	} else {
		ctrl_outw(2, EPLD_Flash);
	}
}

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.set_vpp	= mtd_set_vpp,
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

static struct plat_stmmacphy_data phy_private_data[2] = {
{
	/* MAC0: STE101P */
	.bus_id = 0,
	.phy_addr = 0,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
}, {
	/* MAC1: SMSC LAN 8700 */
	.bus_id = 1,
	.phy_addr = 1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
} };

static struct platform_device mb519_phy_devices[2] = {
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
			 * but because the mb519 uses the MII0_MDINT line
			 * as MODE4, and the STE101P MDINT pin is O/C,
			 * there may or maynot be a pull-up resistor
			 * depending on switch SW1-4. Most of the time there
			 * isn't, so disable the interrupt.
			 */
			.start	= -1,
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data[0],
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
	.dev = {
		.platform_data = &phy_private_data[1],
	 }
} };

static struct platform_device *mb519_devices[] __initdata = {
	&physmap_flash,
	&mb519_phy_devices[0],
	&mb519_phy_devices[1],
};

static int __init device_init(void)
{
	stx7200_configure_pwm(&pwm_private_info);
	stx7200_configure_ssc(&ssc_private_info);
	stx7200_configure_usb();
	stx7200_configure_ethernet(0, 0, 1, 0);
	// stx7200_configure_ethernet(1, 0, 1, 1);

	return platform_add_devices(mb519_devices, ARRAY_SIZE(mb519_devices));
}
arch_initcall(device_init);

static void __iomem *stx7200mboard_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init stx7200mboard_init_irq(void)
{
	/* The off chip interrupts on the mb519 are a mess. The external
	 * EPLD priority encodes them, but because they pass through the ILC3
	 * there is no way to decode them.
	 *
	 * So here we bodge it as well. Only enable the STEM INTR0 signal,
	 * and hope nothing else goes active.
	 *
	 * Note that this changed between EPLD rev 1r2 and 1r3. This is correct
	 * for 1r3 which should be the most common now.
	 */
	ctrl_outw(1<<4, EPLD_IntMask0Set); /* IntPriority(4) <= not STEM_notINTR0 */
}

struct sh_machine_vector mv_stx7200mboard __initmv = {
	.mv_name		= "mb519",
	.mv_setup		= mb519_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= stx7200mboard_init_irq,
	.mv_ioport_map		= stx7200mboard_ioport_map,
#ifdef CONFIG_HEARTBEAT
	.mv_heartbeat		= heartbeat_heart,
#endif
};
